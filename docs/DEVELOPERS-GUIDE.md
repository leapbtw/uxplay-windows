# Multi-architecture developer guide

The x64 and ARM64 builds share one entry point for local development and CI:

```powershell
.\build.ps1 package -Architecture x64
.\build.ps1 package -Architecture arm64
```

It installs missing MSYS2 packages, builds the C++ application and Bluetooth
beacon, deploys Qt and GStreamer, discovers transitive DLL dependencies, runs
the isolated runtime self-test, and creates the portable ZIP and MSI.

## One-time prerequisites

- Windows 10 or Windows 11
- [MSYS2](https://www.msys2.org/) installed in `C:\msys64`
- Native CPython 3.14 matching the target architecture for the WinRT Bluetooth beacon
- .NET 8 SDK for the WiX MSI (not needed with `-SkipInstaller`)
- Visual Studio Build Tools with the C++ workload only when the local
  `Bonjour SDK` cache does not exist

If MSYS2 is installed elsewhere, pass its path:

```powershell
.\build.ps1 package -Architecture arm64 -MsysRoot D:\msys64
```

If native Python is not on `PATH`, select it explicitly:

```powershell
.\build.ps1 package -Architecture arm64 -BeaconPython C:\Python314-arm64\python.exe
```

## Commands

The examples below build x64; replace `x64` with `arm64` for the ARM64 build.

```powershell
# Install or update required MSYS2 and Python packages
.\build.ps1 bootstrap -Architecture x64

# Compile without creating a distributable bundle
.\build.ps1 build -Architecture x64

# Build, bundle, verify, and produce ZIP + MSI
.\build.ps1 package -Architecture x64

# Verify an existing release folder in an isolated runtime environment
.\build.ps1 test -Architecture x64

# Remove only the selected architecture's build outputs
.\build.ps1 clean -Architecture x64
```

For a faster developer package without WiX:

```powershell
.\build.ps1 package -Architecture x64 -SkipInstaller
```

The old `./make_release.sh` command remains as a compatibility wrapper around
`build.ps1 package`.

## Output layout

```text
out/<arch>/build       CMake and Ninja build
out/<arch>/beacon      PyInstaller build
out/<arch>/artifacts   MSI and portable ZIP
release                verified runtime staging folder
Bonjour SDK            generated/cached SDK for the selected architecture
```

## How runtime bundling works

`packaging/gstreamer-features.txt` contains stable GStreamer element names,
not DLL filenames. During every build, the installed GStreamer registry maps
those names to the current plugin DLLs.

The Bluetooth beacon is packaged with native Windows CPython so pip can use
the official prebuilt WinRT wheels. MSYS2 Python remains dedicated to the
GStreamer registry, where its PyGObject bindings are required.

After Qt and GStreamer plugins are staged, the dependency collector recursively
reads every PE import with `objdump` and copies the matching runtime DLL from
the active MSYS2 UCRT64 or CLANGARM64 environment. ABI filename changes such as
`libx265-215.dll` to `libx265-216.dll` therefore require no repository update.

The final self-test runs with system GStreamer paths disabled. A missing plugin
or non-system DLL makes both local packaging and CI fail.

## Updating supported GStreamer functionality

When application code starts using a new GStreamer element, add its factory
name to:

```text
packaging/gstreamer-features.txt
```

Do not add DLL paths. The resolver determines the owning plugin automatically.
