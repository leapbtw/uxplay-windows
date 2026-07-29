<# Single entry point for preparing, building, packaging, and testing x64 and ARM64.
   Produces the portable bundle and MSI through the same workflow locally and in CI. #>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet("bootstrap", "build", "package", "test", "clean")]
    [string]$Action = "package",

    [ValidateSet("x64", "arm64")]
    [string]$Architecture = "x64",

    [string]$MsysRoot = $(if ($env:MSYS2_ROOT) { $env:MSYS2_ROOT } else { "C:\msys64" }),

    [string]$BeaconPython = $env:BEACON_PYTHON,

    [switch]$SkipBootstrap,

    [switch]$SkipInstaller
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = $PSScriptRoot
$architectureConfig = if ($Architecture -eq "arm64") {
    @{
        EnvironmentName = "clangarm64"
        Msystem = "CLANGARM64"
        DependencyFile = "clangarm64_dependencies.txt"
        PackagePrefix = "^mingw-w64-clang-aarch64-"
        BonjourBinDirectory = "arm64"
        WixArchitecture = "arm64"
        PythonArchitecturePattern = "^(?i:arm64|aarch64)$"
    }
} else {
    @{
        EnvironmentName = "ucrt64"
        Msystem = "UCRT64"
        DependencyFile = "ucrt_x64_dependencies.txt"
        PackagePrefix = "^mingw-w64-ucrt-x86_64-"
        BonjourBinDirectory = "x64"
        WixArchitecture = "x64"
        PythonArchitecturePattern = "^(?i:amd64|x86_64)$"
    }
}
$environmentName = $architectureConfig.EnvironmentName
$prefix = Join-Path $MsysRoot $environmentName
$runtimeBin = Join-Path $prefix "bin"
$outDir = Join-Path $projectRoot "out\$Architecture"
$buildDir = Join-Path $outDir "build"
$beaconOutDir = Join-Path $outDir "beacon"
$artifactDir = Join-Path $outDir "artifacts"
$stageDir = Join-Path $projectRoot "release"
$bonjourSdk = Join-Path $projectRoot "Bonjour SDK"
$featuresFile = Join-Path $projectRoot "packaging\gstreamer-features.txt"
$wixCacheDir = Join-Path $projectRoot ".wix"

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,

        [string[]]$Arguments = @(),

        [string]$WorkingDirectory = $projectRoot
    )

    Push-Location $WorkingDirectory
    try {
        & $FilePath @Arguments
        if ($LASTEXITCODE -ne 0) {
            throw (
                "Command failed with exit code ${LASTEXITCODE}: " +
                "$FilePath $($Arguments -join ' ')"
            )
        }
    }
    finally {
        Pop-Location
    }
}

function Assert-Msys2 {
    $pacman = Join-Path $MsysRoot "usr\bin\pacman.exe"
    if (-not (Test-Path -LiteralPath $pacman)) {
        throw (
            "MSYS2 was not found at $MsysRoot. " +
            "Install MSYS2 from https://www.msys2.org/ or pass -MsysRoot."
        )
    }
}

function Assert-BuildTools {
    $required = @(
        (Join-Path $runtimeBin "cmake.exe"),
        (Join-Path $runtimeBin "python.exe"),
        (Join-Path $runtimeBin "objdump.exe"),
        (Join-Path $runtimeBin "windeployqt.exe"),
        (Join-Path $runtimeBin "gst-inspect-1.0.exe")
    )
    $missing = $required | Where-Object { -not (Test-Path -LiteralPath $_) }
    if ($missing) {
        throw (
            "Required $($architectureConfig.Msystem) tools are missing:`n" +
            ($missing -join [Environment]::NewLine)
        )
    }
}

function Set-BuildEnvironment {
    Assert-Msys2
    $env:MSYSTEM = $architectureConfig.Msystem
    $env:PATH = "$runtimeBin;$(Join-Path $MsysRoot 'usr\bin');$env:PATH"
    $env:BONJOUR_SDK_HOME = $bonjourSdk
    $env:BONJOUR_SDK = $bonjourSdk
}

