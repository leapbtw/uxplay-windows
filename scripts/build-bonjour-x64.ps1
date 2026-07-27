<# Prepares the x64 Bonjour SDK required by uxplay-windows.
   Downloads, patches, and builds dnssd.dll and mDNSResponder when needed. #>
[CmdletBinding()]
param(
    [string]$ProjectRoot,
    [switch]$Force
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if (-not $ProjectRoot) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

$sdk = Join-Path $ProjectRoot "Bonjour SDK"
$required = @(
    (Join-Path $sdk "Include\dns_sd.h"),
    (Join-Path $sdk "Lib\x64\dnssd.lib"),
    (Join-Path $sdk "Bin\x64\dnssd.dll"),
    (Join-Path $sdk "Bin\x64\mDNSResponder.exe")
)

if (-not $Force -and ($required | Where-Object { -not (Test-Path $_) }).Count -eq 0) {
    Write-Host "Bonjour SDK x64 is already available."
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

$sourceDir = Join-Path $ProjectRoot "out\x64\bonjour-source"
if (Test-Path -LiteralPath $sourceDir) {
    $resolvedSource = [IO.Path]::GetFullPath($sourceDir)
    $expectedRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot "out\x64"))
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
    /m /t:Build /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    throw "dnssd.dll build failed."
}

& $msbuild `
    (Join-Path $sourceDir "mDNSWindows\SystemService\mDNSResponder.vcxproj") `
    /m /t:Build /p:Configuration=Release /p:Platform=x64
if ($LASTEXITCODE -ne 0) {
    throw "mDNSResponder.exe build failed."
}

New-Item -ItemType Directory -Force -Path (Join-Path $sdk "Include") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $sdk "Lib\x64") | Out-Null
New-Item -ItemType Directory -Force -Path (Join-Path $sdk "Bin\x64") | Out-Null

Copy-Item `
    (Join-Path $sourceDir "mDNSShared\dns_sd.h") `
    (Join-Path $sdk "Include\dns_sd.h") `
    -Force
Copy-Item `
    (Join-Path $sourceDir "mDNSWindows\DLL\x64\Release\dnssd.lib") `
    (Join-Path $sdk "Lib\x64\dnssd.lib") `
    -Force
Copy-Item `
    (Join-Path $sourceDir "mDNSWindows\DLL\x64\Release\dnssd.dll") `
    (Join-Path $sdk "Bin\x64\dnssd.dll") `
    -Force
Copy-Item `
    (Join-Path $sourceDir "mDNSWindows\SystemService\x64\Release\mDNSResponder.exe") `
    (Join-Path $sdk "Bin\x64\mDNSResponder.exe") `
    -Force

$stamp = [ordered]@{
    source = "apple-oss-distributions/mDNSResponder"
    tag = "rel/mDNSResponder-2881"
    architecture = "x64"
    patchSha256 = (
        Get-FileHash `
            (Join-Path $ProjectRoot "mdnsresponder-patches\2881.patch") `
            -Algorithm SHA256
    ).Hash.ToLowerInvariant()
}
$stamp | ConvertTo-Json |
    Set-Content (Join-Path $sdk "build-info.json") -Encoding utf8

Write-Host "Bonjour SDK x64 built successfully."
