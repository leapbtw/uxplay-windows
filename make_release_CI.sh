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

cp "$BUILD_DIR/$EXE_NAME" "$DIST_DIR/"
cp "$BEACON_DIR/dist/$BEACON_EXE" "$DIST_DIR/"

echo "================================================="
echo " 4. Gathering BIG runtime bundle for CI"
echo "================================================="

UCRT64_PREFIX="$(dirname "$(dirname "$(command -v gcc)")")"
GST_PLUGIN_DIR="$UCRT64_PREFIX/lib/gstreamer-1.0"

COPIED_LIST_FILE="$DIST_DIR/copied-dll-paths.txt"
rm -f "$COPIED_LIST_FILE"
touch "$COPIED_LIST_FILE"

copy_dll_flat() {
  local src="$1"
  local dest="$DIST_DIR/$(basename "$src")"

  [ ! -f "$src" ] && return 0
  [ -e "$dest" ] && return 0

  cp -n "$src" "$dest"
  echo "$src -> $dest" >> "$COPIED_LIST_FILE" 2>/dev/null || true
}

DLL_LIST_FILE="$PROJECT_ROOT/stuff/dll.txt"

if [ ! -f "$DLL_LIST_FILE" ]; then
  echo "ERROR: dll.txt not found at: $DLL_LIST_FILE" >&2
  exit 1
fi

while IFS= read -r dll_path; do
  dll_path="$(echo "$dll_path" | xargs)"
  [ -z "$dll_path" ] && continue
  copy_dll_flat "$dll_path"
done < "$DLL_LIST_FILE"

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

WDEPLOYQT="$(command -v windeployqt.exe 2>/dev/null || command -v windeployqt 2>/dev/null)"
if [ -z "$WDEPLOYQT" ]; then
  echo "ERROR: windeployqt not found in PATH" >&2
  exit 1
fi

rm -f "$DIST_DIR"/Qt*.dll "$DIST_DIR"/qt*.dll 2>/dev/null || true

"$WDEPLOYQT" --no-translations --no-compiler-runtime \
  --dir "$DIST_DIR" "$DIST_DIR/$EXE_NAME"

if [ -f "$DIST_DIR/qt6core.dll" ]; then
  QTCORE_DLL="$DIST_DIR/qt6core.dll"
elif [ -f "$DIST_DIR/Qt6Core.dll" ]; then
  QTCORE_DLL="$DIST_DIR/Qt6Core.dll"
else
  echo "ERROR: Qt6Core.dll not found in $DIST_DIR after windeployqt" >&2
  exit 1
fi

DUMPBIN="C:\Program Files\Microsoft Visual Studio\2022\Enterprise\DIA SDK\bin\dumpbin.exe"
if [ ! -f "$DUMPBIN" ]; then
  DUMPBIN="C:\Program Files (x86)\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC\14.3*\bin\Hostx64\x64\dumpbin.exe"
fi

if (test -f "$DUMPBIN"); then
  "$DUMPBIN" /exports "$QTCORE_DLL" | findstr /C:"WideGetUniversalNameW" || true
fi

rm release/mpr.dll

echo "================================================="
echo " ✅ Done! Package is ready in $DIST_DIR"
echo "================================================="

sleep 1
echo ""