function Get-BeaconPython {
    $candidates = @()
    if ($BeaconPython) {
        $candidates += $BeaconPython
    }
    if ($env:pythonLocation) {
        $candidates += (Join-Path $env:pythonLocation "python.exe")
    }

    $windowsPython = Get-Command python.exe -All -ErrorAction SilentlyContinue |
        Where-Object {
            $_.Source -and
            -not $_.Source.StartsWith(
                $MsysRoot,
                [StringComparison]::OrdinalIgnoreCase
            )
        } |
        Select-Object -First 1
    if ($windowsPython) {
        $candidates += $windowsPython.Source
    }

    if ($Architecture -eq "x64") {
        # Preserve compatibility with existing local x64 MSYS2 setups. CI
        # always provides native Windows CPython through actions/setup-python.
        $candidates += (Join-Path $runtimeBin "python.exe")
    }

    $selected = $candidates |
        Where-Object { $_ -and (Test-Path -LiteralPath $_) } |
        Select-Object -First 1
    if (-not $selected) {
        throw (
            "Python for the Bluetooth beacon was not found. Install CPython " +
            "3.14 $Architecture or pass -BeaconPython C:\path\to\python.exe."
        )
    }
    return (Resolve-Path -LiteralPath $selected).Path
}

function Test-MsysPython {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Python
    )

    return $Python.StartsWith(
        $MsysRoot,
        [StringComparison]::OrdinalIgnoreCase
    )
}

function Assert-BeaconPythonPackages {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Python
    )

    if (-not (Test-MsysPython -Python $Python)) {
        $pythonArchitecture = (
            & $Python -c "import platform; print(platform.machine())"
        ).Trim()
        if (
            $LASTEXITCODE -ne 0 -or
            $pythonArchitecture -notmatch $architectureConfig.PythonArchitecturePattern
        ) {
            throw (
                "The Bluetooth beacon requires native Windows $Architecture " +
                "Python; $Python reports '$pythonArchitecture'."
            )
        }
    }

    & $Python -c (
        "import PyInstaller;" +
        "import psutil;" +
        "import winrt.windows.foundation;" +
        "import winrt.windows.foundation.collections;" +
        "import winrt.windows.devices.bluetooth.advertisement;" +
        "import winrt.windows.storage.streams"
    )
    if ($LASTEXITCODE -ne 0) {
        throw (
            "Required beacon packages are missing from $Python. " +
            "Run .\build.ps1 bootstrap first."
        )
    }
}

function Install-Dependencies {
    $beaconPython = Get-BeaconPython
    Set-BuildEnvironment
    $pacman = Join-Path $MsysRoot "usr\bin\pacman.exe"
    $packageList = Join-Path $projectRoot $architectureConfig.DependencyFile
    $packages = Get-Content -LiteralPath $packageList |
        ForEach-Object { $_.Trim() } |
        Where-Object { $_ -and -not $_.StartsWith("#") }

    Write-Host "Installing/updating required MSYS2 packages..."
    Invoke-Native `
        -FilePath $pacman `
        -Arguments (@("--noconfirm", "-S", "--needed") + $packages)

    Write-Host "Installing/verifying beacon packages with $beaconPython..."
    $pipArguments = @(
        "-m",
        "pip",
        "install",
        "--disable-pip-version-check"
    )
    if (Test-MsysPython -Python $beaconPython) {
        $pipArguments += "--break-system-packages"
    } else {
        # Native Windows CPython can consume the published WinRT wheels.
        # Do not fall back to the fragile MinGW source build in CI.
        $pipArguments += "--only-binary=:all:"
    }
    $pipArguments += @(
        "--requirement",
        (Join-Path $projectRoot "packaging\python-requirements.txt")
    )
    Invoke-Native `
        -FilePath $beaconPython `
        -Arguments $pipArguments
    Assert-BeaconPythonPackages -Python $beaconPython
}

function Ensure-BonjourSdk {
    & (Join-Path $projectRoot "scripts\build-bonjour.ps1") `
        -ProjectRoot $projectRoot `
        -Architecture $Architecture
}

