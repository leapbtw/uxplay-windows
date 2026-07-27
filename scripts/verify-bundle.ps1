<# Verifies that the x64 bundle is complete and independent of the development setup.
   Runs static checks and the self-test with runtime paths restricted to the release. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StageDir,

    [string]$MsysRoot = "C:\msys64",

    [string]$EnvironmentName = "ucrt64",

    [string]$TestCacheDir,

    [switch]$SkipStaticValidation
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$projectRoot = Split-Path -Parent $PSScriptRoot
$stage = (Resolve-Path -LiteralPath $StageDir).Path
if (-not $TestCacheDir) {
    $TestCacheDir = Join-Path $projectRoot "out\x64\test-cache"
}
New-Item -ItemType Directory -Force -Path $TestCacheDir | Out-Null

if (-not $SkipStaticValidation) {
    & (Join-Path $PSScriptRoot "collect-runtime-dependencies.ps1") `
        -StageDir $stage `
        -MsysRoot $MsysRoot `
        -EnvironmentName $EnvironmentName `
        -ValidateOnly
}

$requiredFiles = @(
    "uxplay-windows.exe",
    "uxplay-bluetooth-beacon.exe",
    "dnssd.dll",
    "mDNSResponder.exe",
    "platforms\qwindows.dll",
    "lib\gstreamer-1.0",
    "libexec\gstreamer-1.0\gst-plugin-scanner.exe",
    "resources\gstreamer-features.txt",
    "resources\gstreamer-plugins.json",
    "resources\build-manifest.json",
    "resources\bundle-files.json"
)

foreach ($relativePath in $requiredFiles) {
    $path = Join-Path $stage $relativePath
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required bundle path is missing: $relativePath"
    }
}

$savedEnvironment = @{}
$environmentNames = @(
    "PATH",
    "GST_PLUGIN_PATH",
    "GST_PLUGIN_PATH_1_0",
    "GST_PLUGIN_SYSTEM_PATH",
    "GST_PLUGIN_SYSTEM_PATH_1_0",
    "GST_PLUGIN_SCANNER",
    "GST_PLUGIN_SCANNER_1_0",
    "GST_REGISTRY_1_0",
    "GIO_EXTRA_MODULES",
    "FONTCONFIG_PATH"
)

foreach ($name in $environmentNames) {
    $savedEnvironment[$name] = [Environment]::GetEnvironmentVariable(
        $name,
        "Process"
    )
}

try {
    $windowsDir = $env:WINDIR
    if (-not $windowsDir) {
        $windowsDir = "C:\Windows"
    }
    $pluginDir = Join-Path $stage "lib\gstreamer-1.0"
    $scanner = Join-Path $stage "libexec\gstreamer-1.0\gst-plugin-scanner.exe"
    $registry = Join-Path $TestCacheDir "registry-x64.bin"

    if (Test-Path -LiteralPath $registry) {
        Remove-Item -LiteralPath $registry -Force
    }

    $env:PATH = @(
        $stage,
        (Join-Path $windowsDir "System32"),
        $windowsDir,
        (Join-Path $windowsDir "System32\Wbem")
    ) -join ";"
    $env:GST_PLUGIN_PATH = $pluginDir
    $env:GST_PLUGIN_PATH_1_0 = $pluginDir
    $env:GST_PLUGIN_SYSTEM_PATH = $pluginDir
    $env:GST_PLUGIN_SYSTEM_PATH_1_0 = $pluginDir
    $env:GST_PLUGIN_SCANNER = $scanner
    $env:GST_PLUGIN_SCANNER_1_0 = $scanner
    $env:GST_REGISTRY_1_0 = $registry
    $env:GIO_EXTRA_MODULES = Join-Path $stage "lib\gio\modules"
    $env:FONTCONFIG_PATH = Join-Path $stage "etc\fonts"

    & (Join-Path $stage "uxplay-windows.exe") --self-test
    if ($LASTEXITCODE -ne 0) {
        throw "uxplay-windows runtime self-test failed with exit code $LASTEXITCODE"
    }
}
finally {
    foreach ($name in $environmentNames) {
        [Environment]::SetEnvironmentVariable(
            $name,
            $savedEnvironment[$name],
            "Process"
        )
    }
}

Write-Host "Isolated bundle verification passed."
