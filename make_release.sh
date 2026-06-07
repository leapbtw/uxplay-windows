#!/bin/bash
set -e

EXE_NAME="uxplay-windows.exe"
BEACON_EXE="uxplay-bluetooth-beacon.exe"
DIST_DIR="$(pwd)/release"
BUILD_DIR="$(pwd)/build"
BEACON_DIR="$(pwd)/libuxplay/Bluetooth_LE_beacon"

PROJECT_ROOT="$(pwd)"

# Bonjour SDK (per arm64)
BONJOUR_SDK_HOME="$PROJECT_ROOT/Bonjour SDK"
BONJOUR_ARCH_DIR="arm64"

export BONJOUR_SDK_HOME
export BONJOUR_SDK="$BONJOUR_SDK_HOME"

if [ ! -f "$BONJOUR_SDK_HOME/Include/dns_sd.h" ]; then
  echo "ERROR: dns_sd.h not found at: $BONJOUR_SDK_HOME/Include/dns_sd.h" >&2
  exit 1
fi
if [ ! -f "$BONJOUR_SDK_HOME/Lib/$BONJOUR_ARCH_DIR/dnssd.lib" ]; then
  echo "ERROR: dnssd.lib not found at: $BONJOUR_SDK_HOME/Lib/$BONJOUR_ARCH_DIR/dnssd.lib" >&2
  exit 1
fi

echo "================================================="
echo " 1. Compiling C++ App (CMake + Ninja)"
echo "================================================="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release -G Ninja \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON $PROJECT_ROOT -DNO_MARCH_NATIVE=ON ..
ninja
cd ..

echo "================================================="
echo " 2. Compiling Python BLE Beacon (PyInstaller)"
echo "================================================="
cd "$BEACON_DIR"

#pyinstaller \
#  --onefile \
#  --name ${BEACON_EXE%.*} \
#  --hidden-import=winrt.windows.foundation \
#  --hidden-import=winrt.windows.foundation.collections \
#  --hidden-import=winrt.windows.devices.bluetooth.advertisement \
#  --hidden-import=winrt.windows.storage.streams \
#  --hidden-import=psutil \
#  --console \
#  uxplay-beacon-windows.py \
#  --noconfirm

cd ../..

echo "================================================="
echo " 3. Preparing Clean Dist Folder"
echo "================================================="
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/lib/gstreamer-1.0"
[ -f "docs/LICENSE.rtf" ] && cp "docs/LICENSE.rtf" "$DIST_DIR/"

cp "$BUILD_DIR/$EXE_NAME" "$DIST_DIR/"
cp "$BEACON_DIR/dist/$BEACON_EXE" "$DIST_DIR/"


echo "================================================="
echo " 6. Copying mDNSResponder.exe and dnssd.dll (arm64)"
echo "================================================="
cp "./Bonjour SDK/Bin/$BONJOUR_ARCH_DIR/dnssd.dll" release/.
cp "./Bonjour SDK/Bin/$BONJOUR_ARCH_DIR/mDNSResponder.exe" release/.

find $PROJECT_ROOT -name compile_commands.json
cp ./build/compile_commands.json release/.

echo "================================================="
echo " 4. Gathering MSYS2 & GStreamer Runtime DLLs"
echo "================================================="
echo "⚠️  ACTION REQUIRED: GStreamer loads plugins dynamically."
echo "To capture the correct DLLs, we need to inspect the running process."
echo ""
echo " -> I will start $EXE_NAME now."
echo " -> PLEASE CONNECT YOUR DEVICE AND START AN AIRPLAY STREAM."
echo " -> Once the video/audio is actively playing, come back and press ENTER."
echo "------------------------------------------------------------------------"

# Start the app in the background from the build folder
"$DIST_DIR/$EXE_NAME" &
APP_PID=$!

read -p "Press [ENTER] here once the stream is playing on screen..."

if ! tasklist | grep -iq "$EXE_NAME"; then
  echo "ERROR: $EXE_NAME crashed or is not running!"
  exit 1
fi

echo "Capturing Loaded Modules..."
DLL_LIST=$(powershell.exe -NoProfile -Command "
    Get-Process -Name '${EXE_NAME%.*}' |
    Select-Object -ExpandProperty Modules |
    Where-Object { \$_.FileName -like '*clangarm64*' } |
    Select-Object -ExpandProperty FileName
")

echo "Copying DLL dependencies (src -> dest)..."
while read -r win_path; do
  [ -z "$win_path" ] && continue

  # change windows path into unix path
  unix_path=$(cygpath -u "$win_path")

  case "$unix_path" in
    *.dll|*.DLL) ;;
    *)
      continue
      ;;
  esac

  if [[ "$unix_path" == *"/lib/gstreamer-1.0/"* ]]; then
    dest_dir="$DIST_DIR/lib/gstreamer-1.0"
    dest="$dest_dir/$(basename "$unix_path")"
    mkdir -p "$dest_dir"
  else
    dest="$DIST_DIR/$(basename "$unix_path")"
  fi

  # do not overwrite if already copied
  if [ -f "$dest" ]; then
    continue
  fi

  cp "$unix_path" "$dest"
  echo "COPIED: $unix_path -> $dest"
done <<< "$DLL_LIST"


echo "Closing app..."
# Cleanly kill the Qt App process
taskkill //PID $APP_PID //T //F > /dev/null 2>&1 || true


echo "================================================="
echo " 5. Finalizing Qt Dependencies (windeployqt)"
echo "================================================="
windeployqt --no-translations --no-compiler-runtime \
  --dir "$DIST_DIR" "$DIST_DIR/$EXE_NAME"

echo "================================================="
echo " 7. Copying more files over"
echo "================================================="
find $PROJECT_ROOT -name *.ico
mkdir -p release/resources
cp stuff/newicon.ico release/resources/icon.ico
cp stuff/uxplay_arguments_list.txt \
  release/resources/uxplay_arguments_list.txt

echo "================================================="
echo " ✅ Done! Package is ready in $DIST_DIR"
echo "================================================="

sleep 1
echo ""