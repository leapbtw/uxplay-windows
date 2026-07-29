<# Prepares the x64 or ARM64 Bonjour SDK required by uxplay-windows.
   Downloads, patches, and builds dnssd.dll and mDNSResponder when needed. #>
[CmdletBinding()]
param(
    [string]$ProjectRoot,

    [ValidateSet("x64", "arm64")]
    [string]$Architecture = "x64",

    [switch]$Force
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$msbuildPlatform = if ($Architecture -eq "arm64") { "ARM64" } else { "x64" }
$sourcePlatformDirectory = if ($Architecture -eq "arm64") { "arm64" } else { "x64" }
$sdk = Join-Path $ProjectRoot "Bonjour SDK"
$required = @(
    (Join-Path $sdk "Include\dns_sd.h"),
    (Join-Path $sdk "Lib\x64\dnssd.lib"),
    (Join-Path $sdk "Bin\$Architecture\dnssd.dll"),
    (Join-Path $sdk "Bin\$Architecture\mDNSResponder.exe"),
    (Join-Path $sdk "build-info.json")
)
if ($Architecture -eq "arm64") {
    $required += (Join-Path $sdk "Lib\arm64\dnssd.lib")
}

$cacheValid = ($required | Where-Object { -not (Test-Path $_) }).Count -eq 0
if ($cacheValid) {
    try {
        $buildInfo = Get-Content `
            -LiteralPath (Join-Path $sdk "build-info.json") `
            -Raw |
            ConvertFrom-Json
        $cacheValid = $buildInfo.architecture -eq $Architecture
    }
    catch {
        $cacheValid = $false
    }
}

if (-not $Force -and $cacheValid) {
    Write-Host "Bonjour SDK $Architecture is already available."
    return
}

$msbuildCommand = Get-Command msbuild.exe -ErrorAction SilentlyContinue
$msbuild = if ($msbuildCommand) {
    $msbuildCommand.Source
} else {
    $null
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} `
    "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not $msbuild -and (Test-Path -LiteralPath $vswhere)) {
    $msbuild = & $vswhere `
        -latest `
        -products * `
        -requires Microsoft.Component.MSBuild `
        -find "MSBuild\**\Bin\MSBuild.exe" |
        Select-Object -First 1
}

if (-not $msbuild) {
    $candidates = Get-ChildItem `
        -Path "$env:ProgramFiles\Microsoft Visual Studio\2022\*\MSBuild\Current\Bin\MSBuild.exe" `
        -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($candidates) {
        $msbuild = $candidates.FullName
    }
}

if (-not $msbuild) {
    throw (
        "Bonjour SDK is missing and MSBuild was not found. " +
        "Install Visual Studio Build Tools with the C++ workload, then retry."
    )
}

$architectureOutDir = Join-Path $ProjectRoot "out\$Architecture"
$sourceDir = Join-Path $architectureOutDir "bonjour-source"
if (Test-Path -LiteralPath $sourceDir) {
    $resolvedSource = [IO.Path]::GetFullPath($sourceDir)
    $expectedRoot = [IO.Path]::GetFullPath($architectureOutDir)
    if (-not $resolvedSource.StartsWith(
        $expectedRoot,
        [StringComparison]::OrdinalIgnoreCase
    )) {
        throw "Refusing to remove unexpected Bonjour source path: $resolvedSource"
    }
    Remove-Item -LiteralPath $resolvedSource -Recurse -Force
}
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $sourceDir) |
    Out-Null

& git clone `
    --branch rel/mDNSResponder-2881 `
    --depth 1 `
    --single-branch `
    https://github.com/apple-oss-distributions/mDNSResponder.git `
    $sourceDir
if ($LASTEXITCODE -ne 0) {
    throw "Unable to clone mDNSResponder."
}

& git -C $sourceDir apply `
    (Join-Path $ProjectRoot "mdnsresponder-patches\2881.patch")
if ($LASTEXITCODE -ne 0) {
    throw "Unable to apply the mDNSResponder patch."
}

& $msbuild `
    (Join-Path $sourceDir "mDNSWindows\DLL\dnssd.vcxproj") `
    /m /t:Build /p:Configuration=Release /p:Platform=$msbuildPlatform
if ($LASTEXITCODE -ne 0) {
    throw "dnssd.dll build failed."
}

& $msbuild `
    (Join-Path $sourceDir "mDNSWindows\SystemService\mDNSResponder.vcxproj") `
    /m /t:Build /p:Configuration=Release /p:Platform=$msbuildPlatform
if ($LASTEXITCODE -ne 0) {
    throw "mDNSResponder.exe build failed."
}

New-Item -ItemType Directory -Force -Path (Join-Path $sdk "Include") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $sdk "Lib\x64") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $sdk "Bin\$Architecture") |
    Out-Null
if ($Architecture -eq "arm64") {
    New-Item -ItemType Directory -Force -Path (Join-Path $sdk "Lib\arm64") |
        Out-Null
}

Copy-Item `
    (Join-Path $sourceDir "mDNSShared\dns_sd.h") `
    (Join-Path $sdk "Include\dns_sd.h") `
    -Force
Copy-Item `
    (Join-Path $sourceDir "mDNSWindows\DLL\$sourcePlatformDirectory\Release\dnssd.lib") `
    (Join-Path $sdk "Lib\x64\dnssd.lib") `
    -Force
if ($Architecture -eq "arm64") {
    # libuxplay currently looks under Lib\x64 on Windows. Keep the native
    # location as well as the compatibility alias until it is configurable.
    Copy-Item `
        (Join-Path $sourceDir "mDNSWindows\DLL\$sourcePlatformDirectory\Release\dnssd.lib") `
        (Join-Path $sdk "Lib\arm64\dnssd.lib") `
        -Force
}
Copy-Item `
    (Join-Path $sourceDir "mDNSWindows\DLL\$sourcePlatformDirectory\Release\dnssd.dll") `
    (Join-Path $sdk "Bin\$Architecture\dnssd.dll") `
    -Force
Copy-Item `
    (Join-Path $sourceDir "mDNSWindows\SystemService\$sourcePlatformDirectory\Release\mDNSResponder.exe") `
    (Join-Path $sdk "Bin\$Architecture\mDNSResponder.exe") `
    -Force

$stamp = [ordered]@{
    source = "apple-oss-distributions/mDNSResponder"
    tag = "rel/mDNSResponder-2881"
    architecture = $Architecture
    patchSha256 = (
        Get-FileHash `
            (Join-Path $ProjectRoot "mdnsresponder-patches\2881.patch") `
            -Algorithm SHA256
    ).Hash.ToLowerInvariant()
}
$stamp | ConvertTo-Json |
    Set-Content (Join-Path $sdk "build-info.json") -Encoding utf8

Write-Host "Bonjour SDK $Architecture built successfully."
