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
echo " 4. Gathering BIG runtime bundle for CI (only from stuff/dll_arm64.txt)"
echo "================================================="

DLL_LIST_FILE="$PROJECT_ROOT/stuff/dll_arm64.txt"
if [ ! -f "$DLL_LIST_FILE" ]; then
  echo "ERROR: dll_arm64.txt not found at: $DLL_LIST_FILE" >&2
  exit 1
fi

CHANGELOG_FILE="$DIST_DIR/CHANGELOG.txt"
mkdir -p "$DIST_DIR"
: > "$CHANGELOG_FILE"

copy_one_dll() {
    local win_path="$1"
    [ -z "$win_path" ] && return 0

    local p="$win_path"

    [[ "$p" =~ ^[A-Za-z]:\\ ]] && p="$(cygpath -u "$p")"
    [[ ! "$p" =~ \.dll$ ]] && return 0

    if [ ! -f "$p" ]; then
        dir="$(dirname "$p")"
        base="$(basename "$p")"

        if [[ "$base" =~ ^(.*)-[0-9]+\.dll$ ]]; then
            prefix="${BASH_REMATCH[1]}"
            cand=$(find "$dir" -maxdepth 1 -iname "${prefix}-*.dll" | sort -V | tail -n1)

            if [ -n "$cand" ]; then
                echo "BUMP: $base -> $(basename "$cand")"
                echo "$(date '+%F %T') $base -> $(basename "$cand")" >> "$CHANGELOG_FILE"
                p="$cand"
            else
                echo "MISSING: $base"
                return 1
            fi
        else
            echo "MISSING: $base"
            return 1
        fi
    fi

    dest="$DIST_DIR/$(basename "$p")"
    [[ -f "$dest" ]] && return 0

    cp "$p" "$dest"
    echo "OK: $(basename "$p")"
}

while IFS= read -r dll_path; do
  dll_path="$(echo "$dll_path" | xargs)" # trim
  [ -z "$dll_path" ] && continue
  copy_one_dll "$dll_path"
done < "$DLL_LIST_FILE"

echo "Big runtime bundle copied (restricted to dll_arm64.txt)."

echo "================================================="
echo " 5. Finalizing Qt Dependencies (windeployqt)"
echo "================================================="
windeployqt --no-translations --no-compiler-runtime \
  --dir "$DIST_DIR" "$DIST_DIR/$EXE_NAME"

echo "================================================="
echo " 6. Copying mDNSResponder.exe and dnssd.dll (arm64)"
echo "================================================="
cp "./Bonjour SDK/Bin/$BONJOUR_ARCH_DIR/dnssd.dll" release/.
cp "./Bonjour SDK/Bin/$BONJOUR_ARCH_DIR/mDNSResponder.exe" release/.

find $PROJECT_ROOT -name compile_commands.json
cp ./build/compile_commands.json release/.

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