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
UXPLAY_PATH = os.path.join(BASE_DIR, "bin", "uxplay.exe")

# Define AppData paths
APPDATA_PATH = os.path.join(os.environ.get('APPDATA'), APP_NAME)
ARGUMENTS_FILE_PATH = os.path.join(APPDATA_PATH, "arguments.txt")

# Define Bonjour Service Name
BONJOUR_SERVICE_NAME = "Bonjour Service" # Standard service name for Apple's Bonjour

# Load the tray icon image
try:
    IMAGE = PIL.Image.open(TRAY_ICON_PATH)
except FileNotFoundError:
    print(f"Error: Tray icon not found at {TRAY_ICON_PATH}. Ensure 'icon.ico' exists.")
    IMAGE = None # Fallback or provide a default placeholder image if possible

process = None
SCRIPT_FOR_AUTOSTART = f'"{sys.executable}"'
RUN_KEY = r"Software\Microsoft\Windows\CurrentVersion\Run"

def ensure_appdata_structure():
    print(f"Ensuring application data directory: {APPDATA_PATH}")
    os.makedirs(APPDATA_PATH, exist_ok=True)
    if not os.path.exists(ARGUMENTS_FILE_PATH):
        print(f"Creating default arguments file: {ARGUMENTS_FILE_PATH}")
        with open(ARGUMENTS_FILE_PATH, 'w') as f:
            f.write('')
        print("Default arguments.txt created.")
    else:
        print("arguments.txt already exists.")

def get_user_arguments():
    if not os.path.exists(ARGUMENTS_FILE_PATH):
        print(f"Warning: {ARGUMENTS_FILE_PATH} not found. No custom arguments will be used.")
        return []
    try:
        with open(ARGUMENTS_FILE_PATH, 'r') as f:
            args_str = f.read().strip()
            if not args_str:
                return []
            return args_str.split()
    except Exception as e:
        print(f"Error reading arguments from {ARGUMENTS_FILE_PATH}: {e}")
        return []

def is_service_running(service_name):
    try:
        result = subprocess.run(
            ['sc', 'query', service_name],
            capture_output=True,
            text=True,
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        if result.returncode != 0:
            if f"The specified service does not exist" in result.stderr:
                print(f"Service '{service_name}' does not exist.")
            else:
                print(f"Error querying service '{service_name}': {result.stderr.strip()}")
            return False
        return "STATE        : 4  RUNNING" in result.stdout
    except FileNotFoundError:
        print("Error: 'sc' command not found. Ensure it's in your PATH.")
        return False
    except Exception as e:
        print(f"An unexpected error occurred while checking service '{service_name}': {e}")
        return False

def start_service(service_name):
    if is_service_running(service_name):
        print(f"Service '{service_name}' is already running.")
        return True
    try:
        print(f"Starting service '{service_name}'...")
        result = subprocess.run(
            ['sc', 'start', service_name],
            capture_output=True,
            text=True,
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        if result.returncode != 0:
            print(f"Failed to start service '{service_name}': {result.stderr.strip()}")
            print(f"Stdout: {result.stdout.strip()}")
            return False

        # Give it a moment to start and then verify
        # Polling for status is more robust than a fixed sleep
        max_attempts = 10
        for i in range(max_attempts):
            time.sleep(1) # Wait 1 second
            if is_service_running(service_name):
                print(f"Service '{service_name}' started successfully.")
                return True
            print(f"Waiting for '{service_name}' to start... ({i+1}/{max_attempts})")
        print(f"Service '{service_name}' did not start in time.")
        return False
    except FileNotFoundError:
        print("Error: 'sc' command not found. Ensure it's in your PATH.")
        return False
    except Exception as e:
        print(f"An unexpected error occurred while starting service '{service_name}': {e}")
        return False

def stop_service(service_name):
    if not is_service_running(service_name):
        print(f"Service '{service_name}' is not running.")
        return True
    try:
        print(f"Stopping service '{service_name}'...")
        result = subprocess.run(
            ['sc', 'stop', service_name],
            capture_output=True,
            text=True,
            creationflags=subprocess.CREATE_NO_WINDOW
        )
        if result.returncode != 0:
            print(f"Failed to stop service '{service_name}': {result.stderr.strip()}")
            print(f"Stdout: {result.stdout.strip()}")
            return False

        # Give it a moment to stop and then verify
        # Polling for status is more robust than a fixed sleep
        max_attempts = 10
        for i in range(max_attempts):
            time.sleep(1) # Wait 1 second
            if not is_service_running(service_name): # Check if it's NOT running
                print(f"Service '{service_name}' stopped successfully.")
                return True
            print(f"Waiting for '{service_name}' to stop... ({i+1}/{max_attempts})")
        print(f"Service '{service_name}' did not stop in time.")
        return False
    except FileNotFoundError:
        print("Error: 'sc' command not found. Ensure it's in your PATH.")
        return False
    except Exception as e:
        print(f"An unexpected error occurred while stopping service '{service_name}': {e}")
        return False

def start_server():
    global process
    if process and process.poll() is None:
        print("UxPlay server is already running.")
        return

    # 1. Start Bonjour Service
    print("Attempting to start Bonjour Service...")
    if not start_service(BONJOUR_SERVICE_NAME):
        print("Warning: Could not start Bonjour Service. UxPlay might not be discoverable.")
        # Proceed with UxPlay but log warning; user can still try to connect manually by IP.

    # 2. Start UxPlay
    print("Attempting to start UxPlay server...")
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
        stop_service(BONJOUR_SERVICE_NAME) # Clean up Bonjour if UxPlay failed to launch
    except Exception as e:
        print(f"An error occurred while starting UxPlay: {e}")
        stop_service(BONJOUR_SERVICE_NAME) # Clean up Bonjour if UxPlay failed to launch

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

    # Stop Bonjour Service after UxPlay
    print("Attempting to stop Bonjour Service...")
    stop_service(BONJOUR_SERVICE_NAME)

def restart_server(icon, item):
    print("Restarting UxPlay server and Bonjour Service...")
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
        ensure_appdata_structure()
    try:
        os.startfile(ARGUMENTS_FILE_PATH)
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
            pystray.MenuItem("Edit UxPlay Arguments", open_arguments_file),
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
