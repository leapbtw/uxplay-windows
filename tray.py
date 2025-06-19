import os
import sys
import logging
import shlex
import subprocess
import threading
import time
import winreg
import webbrowser

from pathlib import Path
from typing import List, Optional

import pystray
from PIL import Image

# ─── Global Constants & Logging Setup ─────────────────────────────────────────

APP_NAME = "uxplay-windows"
# Use %APPDATA%\windows-uxplay for all data & logs
APPDATA_DIR = Path(os.environ["APPDATA"]) / "windows-uxplay"
LOG_FILE = APPDATA_DIR / f"{APP_NAME}.log"

# Ensure AppData dir exists before we try to log there
APPDATA_DIR.mkdir(parents=True, exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler(str(LOG_FILE), encoding="utf-8"),
        logging.StreamHandler(sys.stdout),
    ],
)

# ─── Path Management ───────────────────────────────────────────────────────────

class Paths:
    def __init__(self):
        # Detect base path (PyInstaller vs. source)
        if getattr(sys, "frozen", False):
            # When frozen, sys.executable is the .exe
            self.base_dir = Path(getattr(sys, "_MEIPASS", "") or 
                                 sys.executable).parent
        else:
            self.base_dir = Path(__file__).resolve().parent

        self.appdata_dir: Path = APPDATA_DIR
        self.arguments_file: Path = self.appdata_dir / "arguments.txt"
        self.icon_file: Path = self.base_dir / "icon.ico"
        self.uxplay_exe: Path = self.base_dir / "bin" / "uxplay.exe"

# ─── Argument File Management ─────────────────────────────────────────────────

class ArgumentManager:
    def __init__(self, file_path: Path):
        self.file_path = file_path

    def ensure_exists(self) -> None:
        logging.info("Ensuring arguments file at '%s'", self.file_path)
        self.file_path.parent.mkdir(parents=True, exist_ok=True)
        if not self.file_path.exists():
            self.file_path.write_text("")
            logging.info("Created empty arguments.txt")

    def read_args(self) -> List[str]:
        if not self.file_path.exists():
            logging.warning("arguments.txt missing, no custom args")
            return []
        raw = self.file_path.read_text(encoding="utf-8").strip()
        if not raw:
            return []
        try:
            return shlex.split(raw)
        except ValueError as e:
            logging.error("Failed to parse arguments: %s", e)
            return []

# ─── Server Process Management ────────────────────────────────────────────────

class ServerManager:
    def __init__(self, exe_path: Path, arg_mgr: ArgumentManager):
        self.exe_path = exe_path
        self.arg_mgr = arg_mgr
        self.process: Optional[subprocess.Popen] = None

    def start(self) -> None:
        if self.process and self.process.poll() is None:
            logging.info("Server already running (PID %s)", self.process.pid)
            return

        if not self.exe_path.exists():
            logging.error("uxplay.exe not found at %s", self.exe_path)
            return

        cmd = [str(self.exe_path)] + self.arg_mgr.read_args()
        logging.info("Starting UxPlay with: %s", cmd)
        try:
            self.process = subprocess.Popen(
                cmd,
                creationflags=subprocess.CREATE_NO_WINDOW
            )
            logging.info("Started (PID %s)", self.process.pid)
        except Exception:
            logging.exception("Failed to launch UxPlay")

    def stop(self) -> None:
        if not (self.process and self.process.poll() is None):
            logging.info("Server not running.")
            return
        pid = self.process.pid
        logging.info("Stopping UxPlay (PID %s)...", pid)
        try:
            self.process.terminate()
            self.process.wait(timeout=3)
            logging.info("Stopped cleanly.")
        except subprocess.TimeoutExpired:
            logging.warning("Did not stop in time, killing")
            self.process.kill()
            self.process.wait()
        except Exception:
            logging.exception("Error stopping UxPlay")
        finally:
            self.process = None

# ─── Autostart via Windows Registry ──────────────────────────────────────────

