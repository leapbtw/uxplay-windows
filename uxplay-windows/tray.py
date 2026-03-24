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

APP_NAME = "uxplay-windows"
APPDATA_DIR = Path(os.environ["APPDATA"]) / "uxplay-windows"
LOG_FILE = APPDATA_DIR / f"{APP_NAME}.log"

APPDATA_DIR.mkdir(parents=True, exist_ok=True)

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    handlers=[
        logging.FileHandler(str(LOG_FILE), encoding="utf-8"),
        logging.StreamHandler(sys.stdout),
    ],
)

class Paths:
    def __init__(self):
        if getattr(sys, "frozen", False):
            if hasattr(sys, "_MEIPASS"):
                cand = Path(sys._MEIPASS)
            else:
                cand = Path(sys.executable).parent
        else:
            cand = Path(__file__).resolve().parent

        internal = cand / "_internal"
        self.resource_dir = internal if internal.is_dir() else cand
        self.icon_file = self.resource_dir / "icon.ico"
        ux1 = self.resource_dir / "bin" / "uxplay.exe"
        ux2 = self.resource_dir / "uxplay.exe"
        self.uxplay_exe = ux1 if ux1.exists() else ux2
        self.appdata_dir = APPDATA_DIR
        self.arguments_file = self.appdata_dir / "arguments.txt"

class ArgumentManager:
    def __init__(self, file_path: Path):
        self.file_path = file_path

    def ensure_exists(self) -> None:
        self.file_path.parent.mkdir(parents=True, exist_ok=True)
        if not self.file_path.exists():
            self.file_path.write_text("", encoding="utf-8")

    def read_args(self) -> List[str]:
        if not self.file_path.exists():
            return []
        text = self.file_path.read_text(encoding="utf-8").strip()
        if not text:
            return []
        try:
            raw_args = shlex.split(text)
            return [a for a in raw_args if a.startswith(('-', '/'))]
        except ValueError as e:
            logging.error("Parse error: %s", e)
            return []

class ServerManager:
    def __init__(self, exe_path: Path, arg_mgr: ArgumentManager):
        self.exe_path = exe_path
        self.arg_mgr = arg_mgr
        self.process: Optional[subprocess.Popen] = None

    def start(self) -> None:
        if self.process and self.process.poll() is None:
            return
        if not self.exe_path.exists():
            return
        cmd = [str(self.exe_path)] + self.arg_mgr.read_args()
        try:
            self.process = subprocess.Popen(
                cmd,
                creationflags=subprocess.CREATE_NO_WINDOW,
                close_fds=True
            )
        except Exception:
            logging.exception("Launch failed")

    def stop(self) -> None:
        if not (self.process and self.process.poll() is None):
            return
        try:
            self.process.terminate()
            try:
                self.process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self.process.kill()
                self.process.wait()
        except Exception:
            logging.exception("Stop error")
        finally:
            self.process = None

class AutoStartManager:
    RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"
    def __init__(self, app_name: str, exe_cmd: str):
        self.app_name = app_name
        self.exe_cmd = exe_cmd
    def is_enabled(self) -> bool:
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, self.RUN_KEY, 0, winreg.KEY_READ) as key:
                val, _ = winreg.QueryValueEx(key, self.app_name)
                return self.exe_cmd in val
        except FileNotFoundError:
            return False
        except Exception:
            return False
    def enable(self) -> None:
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, self.RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
                winreg.SetValueEx(key, self.app_name, 0, winreg.REG_SZ, self.exe_cmd)
        except Exception:
            logging.exception("Enable failed")
    def disable(self) -> None:
        try:
            with winreg.OpenKey(winreg.HKEY_CURRENT_USER, self.RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
                winreg.DeleteValue(key, self.app_name)
        except FileNotFoundError:
            pass
        except Exception:
            logging.exception("Disable failed")
    def toggle(self) -> None:
        if self.is_enabled(): self.disable()
        else: self.enable()

class TrayIcon:
    def __init__(self, icon_path: Path, server_mgr: ServerManager, arg_mgr: ArgumentManager, auto_mgr: AutoStartManager):
        self.server_mgr = server_mgr
        self.arg_mgr = arg_mgr
        self.auto_mgr = auto_mgr
        menu = pystray.Menu(
            pystray.MenuItem("Start UxPlay", lambda _: server_mgr.start()),
            pystray.MenuItem("Stop UxPlay",  lambda _: server_mgr.stop()),
            pystray.MenuItem("Restart UxPlay", lambda _: self._restart()),
            pystray.MenuItem("Autostart with Windows", lambda _: auto_mgr.toggle(), checked=lambda _: auto_mgr.is_enabled()),
            pystray.MenuItem("Edit UxPlay Arguments", lambda _: self._open_args()),
            pystray.MenuItem("License", lambda _: webbrowser.open("https://github.com/leapbtw/uxplay-windows/blob/main/LICENSE.md")),
            pystray.MenuItem("Exit", lambda _: self._exit())
        )
        self.icon = pystray.Icon(name=APP_NAME, icon=Image.open(icon_path), title=APP_NAME, menu=menu)

    def _restart(self):
        self.server_mgr.stop()
        self.server_mgr.start()

    def _open_args(self):
        self.arg_mgr.ensure_exists()
        try:
            os.startfile(str(self.arg_mgr.file_path))
        except Exception:
            logging.exception("Open failed")

    def _exit(self):
        self.server_mgr.stop()
        self.icon.stop()

    def run(self):
        self.icon.run()

class Application:
    def __init__(self):
        self.paths = Paths()
        self.arg_mgr = ArgumentManager(self.paths.arguments_file)
        script = Path(__file__).resolve()
        if getattr(sys, "frozen", False):
            exe_cmd = f'"{sys.executable}"'
        else:
            exe_cmd = f'"{sys.executable}" "{script}"'
        self.auto_mgr = AutoStartManager(APP_NAME, exe_cmd)
        self.server_mgr = ServerManager(self.paths.uxplay_exe, self.arg_mgr)
        self.tray = TrayIcon(self.paths.icon_file, self.server_mgr, self.arg_mgr, self.auto_mgr)

    def run(self):
        self.arg_mgr.ensure_exists()
        threading.Thread(target=self._delayed_start, daemon=True).start()
        self.tray.run()

    def _delayed_start(self):
        time.sleep(3)
        self.server_mgr.start()

if __name__ == "__main__":
    Application().run()