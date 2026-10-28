<#
.SYNOPSIS
Runs every pre-PR check on a clean tree: repo guards, a fresh configure, build, tests, clang-format
and clang-tidy. Prints one summary line per step and exits non-zero on the first failure.

.PARAMETER Preset
CMake preset to configure, build and test. Defaults to check (Release with asserts).

.NOTES
RR_BANNED_TERMS_FILE must name a text file outside the repo with one extended regex per line.
Matches in tracked files, commit metadata or the branch name fail the run.
#>
param(
    [ValidateSet('check', 'debug')]
    [string]$Preset = 'check'
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$gameDataMagics = @('LJAM', 'ALP ')
$results = New-Object System.Collections.Generic.List[string]
$head = git rev-parse HEAD

function Write-Summary([string]$Outcome) {
    Write-Host ''
    Write-Host "Summary for $head ($Preset)"
    $results | ForEach-Object { Write-Host "  $_" }
    Write-Host $Outcome
}

function Invoke-Step([string]$Name, [scriptblock]$Command) {
    Write-Host "==> $Name" -ForegroundColor Cyan
    $timer = [Diagnostics.Stopwatch]::StartNew()
    $ErrorActionPreference = 'Continue'
    $global:LASTEXITCODE = 0
    & $Command 2>&1 | ForEach-Object { "$_" }
    $status = if ($LASTEXITCODE -eq 0) { 'PASS' } else { 'FAIL' }
    $results.Add(('{0}  {1} ({2:N1} s)' -f $status, $Name, $timer.Elapsed.TotalSeconds))
    if ($status -eq 'FAIL') {
        Write-Summary 'Check failed.'
        exit 1
    }
}

function Fail([string]$Message) {
    Write-Host "  $Message" -ForegroundColor Red
    $global:LASTEXITCODE = 1
}

function Find-Tool([string]$Name) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }
    $llvmPath = Join-Path $env:ProgramFiles "LLVM\bin\$Name.exe"
    if (Test-Path $llvmPath) {
        return $llvmPath
    }
    Fail "$Name not found. Install LLVM or add it to PATH."
    return $null
}

function Enter-MsvcEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        Fail 'vswhere.exe not found. Install Visual Studio 2022 Build Tools.'
        return
    }
    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installPath) {
        Fail 'MSVC x64 build tools not found. Install Visual Studio 2022 Build Tools.'
        return
    }
    $vcvars = Join-Path $installPath 'VC\Auxiliary\Build\vcvars64.bat'
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            Set-Item "env:$($Matches[1])" $Matches[2]
        }
    }
}

function Get-TrackedFiles([string[]]$Patterns) {
    return @(git -c core.quotepath=off ls-files -- $Patterns)
}

function Read-FileMagic([string]$Path) {
    $stream = [IO.File]::OpenRead((Join-Path $repoRoot $Path))
    try {
        $buffer = New-Object byte[] 4
        $count = $stream.Read($buffer, 0, 4)
        return [Text.Encoding]::ASCII.GetString($buffer, 0, $count)
    }
    finally {
        $stream.Dispose()
    }
}

function Get-BannedTerms {
    $path = $env:RR_BANNED_TERMS_FILE
    if (-not $path -or -not (Test-Path $path -PathType Leaf)) {
        Fail 'RR_BANNED_TERMS_FILE must name an existing banned-terms file.'
        return @()
    }
    $fullPath = (Resolve-Path $path).Path
    if ($fullPath.StartsWith($repoRoot, [StringComparison]::OrdinalIgnoreCase)) {
        Fail 'The banned-terms file must live outside the repo.'
        return @()
    }
    $terms = @(Get-Content $fullPath | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    if ($terms.Count -eq 0) {
        Fail 'The banned-terms file is empty.'
    }
    return $terms
}

Invoke-Step 'Clean working tree' {
    $changes = @(git status --porcelain --untracked-files=all)
    if ($changes.Count -gt 0) {
        $changes | ForEach-Object { Fail "uncommitted: $_" }
    }
}

Invoke-Step 'No game data or build output tracked' {
    $ignoredHits = @(git -c core.ignorecase=true -c core.quotepath=off ls-files --cached --ignored --exclude-standard)
    $magicHits = @(Get-TrackedFiles @() | Where-Object { $gameDataMagics -contains (Read-FileMagic $_) })
    $global:LASTEXITCODE = 0
    $ignoredHits | ForEach-Object { Fail "matches .gitignore: $_" }
    $magicHits | ForEach-Object { Fail "game data signature: $_" }
}

Invoke-Step 'No banned terms in tree, history or branch' {
    $terms = @(Get-BannedTerms)
    if ($terms.Count -eq 0) {
        return
    }
    $patternArgs = $terms | ForEach-Object { '-e', $_ }
    $treeHits = @(git grep -n -i -E @patternArgs HEAD -- .)
    $metadata = @(git log --format='%h %an <%ae> %cn <%ce>%n%B' HEAD) + @(git branch --show-current)
    $metadataHits = @($metadata | Select-String -Pattern $terms)
    $global:LASTEXITCODE = 0
    $treeHits | ForEach-Object { Fail "tree: $_" }
    $metadataHits | ForEach-Object { Fail "history or branch: $_" }
}

Invoke-Step 'Toolchain' {
    if (-not $env:VCPKG_ROOT) {
        Fail 'VCPKG_ROOT is not set. Point it at a vcpkg clone.'
    }
    foreach ($tool in 'cmake', 'ninja') {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            Fail "$tool not found on PATH."
        }
    }
    if ($LASTEXITCODE -eq 0) {
        Enter-MsvcEnvironment
    }
}

Invoke-Step "Configure into empty build/$Preset" {
    Remove-Item -Recurse -Force "build/$Preset" -ErrorAction SilentlyContinue
    cmake --preset $Preset
}
Invoke-Step "Build ($Preset)" { cmake --build --preset $Preset }
Invoke-Step "Test ($Preset)" { ctest --preset $Preset }

Invoke-Step 'clang-format' {
    $clangFormat = Find-Tool 'clang-format'
    if ($clangFormat) {
        & $clangFormat --dry-run --Werror (Get-TrackedFiles @('*.cpp', '*.hpp'))
    }
}
Invoke-Step 'clang-tidy' {
    $clangTidy = Find-Tool 'clang-tidy'
    if ($clangTidy) {
        & $clangTidy -p "build/$Preset" --quiet (Get-TrackedFiles @('src/*.cpp', 'tests/*.cpp'))
    }
}

Write-Summary 'All checks passed.'
