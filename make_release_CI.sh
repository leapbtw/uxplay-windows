#!/bin/bash
set -e

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

DLL_LIST_FILE="$PROJECT_ROOT/stuff/dll.txt"

copy_dll_preserve_path() {
  local src="$1"

  [ ! -f "$src" ] && return 0

  local relpath="$src"
  if [[ "$src" == "$UCRT64_PREFIX"* ]]; then
    relpath="${src#"$UCRT64_PREFIX"/}"
  else
    relpath="$(echo "$src" | sed 's#^/##')" # fallback
  fi

  local dest="$DIST_DIR/$relpath"
  local dest_dir
  dest_dir="$(dirname "$dest")"

  mkdir -p "$dest_dir"

  cp -n "$src" "$dest" 2>/dev/null || true
}

while IFS= read -r dll_path; do
  dll_path="$(echo "$dll_path" | xargs)" # trim
  [ -z "$dll_path" ] && continue
  copy_dll_preserve_path "$dll_path"
done < "$DLL_LIST_FILE"

echo "Big runtime bundle copied (restricted to dll.txt)."

echo "================================================="
echo " 5. Finalizing Qt Dependencies (windeployqt)"
echo "================================================="
windeployqt --no-translations --no-compiler-runtime \
  --dir "$DIST_DIR" "$DIST_DIR/$EXE_NAME"

echo "================================================="
echo " ✅ Done! Package is ready in $DIST_DIR"
echo "================================================="

sleep 1
echo ""