class AutoStartManager:
    RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"

    def __init__(self, app_name: str, exe_script: str):
        self.app_name = app_name
        self.exe_script = exe_script

    def is_enabled(self) -> bool:
        try:
            with winreg.OpenKey(
                winreg.HKEY_CURRENT_USER,
                self.RUN_KEY,
                0,
                winreg.KEY_READ
            ) as key:
                val, _ = winreg.QueryValueEx(key, self.app_name)
                return self.exe_script in val
        except FileNotFoundError:
            return False
        except Exception:
            logging.exception("Failed to read autostart setting")
            return False

    def enable(self) -> None:
        try:
            with winreg.OpenKey(
                winreg.HKEY_CURRENT_USER,
                self.RUN_KEY,
                0,
                winreg.KEY_SET_VALUE
            ) as key:
                winreg.SetValueEx(
                    key,
                    self.app_name,
                    0,
                    winreg.REG_SZ,
                    self.exe_script
                )
            logging.info("Autostart enabled")
        except Exception:
            logging.exception("Failed to enable autostart")

    def disable(self) -> None:
        try:
            with winreg.OpenKey(
                winreg.HKEY_CURRENT_USER,
                self.RUN_KEY,
                0,
                winreg.KEY_SET_VALUE
            ) as key:
                winreg.DeleteValue(key, self.app_name)
            logging.info("Autostart disabled")
        except FileNotFoundError:
            logging.info("Autostart entry not present")
        except Exception:
            logging.exception("Failed to disable autostart")

    def toggle(self) -> None:
        if self.is_enabled():
            self.disable()
        else:
            self.enable()

# ─── System Tray UI ───────────────────────────────────────────────────────────

class TrayIcon:
    def __init__(
        self,
        icon_path: Path,
        server_mgr: ServerManager,
        arg_mgr: ArgumentManager,
        auto_mgr: AutoStartManager
    ):
        self.server_mgr = server_mgr
        self.arg_mgr = arg_mgr
        self.auto_mgr = auto_mgr

        menu = pystray.Menu(
            pystray.MenuItem("Start UxPlay", lambda _: server_mgr.start()),
            pystray.MenuItem("Stop UxPlay", lambda _: server_mgr.stop()),
            pystray.MenuItem("Restart UxPlay", lambda _: self._restart()),
            pystray.MenuItem(
                "Autostart with Windows",
                lambda _: auto_mgr.toggle(),
                checked=lambda _: auto_mgr.is_enabled()
            ),
            pystray.MenuItem(
                "Edit UxPlay Arguments", lambda _: self._open_args()
            ),
            pystray.MenuItem(
                "License",
                lambda _: webbrowser.open(
                    "https://github.com/leapbtw/uxplay-windows/"
                    "blob/main/LICENSE.md"
                )
            ),
            pystray.MenuItem("Exit", lambda _: self._exit())
        )

        self.icon = pystray.Icon(
            name=APP_NAME,
            icon=Image.open(icon_path),
            title=APP_NAME,
            menu=menu
        )

    def _restart(self) -> None:
        logging.info("Restarting UxPlay")
        self.server_mgr.stop()
        self.server_mgr.start()

    def _open_args(self) -> None:
        self.arg_mgr.ensure_exists()
        try:
            os.startfile(str(self.arg_mgr.file_path))
            logging.info("Opened arguments.txt in default editor")
        except Exception:
            logging.exception("Failed to open arguments file")

    def _exit(self) -> None:
        logging.info("Exiting application")
        self.server_mgr.stop()
        self.icon.stop()

    def run(self) -> None:
        self.icon.run()

# ─── Application Orchestration ───────────────────────────────────────────────

class Application:
    def __init__(self):
        self.paths = Paths()
        self.arg_mgr = ArgumentManager(self.paths.arguments_file)

        # Build the registry value string
        script_path = Path(__file__).resolve()
        if getattr(sys, "frozen", False):
            exe_script = f'"{sys.executable}"'
        else:
            exe_script = f'"{sys.executable}" "{script_path}"'

        self.auto_mgr = AutoStartManager(APP_NAME, exe_script)
        self.server_mgr = ServerManager(self.paths.uxplay_exe, self.arg_mgr)
        self.tray = TrayIcon(
            self.paths.icon_file,
            self.server_mgr,
            self.arg_mgr,
            self.auto_mgr
        )

    def run(self) -> None:
        self.arg_mgr.ensure_exists()

        # Delay server start so the tray icon appears promptly
        threading.Thread(target=self._delayed_start, daemon=True).start()

        logging.info("Starting system tray icon")
        self.tray.run()
        logging.info("Application terminated")

    def _delayed_start(self) -> None:
        time.sleep(3)
        self.server_mgr.start()

if __name__ == "__main__":
    Application().run()
