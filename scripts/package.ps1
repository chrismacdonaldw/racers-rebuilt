<#
.SYNOPSIS
Builds the release preset from a clean tree and zips the exe with LICENSE, the user docs, the
content/ldraw folder and a licenses folder into build/package/racers-rebuilt-windows-x64.zip.

.NOTES
Needs VCPKG_ROOT, cmake and ninja, like scripts/check.ps1. Prints the version, size and SHA-256 of
the zip. The build runs CMAKE_BUILD_PARALLEL_LEVEL jobs, or half the cores when it isn't set.
The licenses folder holds vcpkg's copyright file for each vcpkg.json dependency the exe links.
#>
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot
. (Join-Path $PSScriptRoot 'msvc.ps1')
. (Join-Path $PSScriptRoot 'jobs.ps1')

$exeName = 'racers-rebuilt.exe'
$binFolder = 'build/release/bin'
$packageFolder = 'build/package'
$ldrawFolder = 'content/ldraw'
$zipPath = Join-Path $packageFolder 'racers-rebuilt-windows-x64.zip'
$licensesFolder = Join-Path $packageFolder 'licenses'
$packageLdrawFolder = Join-Path $packageFolder 'ldraw'
$userDocs = @('LICENSE', 'README.md', 'SETTINGS.md', 'MODDING.md')
$vcpkgShare = 'build/release/vcpkg_installed/x64-windows-static/share'
$testOnlyPorts = @('doctest')

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

function Get-LinkedPorts {
    $manifest = Get-Content 'vcpkg.json' -Raw | ConvertFrom-Json
    $names = foreach ($dependency in $manifest.dependencies) {
        if ($dependency -is [string]) { $dependency } else { $dependency.name }
    }
    return @($names | Where-Object { $_ -notin $testOnlyPorts })
}

function Copy-Licenses {
    New-Item -ItemType Directory -Force $licensesFolder | Out-Null
    foreach ($port in Get-LinkedPorts) {
        $copyright = Join-Path $vcpkgShare "$port/copyright"
        if (-not (Test-Path $copyright)) {
            throw "No license for $port at $copyright."
        }
        Copy-Item $copyright (Join-Path $licensesFolder "$port.txt")
    }
}

function Copy-Ldraw {
    Copy-Item $ldrawFolder $packageLdrawFolder -Recurse
    Remove-Item (Join-Path $packageLdrawFolder '.gitattributes')
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
Invoke-Native 'Build (release)' { cmake --build --preset release --parallel (Get-JobCount) }

$built = @(Get-ChildItem $binFolder -File | Select-Object -ExpandProperty Name)
if ($built.Count -ne 1 -or $built[0] -ne $exeName) {
    throw "Expected only $exeName in $binFolder, found: $($built -join ', ')"
}

New-Item -ItemType Directory -Force $packageFolder | Out-Null
Copy-Licenses
Copy-Ldraw
$contents = @((Join-Path $binFolder $exeName)) + $userDocs + @($packageLdrawFolder, $licensesFolder)
Compress-Archive -Path $contents -DestinationPath $zipPath -CompressionLevel Optimal

$zip = Get-Item $zipPath
$hash = (Get-FileHash $zipPath -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Host ''
Write-Host "racers-rebuilt $version for $(git rev-parse --short HEAD)"
Write-Host ('  {0} ({1:N1} MB)' -f $zip.FullName, ($zip.Length / 1MB))
Write-Host "  SHA-256 $hash"
