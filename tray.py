import pystray
import PIL.Image
import os
import subprocess
import sys
import winreg
import webbrowser
import threading
import time

# Detect base path correctly whether running from source or PyInstaller .exe
if getattr(sys, 'frozen', False):
    BASE_DIR = sys._MEIPASS if hasattr(sys, '_MEIPASS') else os.path.dirname(sys.executable)
else:
    BASE_DIR = os.path.dirname(os.path.abspath(__file__))

# Correct file paths
TRAY_ICON_PATH = os.path.join(BASE_DIR, "icon.ico")
UXPLAY_PATH = os.path.join(BASE_DIR, "bin", "uxplay.exe")
IMAGE = PIL.Image.open(TRAY_ICON_PATH)

process = None
APP_NAME = "uxplay-windows"
SCRIPT_PATH = f'"{sys.executable}"'
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"

def start_server():
    global process
    if process and process.poll() is None:
        return  # already running
    process = subprocess.Popen(
        [UXPLAY_PATH],
        creationflags=subprocess.CREATE_NO_WINDOW
    )

def stop_server():
    global process
    if process and process.poll() is None:
        process.terminate()
        process.wait()
        process = None

def restart_server(icon, item):
    stop_server()
    start_server()

def is_autostart_enabled():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_READ) as key:
            value, _ = winreg.QueryValueEx(key, APP_NAME)
            return SCRIPT_PATH in value
    except FileNotFoundError:
        return False

def enable_autostart():
    with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
        winreg.SetValueEx(key, APP_NAME, 0, winreg.REG_SZ, SCRIPT_PATH)

def disable_autostart():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
            winreg.DeleteValue(key, APP_NAME)
    except FileNotFoundError:
        pass

def toggle_autostart(icon, item):
    if is_autostart_enabled():
        disable_autostart()
    else:
        enable_autostart()

def show_license(icon, item):
    webbrowser.open('https://github.com/leapbtw/uxplay-windows/blob/main/LICENSE.md')

def exit_tray_icon(icon, item):
    stop_server()
    icon.stop()

tray_icon = pystray.Icon("Icon", IMAGE, title="uxplay-windows\nRight-click to show options", menu=pystray.Menu(
    pystray.MenuItem("Start UxPlay", start_server),
    pystray.MenuItem("Stop UxPlay", stop_server),
    pystray.MenuItem("Restart UxPlay", restart_server),
    pystray.MenuItem(
        "Autostart with Windows",
        toggle_autostart,
        checked=lambda item: is_autostart_enabled()
    ),
    pystray.MenuItem("Exit", exit_tray_icon),
    pystray.MenuItem("License", show_license)
))

# Start the server after 3-second delay in a non-blocking thread
def delayed_start():
    time.sleep(3)
    start_server()

threading.Thread(target=delayed_start, daemon=True).start()

tray_icon.run()
