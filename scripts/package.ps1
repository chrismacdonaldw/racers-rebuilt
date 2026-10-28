<#
.SYNOPSIS
Builds the release preset from a clean tree and zips the exe with LICENSE and README.md into
build/package/racers-rebuilt-windows-x64.zip.

.NOTES
Needs VCPKG_ROOT, cmake and ninja, like scripts/check.ps1. Prints the version, size and SHA-256 of
the zip.
#>
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot
. (Join-Path $PSScriptRoot 'msvc.ps1')

$exeName = 'racers-rebuilt.exe'
$binFolder = 'build/release/bin'
$packageFolder = 'build/package'
$zipPath = Join-Path $packageFolder 'racers-rebuilt-windows-x64.zip'

function Invoke-Native([string]$Description, [scriptblock]$Command) {
    Write-Host "==> $Description" -ForegroundColor Cyan
    & $Command
    if ($LASTEXITCODE -ne 0) {
        throw "$Description failed with exit code $LASTEXITCODE."
    }
}

function Get-ProjectVersion {
    $match = Select-String -Path 'CMakeLists.txt' -Pattern 'project\(racers-rebuilt VERSION (\d+\.\d+\.\d+)'
    if (-not $match) {
        throw 'No project(racers-rebuilt VERSION x.y.z) line in CMakeLists.txt.'
    }
    return $match.Matches[0].Groups[1].Value
}

if (@(git status --porcelain --untracked-files=all).Count -gt 0) {
    throw 'The working tree has changes. Package from a clean checkout.'
}
if (-not $env:VCPKG_ROOT) {
    throw 'VCPKG_ROOT is not set. Point it at a vcpkg clone.'
}
foreach ($tool in 'cmake', 'ninja') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool not found on PATH."
    }
}
$problem = Enter-MsvcEnvironment
if ($problem) {
    throw $problem
}

$version = Get-ProjectVersion
Remove-Item -Recurse -Force 'build/release', $packageFolder -ErrorAction SilentlyContinue
Invoke-Native 'Configure (release)' { cmake --preset release }
Invoke-Native 'Build (release)' { cmake --build --preset release }

$built = @(Get-ChildItem $binFolder -File | Select-Object -ExpandProperty Name)
if ($built.Count -ne 1 -or $built[0] -ne $exeName) {
    throw "Expected only $exeName in $binFolder, found: $($built -join ', ')"
}

New-Item -ItemType Directory -Force $packageFolder | Out-Null
$contents = @((Join-Path $binFolder $exeName), 'LICENSE', 'README.md')
Compress-Archive -Path $contents -DestinationPath $zipPath -CompressionLevel Optimal

$zip = Get-Item $zipPath
$hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host ''
Write-Host "racers-rebuilt $version for $(git rev-parse --short HEAD)"
Write-Host ('  {0} ({1:N1} MB)' -f $zip.FullName, ($zip.Length / 1MB))
Write-Host "  SHA-256 $hash"
