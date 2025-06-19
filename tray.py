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
# Ensure the icon file exists before trying to open it

process = None
APP_NAME = "uxplay-windows"
SCRIPT_FOR_AUTOSTART = f'"{sys.executable}"'
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"

def start_server():
    global process
    if process and process.poll() is None:
        return  # already running
    print("Starting UxPlay server...")
    try:
        process = subprocess.Popen(
            [UXPLAY_PATH],
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        print(f"UxPlay server started with PID: {process.pid}")
    except FileNotFoundError:
        print(f"Error: uxplay.exe not found at {UXPLAY_PATH}. Please ensure it exists.")
    except Exception as e:
        print(f"An error occurred while starting UxPlay: {e}")


def stop_server():
    global process
    if process and process.poll() is None:
        print("Stopping UxPlay server...")
        try:
            process.terminate()
            process.wait(timeout=3)
            print("UxPlay server stopped.")
        except subprocess.TimeoutExpired:
            print("UxPlay server did not terminate gracefully, killing it.")
            process.kill()
            process.wait()
        except Exception as e:
            print(f"An error occurred while stopping UxPlay: {e}")
        finally:
            process = None
    elif process is None:
        print("UxPlay server is not running.")

def restart_server(icon, item):
    print("Restarting UxPlay server...")
    stop_server()
    start_server()

def is_autostart_enabled():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_READ) as key:
            value, _ = winreg.QueryValueEx(key, APP_NAME)
            return SCRIPT_FOR_AUTOSTART in value
    except FileNotFoundError:
        return False
    except Exception as e:
        print(f"Error checking autostart: {e}")
        return False

def enable_autostart():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
            winreg.SetValueEx(key, APP_NAME, 0, winreg.REG_SZ, SCRIPT_FOR_AUTOSTART)
        print(f"Autostart enabled for {APP_NAME} with value: {SCRIPT_FOR_AUTOSTART}")
    except Exception as e:
        print(f"Error enabling autostart: {e}")


def disable_autostart():
    try:
        with winreg.OpenKey(winreg.HKEY_CURRENT_USER, RUN_KEY, 0, winreg.KEY_SET_VALUE) as key:
            winreg.DeleteValue(key, APP_NAME)
        print(f"Autostart disabled for {APP_NAME}.")
    except FileNotFoundError:
        print("Autostart entry not found, nothing to disable.")
    except Exception as e:
        print(f"Error disabling autostart: {e}")

def toggle_autostart(icon, item):
    if is_autostart_enabled():
        disable_autostart()
    else:
        enable_autostart()

def show_license(icon, item):
    webbrowser.open('https://github.com/leapbtw/uxplay-windows/blob/main/LICENSE.md')

def exit_tray_icon(icon, item):
    print("Exiting application...")
    stop_server()
    icon.stop()
    print("Application exited.")


def main():
    tray_icon = pystray.Icon(
        "Icon",
        IMAGE,
        title="uxplay-windows\nRight-click to show options",
        menu=pystray.Menu(
            pystray.MenuItem("Start UxPlay", start_server),
            pystray.MenuItem("Stop UxPlay", stop_server),
            pystray.MenuItem("Restart UxPlay", restart_server),
            pystray.MenuItem(
                "Autostart with Windows",
                toggle_autostart,
                checked=lambda item: is_autostart_enabled()
            ),
            pystray.MenuItem("License", show_license),
            pystray.MenuItem("Exit", exit_tray_icon)
        )
    )

    def delayed_start():
        time.sleep(3)
        start_server()

    threading.Thread(target=delayed_start, daemon=True).start()

    print("Starting tray icon...")
    tray_icon.run()
    print("Tray icon stopped.")

if __name__ == "__main__":
    main()