function Build-Application {
    $beaconPython = Get-BeaconPython
    Set-BuildEnvironment
    Assert-BuildTools
    Assert-BeaconPythonPackages -Python $beaconPython
    Ensure-BonjourSdk
    New-Item -ItemType Directory -Force -Path $buildDir | Out-Null

    $cmake = Join-Path $runtimeBin "cmake.exe"
    Invoke-Native `
        -FilePath $cmake `
        -Arguments @(
            "-S", $projectRoot,
            "-B", $buildDir,
            "-G", "Ninja",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
            "-DNO_MARCH_NATIVE=ON",
            "-UDNSSD_INCLUDE_DIR"
        )
    Invoke-Native `
        -FilePath $cmake `
        -Arguments @("--build", $buildDir, "--parallel")

    $beaconSource = Join-Path $projectRoot "libuxplay\Bluetooth_LE_beacon"
    $beaconDist = Join-Path $beaconOutDir "dist"
    $beaconWork = Join-Path $beaconOutDir "work"
    $beaconSpec = Join-Path $beaconOutDir "spec"
    New-Item -ItemType Directory -Force -Path $beaconDist | Out-Null
    New-Item -ItemType Directory -Force -Path $beaconWork | Out-Null
    New-Item -ItemType Directory -Force -Path $beaconSpec | Out-Null

    Invoke-Native `
        -FilePath $beaconPython `
        -WorkingDirectory $beaconSource `
        -Arguments @(
            "-m", "PyInstaller",
            "--onefile",
            "--name", "uxplay-bluetooth-beacon",
            "--distpath", $beaconDist,
            "--workpath", $beaconWork,
            "--specpath", $beaconSpec,
            "--hidden-import=winrt.windows.foundation",
            "--hidden-import=winrt.windows.foundation.collections",
            "--hidden-import=winrt.windows.devices.bluetooth.advertisement",
            "--hidden-import=winrt.windows.storage.streams",
            "--hidden-import=psutil",
            "--console",
            "--noconfirm",
            "uxplay-beacon-windows.py"
        )
}

function Copy-DirectoryContents {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,

        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Copy-Item `
        -Path (Join-Path $Source "*") `
        -Destination $Destination `
        -Recurse `
        -Force
}

function Write-BuildManifest {
    $pacman = Join-Path $MsysRoot "usr\bin\pacman.exe"
    $cmake = Join-Path $runtimeBin "cmake.exe"
    $python = Join-Path $runtimeBin "python.exe"
    $beaconPython = Get-BeaconPython
    $gstInspect = Join-Path $runtimeBin "gst-inspect-1.0.exe"
    $qmake = Join-Path $runtimeBin "qmake6.exe"

    $packageVersions = @()
    $pacmanOutput = & $pacman -Q
    if ($LASTEXITCODE -eq 0) {
        $packageVersions = $pacmanOutput |
            Where-Object { $_ -match $architectureConfig.PackagePrefix }
    }

    $manifest = [ordered]@{
        generatedAtUtc = [DateTime]::UtcNow.ToString("o")
        architecture = $Architecture
        branch = (& git -C $projectRoot branch --show-current).Trim()
        commit = (& git -C $projectRoot rev-parse HEAD).Trim()
        # Read the gitlink directly. `git submodule` launches helper shell
        # scripts whose Unix utilities might not be on PATH in PowerShell.
        libuxplayCommit = (& git -C $projectRoot rev-parse HEAD:libuxplay).Trim()
        cmake = (& $cmake --version | Select-Object -First 1)
        msys2Python = (& $python --version 2>&1)
        beaconPython = (& $beaconPython --version 2>&1)
        gstreamer = (& $gstInspect --version | Select-Object -Last 1)
        qt = $(if (Test-Path -LiteralPath $qmake) {
            (& $qmake -query QT_VERSION)
        } else {
            "unknown"
        })
        msys2Packages = $packageVersions
    }

    $manifest |
        ConvertTo-Json -Depth 5 |
        Set-Content `
            -LiteralPath (Join-Path $stageDir "resources\build-manifest.json") `
            -Encoding utf8
}

function Stage-Runtime {
    Set-BuildEnvironment
    if (Test-Path -LiteralPath $stageDir) {
        Remove-Item -LiteralPath $stageDir -Recurse -Force
    }

    New-Item -ItemType Directory -Force -Path $stageDir | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $stageDir "resources") |
        Out-Null
    New-Item `
        -ItemType Directory `
        -Force `
        -Path (Join-Path $stageDir "lib\gstreamer-1.0") |
        Out-Null

    Copy-Item `
        (Join-Path $buildDir "uxplay-windows.exe") `
        (Join-Path $stageDir "uxplay-windows.exe") `
        -Force
    Copy-Item `
        (Join-Path $beaconOutDir "dist\uxplay-bluetooth-beacon.exe") `
        (Join-Path $stageDir "uxplay-bluetooth-beacon.exe") `
        -Force
    Copy-Item `
        (Join-Path $bonjourSdk "Bin\$($architectureConfig.BonjourBinDirectory)\dnssd.dll") `
        (Join-Path $stageDir "dnssd.dll") `
        -Force
    Copy-Item `
        (Join-Path $bonjourSdk "Bin\$($architectureConfig.BonjourBinDirectory)\mDNSResponder.exe") `
        (Join-Path $stageDir "mDNSResponder.exe") `
        -Force
    Copy-Item `
        (Join-Path $projectRoot "docs\LICENSE.rtf") `
        (Join-Path $stageDir "LICENSE.rtf") `
        -Force
    Copy-Item `
        (Join-Path $projectRoot "stuff\newicon.ico") `
        (Join-Path $stageDir "resources\icon.ico") `
        -Force
    Copy-Item `
        (Join-Path $projectRoot "stuff\uxplay_arguments_list.txt") `
        (Join-Path $stageDir "resources\uxplay_arguments_list.txt") `
        -Force
    Copy-Item `
        $featuresFile `
        (Join-Path $stageDir "resources\gstreamer-features.txt") `
        -Force

    $windeployqt = Join-Path $runtimeBin "windeployqt.exe"
    Invoke-Native `
        -FilePath $windeployqt `
        -Arguments @(
            "--release",
            "--no-translations",
            "--no-compiler-runtime",
            "--dir", $stageDir,
            (Join-Path $stageDir "uxplay-windows.exe")
        )

    $gstPluginDir = Join-Path $prefix "lib\gstreamer-1.0"
    $registry = Join-Path $outDir "gstreamer-build-registry.bin"
    if (Test-Path -LiteralPath $registry) {
        Remove-Item -LiteralPath $registry -Force
    }
    $env:GST_PLUGIN_PATH = ""
    $env:GST_PLUGIN_PATH_1_0 = ""
    $env:GST_PLUGIN_SYSTEM_PATH = $gstPluginDir
    $env:GST_PLUGIN_SYSTEM_PATH_1_0 = $gstPluginDir
    $env:GST_REGISTRY_1_0 = $registry

    Invoke-Native `
        -FilePath (Join-Path $runtimeBin "python.exe") `
        -Arguments @(
            (Join-Path $projectRoot "scripts\resolve-gstreamer-plugins.py"),
            "--features", $featuresFile,
            "--plugin-dir", $gstPluginDir,
            "--destination", (Join-Path $stageDir "lib\gstreamer-1.0"),
            "--manifest", (Join-Path $stageDir "resources\gstreamer-plugins.json")
        )

    $scannerSource = Join-Path `
        $prefix `
        "libexec\gstreamer-1.0\gst-plugin-scanner.exe"
    $scannerDestination = Join-Path $stageDir "libexec\gstreamer-1.0"
    New-Item -ItemType Directory -Force -Path $scannerDestination | Out-Null
    Copy-Item -LiteralPath $scannerSource -Destination $scannerDestination -Force

    $gioSource = Join-Path $prefix "lib\gio\modules"
    $gioDestination = Join-Path $stageDir "lib\gio\modules"
    New-Item -ItemType Directory -Force -Path $gioDestination | Out-Null
    Get-ChildItem -LiteralPath $gioSource -Filter "*.dll" -File |
        ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $gioDestination -Force
        }

    Copy-DirectoryContents `
        -Source (Join-Path $prefix "etc\fonts") `
        -Destination (Join-Path $stageDir "etc\fonts")
    Copy-DirectoryContents `
        -Source (Join-Path $prefix "share\fontconfig") `
        -Destination (Join-Path $stageDir "share\fontconfig")

    Write-BuildManifest

    & (Join-Path $projectRoot "scripts\collect-runtime-dependencies.ps1") `
        -StageDir $stageDir `
        -MsysRoot $MsysRoot `
        -EnvironmentName $environmentName `
        -ManifestPath (Join-Path $stageDir "resources\bundle-files.json")
}

