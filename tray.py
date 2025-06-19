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

# Define application-specific paths
APP_NAME = "uxplay-windows"
TRAY_ICON_PATH = os.path.join(BASE_DIR, "icon.ico")
IMAGE = PIL.Image.open(TRAY_ICON_PATH)
UXPLAY_PATH = os.path.join(BASE_DIR, "bin", "uxplay.exe")

# Define AppData paths
APPDATA_PATH = os.path.join(os.environ.get('APPDATA'), APP_NAME)
ARGUMENTS_FILE_PATH = os.path.join(APPDATA_PATH, "arguments.txt")

process = None
SCRIPT_FOR_AUTOSTART = f'"{sys.executable}"'
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"


def ensure_appdata_structure():
    print(f"Ensuring application data directory: {APPDATA_PATH}")
    os.makedirs(APPDATA_PATH, exist_ok=True)
    if not os.path.exists(ARGUMENTS_FILE_PATH):
        print(f"Creating default arguments file: {ARGUMENTS_FILE_PATH}")
        with open(ARGUMENTS_FILE_PATH, 'w') as f:
            f.write('')  # Create an empty file, users can add arguments later
        print("Default arguments.txt created.")
    else:
        print("arguments.txt already exists.")


def get_user_arguments():
    if not os.path.exists(ARGUMENTS_FILE_PATH):
        print(f"Warning: {ARGUMENTS_FILE_PATH} not found. No custom arguments will be used.")
        return []
    try:
        with open(ARGUMENTS_FILE_PATH, 'r') as f:
            # Read all lines, split by whitespace, and filter out empty strings
            args_str = f.read().strip()
            if not args_str:
                return []
            # Split by any whitespace and handle potential multiple spaces
            # Use shlex.split for more robust parsing if arguments contain spaces/quotes
            # For simplicity, we'll just split by whitespace for now.
            return args_str.split()
    except Exception as e:
        print(f"Error reading arguments from {ARGUMENTS_FILE_PATH}: {e}")
        return []


def start_server():
    global process
    if process and process.poll() is None:
        print("UxPlay server is already running.")
        return  # already running

    print("Attempting to start UxPlay server...")

    # Get user-defined arguments
    user_args = get_user_arguments()
    command = [UXPLAY_PATH] + user_args
    print(f"Launching command: {' '.join(command)}")

    try:
        process = subprocess.Popen(
            command,
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


def open_arguments_file(icon, item):
    if not os.path.exists(ARGUMENTS_FILE_PATH):
        ensure_appdata_structure()  # Ensure it exists before trying to open
    try:
        os.startfile(ARGUMENTS_FILE_PATH)  # Windows-specific way to open file with default app
        print(f"Opened arguments file: {ARGUMENTS_FILE_PATH}")
    except Exception as e:
        print(f"Error opening arguments file: {e}")
        print("You can manually navigate to:")
        print(APPDATA_PATH)


def show_license(icon, item):
    webbrowser.open('https://github.com/leapbtw/uxplay-windows/blob/main/LICENSE.md')


def exit_tray_icon(icon, item):
    print("Exiting application...")
    stop_server()
    icon.stop()
    print("Application exited.")


def main():
    # Ensure AppData structure exists before anything else
    ensure_appdata_structure()

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
            pystray.MenuItem("Edit UxPlay Arguments", open_arguments_file),  # New menu item
            pystray.MenuItem("License", show_license),
            pystray.MenuItem("Exit", exit_tray_icon)
        )
    )

    def delayed_start():
        time.sleep(3)
        start_server()

    # Start the server after a short delay
    threading.Thread(target=delayed_start, daemon=True).start()

    print("Starting tray icon...")
    tray_icon.run()
    print("Tray icon stopped.")


if __name__ == "__main__":
    main()
