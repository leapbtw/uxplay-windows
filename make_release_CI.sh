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
  --noconfirm

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

COPIED_LIST_FILE="$DIST_DIR/copied-dll-paths.txt"
rm -f "$COPIED_LIST_FILE"
touch "$COPIED_LIST_FILE"

copy_dll_flat() {
  # Usage: copy_dll_flat "/path/to/src.dll"
  local src="$1"
  local dest="$DIST_DIR/$(basename "$src")"

  # mimic cp -n behavior: don't overwrite existing
  if [ -e "$dest" ]; then
    return 0
  fi

  cp -n "$src" "$dest"
  echo "COPIED: $src -> $dest"
  echo "$src -> $dest" >> "$COPIED_LIST_FILE"
}

echo "Copying all DLLs from UCRT64/bin... (excluding Qt)"
EXCLUDE_REGEX='^(Qt6|Qt5|qtpcre2|QtSql|QtNetwork|QtCore|QtGui|QtWidgets|QtMultimedia|QtPositioning|QtWebEngine|QtSvg).+\.dll$'

while IFS= read -r dll_path; do
  [ -z "$dll_path" ] && continue
  copy_dll_flat "$dll_path"
done < <(
  find "$UCRT64_PREFIX/bin" -maxdepth 1 -type f -name '*.dll' \
    | grep -Ev "$EXCLUDE_REGEX" || true
)

echo "Copying all DLLs from UCRT64/lib... (excluding Qt)"
while IFS= read -r dll_path; do
  [ -z "$dll_path" ] && continue
  # keep original intent: exclude gstreamer-1.0 dlls under lib
  if [[ "$dll_path" == "$UCRT64_PREFIX/lib/gstreamer-1.0/"* ]]; then
    continue
  fi
  copy_dll_flat "$dll_path"
done < <(
  find "$UCRT64_PREFIX/lib" -type f -name '*.dll' \
    ! -path "$UCRT64_PREFIX/lib/gstreamer-1.0/*" \
    | grep -Ev "$EXCLUDE_REGEX" || true
)

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
echo " Lista DLL copiate (src -> dest) salvata in:"
echo " $COPIED_LIST_FILE"
echo "================================================="

echo "================================================="
echo " ✅ Done! Package is ready in $DIST_DIR"
echo "================================================="

sleep 1
echo ""