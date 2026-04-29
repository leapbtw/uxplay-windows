#!/bin/bash
set -e # Exit immediately if a command fails

EXE_NAME="uxplay-windows.exe"
BEACON_EXE="uxplay-beacon.exe"
DIST_DIR="$(pwd)/release"
BUILD_DIR="$(pwd)/build"
BEACON_DIR="$(pwd)/libuxplay/Bluetooth_LE_beacon"

PROJECT_ROOT="$(pwd)"
BONJOUR_SDK_HOME="$PROJECT_ROOT/Bonjour SDK"

export BONJOUR_SDK_HOME
export BONJOUR_SDK="$BONJOUR_SDK_HOME"

if [ ! -f "$BONJOUR_SDK_HOME/Include/dns_sd.h" ]; then
  echo "ERROR: dns_sd.h not found at: $BONJOUR_SDK_HOME/Include/dns_sd.h" >&2
  exit 1
fi
if [ ! -f "$BONJOUR_SDK_HOME/Lib/x64/dnssd.lib" ]; then
  echo "ERROR: dnssd.lib not found at: $BONJOUR_SDK_HOME/Lib/x64/dnssd.lib" >&2
  exit 1
fi

echo "================================================="
echo " 1. Compiling C++ App (CMake + Ninja)"
echo "================================================="
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake -DCMAKE_BUILD_TYPE=Release -G Ninja ..
ninja
cd ..


echo "================================================="
echo " 2. Compiling Python BLE Beacon (PyInstaller)"
echo "================================================="
cd "$BEACON_DIR"
# Make sure to replace 'uxplay-beacon-windows.py' with the actual filename if different
pyinstaller \
  --onefile \
  --name ${BEACON_EXE%.*} \
  --hidden-import=winrt.windows.foundation \
  --hidden-import=winrt.windows.foundation.collections \
  --hidden-import=winrt.windows.devices.bluetooth.advertisement \
  --hidden-import=winrt.windows.storage.streams \
  --hidden-import=psutil \
  --console \
  uxplay-beacon-windows.py \
  --noconfirm # Overwrite existing build/dist folders without asking
cd ../..


echo "================================================="
echo " 3. Preparing Clean Dist Folder"
echo "================================================="
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/lib/gstreamer-1.0"
[ -f "LICENSE.md" ] && cp "LICENSE.md" "$DIST_DIR/"

# Copy executables
cp "$BUILD_DIR/$EXE_NAME" "$DIST_DIR/"
cp "$BEACON_DIR/dist/$BEACON_EXE" "$DIST_DIR/"


echo "================================================="
echo " 4. Gathering BIG runtime bundle for CI"
echo "================================================="

# Prefix UCRT64 dentro MSYS2
UCRT64_PREFIX="$(dirname "$(dirname "$(command -v gcc)")")"
GST_PLUGIN_DIR="$UCRT64_PREFIX/lib/gstreamer-1.0"

echo "UCRT64_PREFIX = $UCRT64_PREFIX"
echo "DIST_DIR      = $DIST_DIR"

echo "Copying all DLLs from UCRT64/bin..."
find "$UCRT64_PREFIX/bin" -maxdepth 1 -type f -name '*.dll' \
  -exec cp -n {} "$DIST_DIR/" \;

echo "Copying all DLLs from UCRT64/lib..."
find "$UCRT64_PREFIX/lib" -type f -name '*.dll' \
  ! -path "$UCRT64_PREFIX/lib/gstreamer-1.0/*" \
  -exec cp -n {} "$DIST_DIR/" \;

echo "Copying whole GStreamer plugin directory..."
if [ -d "$GST_PLUGIN_DIR" ]; then
  mkdir -p "$DIST_DIR/lib"
  rm -rf "$DIST_DIR/lib/gstreamer-1.0"
  cp -r "$GST_PLUGIN_DIR" "$DIST_DIR/lib/"
else
  echo "WARNING: GStreamer plugin dir not found: $GST_PLUGIN_DIR"
fi

echo "Big runtime bundle copied."


echo "================================================="
echo " 5. Finalizing Qt Dependencies (windeployqt)"
echo "================================================="
windeployqt --no-translations --no-compiler-runtime --dir "$DIST_DIR" "$DIST_DIR/$EXE_NAME"


echo "================================================="
echo " ✅ Done! Package is ready in $DIST_DIR"
echo "================================================="

sleep 1
echo ""
