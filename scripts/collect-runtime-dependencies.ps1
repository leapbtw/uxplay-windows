<# Builds and validates the recursive PE dependency closure of a Windows bundle.
   Reads imports with objdump and copies missing DLLs from the selected MSYS2 environment. #>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$StageDir,

    [string]$MsysRoot = "C:\msys64",

    [string]$EnvironmentName = "ucrt64",

    [switch]$ValidateOnly,

    [string]$ManifestPath
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Get-RelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BasePath,

        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    $absoluteBase = [IO.Path]::GetFullPath($BasePath)
    $separator = [string][IO.Path]::DirectorySeparatorChar
    if (-not $absoluteBase.EndsWith($separator)) {
        $absoluteBase += [IO.Path]::DirectorySeparatorChar
    }

    $baseUri = [Uri]$absoluteBase
    $pathUri = [Uri][IO.Path]::GetFullPath($Path)
    return [Uri]::UnescapeDataString(
        $baseUri.MakeRelativeUri($pathUri).ToString()
    ).Replace("/", $separator)
}

$stage = (Resolve-Path -LiteralPath $StageDir).Path
$runtimeBin = Join-Path $MsysRoot "$EnvironmentName\bin"
$objdump = Join-Path $runtimeBin "objdump.exe"
$runtimeAliases = @{
    # Some CLANGARM64 packages ship lib-prefixed files whose PE import names
    # omit that prefix.
    "dovi.dll" = "libdovi.dll"
    "rav1e.dll" = "librav1e.dll"
}

if (-not (Test-Path -LiteralPath $objdump)) {
    throw "objdump.exe not found at $objdump"
}

$queue = [System.Collections.Generic.Queue[string]]::new()
$visited = [System.Collections.Generic.HashSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
$unresolved = [System.Collections.Generic.SortedSet[string]]::new(
    [System.StringComparer]::OrdinalIgnoreCase
)
$copied = 0

Get-ChildItem -LiteralPath $stage -Recurse -File |
    Where-Object { $_.Extension -in @(".exe", ".dll") } |
    ForEach-Object { $queue.Enqueue($_.FullName) }

if ($queue.Count -eq 0) {
    throw "No PE binaries found under $stage"
}

$windowsDir = $env:WINDIR
if (-not $windowsDir) {
    $windowsDir = "C:\Windows"
}
$systemDirectories = @(
    (Join-Path $windowsDir "System32"),
    $windowsDir
)

while ($queue.Count -gt 0) {
    $binary = $queue.Dequeue()
    if (-not $visited.Add($binary)) {
        continue
    }

    $output = & $objdump -p $binary 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "objdump failed for $binary`n$($output -join [Environment]::NewLine)"
    }

    $imports = foreach ($line in $output) {
        if ($line -match "DLL Name:\s*(.+)$") {
            $Matches[1].Trim()
        }
    }

    foreach ($import in ($imports | Sort-Object -Unique)) {
        $localCandidates = @(
            (Join-Path (Split-Path -Parent $binary) $import),
            (Join-Path $stage $import)
        )

        $localDependency = $localCandidates |
            Where-Object { Test-Path -LiteralPath $_ } |
            Select-Object -First 1

        if ($localDependency) {
            $queue.Enqueue((Resolve-Path -LiteralPath $localDependency).Path)
            continue
        }

        $runtimeDependency = Join-Path $runtimeBin $import
        if (
            -not (Test-Path -LiteralPath $runtimeDependency) -and
            $runtimeAliases.ContainsKey($import)
        ) {
            $runtimeDependency = Join-Path $runtimeBin $runtimeAliases[$import]
        }

        if (Test-Path -LiteralPath $runtimeDependency) {
            if ($ValidateOnly) {
                $relativeBinary = Get-RelativePath -BasePath $stage -Path $binary
                [void]$unresolved.Add(
                    "$import required by $relativeBinary (available only in $runtimeBin)"
                )
                continue
            }

            $destination = Join-Path $stage $import
            if (-not (Test-Path -LiteralPath $destination)) {
                Copy-Item -LiteralPath $runtimeDependency -Destination $destination
                $copied++
            }
            $queue.Enqueue((Resolve-Path -LiteralPath $destination).Path)
            continue
        }

        $isWindowsDependency = $false
        foreach ($directory in $systemDirectories) {
            if (Test-Path -LiteralPath (Join-Path $directory $import)) {
                $isWindowsDependency = $true
                break
            }
        }

        if (
            $isWindowsDependency -or
            $import -match "^(api-ms-win-|ext-ms-win-)"
        ) {
            continue
        }

        $relativeBinary = Get-RelativePath -BasePath $stage -Path $binary
        [void]$unresolved.Add("$import required by $relativeBinary")
    }
}

if ($unresolved.Count -gt 0) {
    $details = $unresolved -join [Environment]::NewLine
    throw "Unresolved non-system runtime dependencies:`n$details"
}

if (-not $ValidateOnly -and $ManifestPath) {
    $manifestDirectory = Split-Path -Parent $ManifestPath
    if ($manifestDirectory) {
        New-Item -ItemType Directory -Force -Path $manifestDirectory | Out-Null
    }

    $files = Get-ChildItem -LiteralPath $stage -Recurse -File |
        Sort-Object FullName |
        ForEach-Object {
            [ordered]@{
                path = (Get-RelativePath -BasePath $stage -Path $_.FullName).
                    Replace("\", "/")
                bytes = $_.Length
                sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.
                    ToLowerInvariant()
            }
        }

    $files | ConvertTo-Json -Depth 4 |
        Set-Content -LiteralPath $ManifestPath -Encoding utf8
}

Write-Host (
    "Runtime dependency check passed: {0} binaries inspected, {1} DLLs copied." -f
    $visited.Count,
    $copied
)
