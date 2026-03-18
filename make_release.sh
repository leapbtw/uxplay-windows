#!/bin/bash

EXE_NAME="uxplay-windows.exe"
DIST_DIR="./release"
BUILD_EXE="./build/$EXE_NAME"

if [ ! -f "$BUILD_EXE" ]; then
    echo "ERROR: $BUILD_EXE not found. Compile first."
    exit 1
fi

if ! tasklist | grep -iq "$EXE_NAME"; then
    echo "ERROR: $EXE_NAME is not running. Please start it and BEGIN A STREAM first."
    exit 1
fi

echo "--- Preparing Clean Dist Folder ---"
rm -rf "$DIST_DIR"
mkdir -p "$DIST_DIR/lib/gstreamer-1.0"
[ -f "LICENSE.md" ] && cp "LICENSE.md" "$DIST_DIR/"

cp "$BUILD_EXE" "$DIST_DIR/"

echo "--- Capturing Loaded Modules ---"
DLL_LIST=$(powershell.exe -NoProfile -Command "
    Get-Process -Name '${EXE_NAME%.*}' |
    Select-Object -ExpandProperty Modules |
    Where-Object { \$_.FileName -like '*ucrt64*' } |
    Select-Object -ExpandProperty FileName
")

while read -r win_path; do
    [ -z "$win_path" ] && continue

    # change windows path into unix path
    unix_path=$(cygpath -u "$win_path")

    if [[ "$unix_path" == *"/lib/gstreamer-1.0/"* ]]; then
        cp -v "$unix_path" "$DIST_DIR/lib/gstreamer-1.0/"
    else
        # do not overwrite exe
        if [[ "$unix_path" != *"$EXE_NAME" ]]; then
            cp -v "$unix_path" "$DIST_DIR/"
        fi
    fi
done <<< "$DLL_LIST"

echo "--- Finalizing Qt dependencies ---"
windeployqt --no-translations --no-compiler-runtime --dir "$DIST_DIR" "$DIST_DIR/$EXE_NAME"

echo "--- Done! Package ready in $DIST_DIR ---"