function Test-Runtime {
    param(
        [switch]$SkipStaticValidation
    )

    Set-BuildEnvironment
    $arguments = @{
        StageDir = $stageDir
        MsysRoot = $MsysRoot
        EnvironmentName = $environmentName
        TestCacheDir = (Join-Path $outDir "test-cache")
    }
    if ($SkipStaticValidation) {
        $arguments.SkipStaticValidation = $true
    }
    & (Join-Path $projectRoot "scripts\verify-bundle.ps1") @arguments
}

function Ensure-Wix {
    $dotnetCommand = Get-Command dotnet.exe -ErrorAction SilentlyContinue
    if (-not $dotnetCommand) {
        throw ".NET 8 SDK is required for the MSI. Use -SkipInstaller for ZIP only."
    }

    $env:DOTNET_CLI_TELEMETRY_OPTOUT = "1"
    $env:DOTNET_NOLOGO = "1"
    $env:DOTNET_SKIP_FIRST_TIME_EXPERIENCE = "1"

    $sdks = & $dotnetCommand.Source --list-sdks
    if ($LASTEXITCODE -ne 0) {
        throw "Unable to inspect installed .NET SDKs."
    }
    if (-not $sdks) {
        throw ".NET 8 SDK is required for the MSI. Use -SkipInstaller for ZIP only."
    }

    $env:DOTNET_CLI_HOME = Join-Path $outDir "dotnet-home"
    $env:NUGET_PACKAGES = Join-Path $outDir "nuget-packages"
    $env:APPDATA = Join-Path $env:DOTNET_CLI_HOME "AppData\Roaming"
    $env:LOCALAPPDATA = Join-Path $env:DOTNET_CLI_HOME "AppData\Local"
    foreach ($directory in @(
        $env:DOTNET_CLI_HOME,
        $env:NUGET_PACKAGES,
        $env:APPDATA,
        $env:LOCALAPPDATA
    )) {
        New-Item -ItemType Directory -Force -Path $directory | Out-Null
    }

    Invoke-Native `
        -FilePath $dotnetCommand.Source `
        -Arguments @(
            "tool", "restore",
            "--tool-manifest",
            (Join-Path $projectRoot ".config\dotnet-tools.json"),
            "--configfile",
            (Join-Path $projectRoot "NuGet.config")
        )

    $extensions = & $dotnetCommand.Source wix extension list -acceptEula wix7
    $extensionListExitCode = $LASTEXITCODE
    # WiX returns 2 when the local extension cache is empty.
    if ($extensionListExitCode -notin @(0, 2)) {
        throw "Unable to list WiX extensions."
    }
    if (-not ($extensions -match "WixToolset\.UI\.wixext")) {
        Invoke-Native `
            -FilePath $dotnetCommand.Source `
            -Arguments @(
                "wix", "extension", "add",
                "WixToolset.UI.wixext/7.0.0",
                "-acceptEula", "wix7"
            )
    }

}

