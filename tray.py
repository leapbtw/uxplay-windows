import logging
import shlex
import subprocess
import threading
import time
import sys
import winreg
import webbrowser
from pathlib import Path
from typing import List, Optional

import pystray
from PIL import Image


# ─── Logging Configuration ─────────────────────────────────────────────────────

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler("uxplay-windows.log"),
        logging.StreamHandler(sys.stdout),
    ],
)


# ─── Path Management ───────────────────────────────────────────────────────────

class Paths:
    def __init__(self):
        # detect PyInstaller bundle or source
        if getattr(sys, "frozen", False):
            base = Path(sys._MEIPASS) if hasattr(sys, "_MEIPASS") \
                   else Path(sys.executable).parent
        else:
            base = Path(__file__).resolve().parent

        self.base_dir: Path = base
        self.appdata_dir: Path = Path.home() / "AppData" / "Roaming" / "uxplay-windows"
        self.arguments_file: Path = self.appdata_dir / "arguments.txt"
        self.icon_file: Path = self.base_dir / "icon.ico"
        self.uxplay_exe: Path = self.base_dir / "bin" / "uxplay.exe"


# ─── Argument File Management ─────────────────────────────────────────────────

class ArgumentManager:
    def __init__(self, args_path: Path):
        self.args_path = args_path

    def ensure_exists(self) -> None:
        logging.info("Ensuring AppData directory '%s'", self.args_path.parent)
        self.args_path.parent.mkdir(parents=True, exist_ok=True)
        if not self.args_path.exists():
            logging.info("Creating empty arguments file at '%s'", self.args_path)
            self.args_path.write_text("")

    def read_args(self) -> List[str]:
        if not self.args_path.exists():
            logging.warning("Arguments file not found, returning empty args")
            return []
        text = self.args_path.read_text().strip()
        if not text:
            return []
        try:
            return shlex.split(text)
        except ValueError as e:
            logging.error("Failed to parse arguments: %s", e)
            return []


# ─── Server Process Management ────────────────────────────────────────────────

class ServerManager:
    def __init__(self, exe_path: Path, arg_manager: ArgumentManager):
        self.exe_path = exe_path
        self.arg_manager = arg_manager
        self.process: Optional[subprocess.Popen] = None

    def start(self) -> None:
        if self.process and self.process.poll() is None:
            logging.info("Server already running (PID %s)", self.process.pid)
            return

        if not self.exe_path.exists():
            logging.error("Executable not found: %s", self.exe_path)
            return

        cmd = [str(self.exe_path)] + self.arg_manager.read_args()
        logging.info("Starting server with command: %s", cmd)
        try:
            self.process = subprocess.Popen(
                cmd, creationflags=subprocess.CREATE_NO_WINDOW
            )
            logging.info("Server started (PID %s)", self.process.pid)
        except Exception as e:
            logging.exception("Failed to start server: %s", e)

    def stop(self) -> None:
        if not self.process or self.process.poll() is not None:
            logging.info("Server is not running.")
            return

        logging.info("Stopping server (PID %s)...", self.process.pid)
        try:
            self.process.terminate()
            self.process.wait(timeout=3)
            logging.info("Server stopped cleanly.")
        except subprocess.TimeoutExpired:
            logging.warning("Termination timed out; killing process")
            self.process.kill()
            self.process.wait()
        except Exception as e:
            logging.exception("Error during server shutdown: %s", e)
        finally:
            self.process = None


# ─── Autostart (Registry) Management ─────────────────────────────────────────

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
                winreg.KEY_READ,
            ) as key:
                val, _ = winreg.QueryValueEx(key, self.app_name)
                return self.exe_script in val
        except FileNotFoundError:
            return False
        except Exception:
            logging.exception("Failed to check autostart")
            return False

    def enable(self) -> None:
        try:
            with winreg.OpenKey(
                winreg.HKEY_CURRENT_USER,
                self.RUN_KEY,
                0,
                winreg.KEY_SET_VALUE,
            ) as key:
                winreg.SetValueEx(
                    key,
                    self.app_name,
                    0,
                    winreg.REG_SZ,
                    self.exe_script,
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
                winreg.KEY_SET_VALUE,
            ) as key:
                winreg.DeleteValue(key, self.app_name)
            logging.info("Autostart disabled")
        except FileNotFoundError:
            logging.info("Autostart entry not found")
        except Exception:
            logging.exception("Failed to disable autostart")

    def toggle(self) -> None:
        if self.is_enabled():
            self.disable()
        else:
            self.enable()


# ─── Tray Icon UI ────────────────────────────────────────────────────────────

class TrayIcon:
    def __init__(
        self,
        icon_path: Path,
        server_mgr: ServerManager,
        arg_mgr: ArgumentManager,
        auto_mgr: AutoStartManager,
    ):
        self.icon = pystray.Icon(
            name="uxplay-windows",
            icon=Image.open(icon_path),
            title="uxplay-windows",
            menu=pystray.Menu(
                pystray.MenuItem("Start UxPlay", lambda _: server_mgr.start()),
                pystray.MenuItem("Stop UxPlay", lambda _: server_mgr.stop()),
                pystray.MenuItem("Restart UxPlay", lambda _: self._restart(server_mgr)),
                pystray.MenuItem(
                    "Autostart with Windows",
                    lambda _: auto_mgr.toggle(),
                    checked=lambda item: auto_mgr.is_enabled(),
                ),
                pystray.MenuItem(
                    "Edit UxPlay Arguments", lambda _: self._open_args(arg_mgr)
                ),
                pystray.MenuItem(
                    "License",
                    lambda _: webbrowser.open(
                        "https://github.com/leapbtw/uxplay-windows/blob/main/LICENSE.md"
                    ),
                ),
                pystray.MenuItem("Exit", lambda _: self._exit(server_mgr)),
            ),
        )

    def _restart(self, server_mgr: ServerManager):
        logging.info("Restarting server...")
        server_mgr.stop()
        server_mgr.start()

    def _open_args(self, arg_mgr: ArgumentManager):
        if not arg_mgr.args_path.exists():
            arg_mgr.ensure_exists()
        try:
            arg_mgr.args_path.open().close()  # ensure file
            os.startfile(str(arg_mgr.args_path))
            logging.info("Opened arguments file")
        except Exception:
            logging.exception("Failed to open arguments file")

    def _exit(self, server_mgr: ServerManager):
        logging.info("Exiting application...")
        server_mgr.stop()
        self.icon.stop()

    def run(self):
        self.icon.run()


# ─── Application Orchestration ──────────────────────────────────────────────

class Application:
    def __init__(self):
        self.paths = Paths()
        self.arg_mgr = ArgumentManager(self.paths.arguments_file)
        self.auto_mgr = AutoStartManager(
            app_name="uxplay-windows",
            exe_script=f'"{sys.executable}" "{self.paths.base_dir / Path(__file__).name}"',
        )
        self.server_mgr = ServerManager(self.paths.uxplay_exe, self.arg_mgr)
        self.tray = TrayIcon(
            icon_path=self.paths.icon_file,
            server_mgr=self.server_mgr,
            arg_mgr=self.arg_mgr,
            auto_mgr=self.auto_mgr,
        )

    def run(self):
        self.arg_mgr.ensure_exists()

        # delayed startup so tray shows quickly
        threading.Thread(target=self._delayed_start, daemon=True).start()

        logging.info("Launching tray icon")
        self.tray.run()
        logging.info("Tray exited, application stopping")

    def _delayed_start(self):
        time.sleep(3)
        self.server_mgr.start()


if __name__ == "__main__":
    Application().run()