function Build-Artifacts {
    New-Item -ItemType Directory -Force -Path $artifactDir | Out-Null
    $zip = Join-Path $artifactDir "uxplay-windows-$Architecture-portable.zip"
    if (Test-Path -LiteralPath $zip) {
        Remove-Item -LiteralPath $zip -Force
    }
    Compress-Archive `
        -Path (Join-Path $stageDir "*") `
        -DestinationPath $zip `
        -CompressionLevel Optimal

    if (-not $SkipInstaller) {
        Ensure-Wix
        $dotnet = (Get-Command dotnet.exe -ErrorAction Stop).Source
        $msi = Join-Path $artifactDir "uxplay-windows-$Architecture.msi"
        $wixPdb = [IO.Path]::ChangeExtension($msi, ".wixpdb")
        if (Test-Path -LiteralPath $wixPdb) {
            Remove-Item -LiteralPath $wixPdb -Force
        }
        Invoke-Native `
            -FilePath $dotnet `
            -Arguments @(
                "wix", "build",
                "-acceptEula", "wix7",
                (Join-Path $projectRoot "product.wxs"),
                "-arch", $architectureConfig.WixArchitecture,
                "-out", $msi,
                "-pdbtype", "none",
                "-ext", "WixToolset.UI.wixext"
            )
        Invoke-Native `
            -FilePath $dotnet `
            -Arguments @(
                "wix", "msi", "validate",
                $msi,
                "-acceptEula", "wix7"
            )
    }
}

function Clear-BuildOutputs {
    foreach ($target in @($outDir, $stageDir, $wixCacheDir)) {
        if (-not (Test-Path -LiteralPath $target)) {
            continue
        }

        $resolved = [IO.Path]::GetFullPath($target)
        $allowedRoots = @(
            [IO.Path]::GetFullPath($outDir),
            [IO.Path]::GetFullPath((Join-Path $projectRoot "release")),
            [IO.Path]::GetFullPath((Join-Path $projectRoot ".wix"))
        )
        if ($resolved -notin $allowedRoots) {
            throw "Refusing to remove unexpected path: $resolved"
        }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}

switch ($Action) {
    "bootstrap" {
        Install-Dependencies
    }
    "build" {
        if (-not $SkipBootstrap) {
            Install-Dependencies
        }
        Build-Application
    }
    "package" {
        if (-not $SkipBootstrap) {
            Install-Dependencies
        }
        Build-Application
        Stage-Runtime
        Test-Runtime -SkipStaticValidation
        Build-Artifacts
        Write-Host "Artifacts are ready in $artifactDir"
    }
    "test" {
        Test-Runtime
    }
    "clean" {
        Clear-BuildOutputs
        Write-Host "$Architecture build outputs removed."
    }
}
