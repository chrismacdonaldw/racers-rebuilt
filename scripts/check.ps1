<#
.SYNOPSIS
Runs every pre-PR check on a clean tree: repo guards, clang-format, configure, build, tests and
clang-tidy. Prints one summary line per step and exits non-zero on the first failure.

.PARAMETER Preset
CMake preset to configure, build and test. Defaults to check (Release with asserts).

.PARAMETER Full
Configures into an empty build folder and runs clang-tidy on every source. Without it the build
folder is reused, and clang-tidy only checks sources that differ from origin/main or include a
header that does, unless the diff touches the toolchain setup.

.NOTES
RR_BANNED_TERMS_FILE must name a text file outside the repo with one extended regex per line.
Matches in tracked files, commit metadata or the branch name fail the run.

The build and clang-tidy run CMAKE_BUILD_PARALLEL_LEVEL jobs, or half the cores when it isn't set.
#>
param(
    [ValidateSet('check', 'debug')]
    [string]$Preset = 'check',
    [switch]$Full
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot
. (Join-Path $PSScriptRoot 'msvc.ps1')
. (Join-Path $PSScriptRoot 'jobs.ps1')

$gameDataMagics = @('LJAM', 'ALP ')
$results = New-Object System.Collections.Generic.List[string]
$head = git rev-parse HEAD
$jobs = Get-JobCount
$buildDir = "build/$Preset"
$mode = if ($Full) { 'full' } else { 'incremental' }
$fullTidyTriggers = '^(\.clang-tidy$|CMakeLists\.txt$|CMakePresets\.json$|vcpkg\.json$|cmake/)'

function Write-Summary([string]$Outcome) {
    Write-Host ''
    Write-Host "Summary for $head ($Preset, $mode)"
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

function Invoke-ClangTidy([string]$ClangTidy, [string]$Database, [string[]]$Files) {
    $pool = [RunspaceFactory]::CreateRunspacePool(1, $jobs)
    $pool.Open()
    try {
        $runs = foreach ($file in $Files) {
            $shell = [PowerShell]::Create().AddScript({
                param($ClangTidy, $Database, $File)
                $output = @(& $ClangTidy -p $Database --quiet $File 2>&1 | ForEach-Object { "$_" })
                [pscustomobject]@{ Output = $output; ExitCode = $LASTEXITCODE }
            }).AddArgument($ClangTidy).AddArgument($Database).AddArgument($file)
            $shell.RunspacePool = $pool
            [pscustomobject]@{ Shell = $shell; Handle = $shell.BeginInvoke() }
        }
        foreach ($run in $runs) {
            $result = $run.Shell.EndInvoke($run.Handle)[0]
            $run.Shell.Dispose()
            $result.Output
            if ($result.ExitCode -ne 0) {
                $global:LASTEXITCODE = 1
            }
        }
    }
    finally {
        $pool.Dispose()
    }
}

function Get-ChangedFiles {
    git rev-parse --verify --quiet origin/main | Out-Null
    if ($LASTEXITCODE -ne 0) {
        return $null
    }
    $base = git merge-base HEAD origin/main
    return @(git -c core.quotepath=off diff --name-only $base HEAD)
}

function Get-DependentSources([string[]]$Headers, [string[]]$Deps) {
    $wanted = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    $Headers | ForEach-Object { [void]$wanted.Add([IO.Path]::GetFullPath((Join-Path $repoRoot $_))) }
    $buildPath = Join-Path $repoRoot $buildDir
    $object = $null
    foreach ($line in $Deps) {
        if ($line -match '^(\S+): #deps') {
            $object = $Matches[1]
            continue
        }
        $dependency = $line.Trim()
        if (-not $object -or -not $dependency) {
            continue
        }
        if ($wanted.Contains([IO.Path]::GetFullPath([IO.Path]::Combine($buildPath, $dependency)))) {
            $object -replace '^(.+)/CMakeFiles/[^/]+\.dir/(.+)\.obj$', '$1/$2'
            $object = $null
        }
    }
}

function Get-TidySources([string[]]$AllSources) {
    if ($Full) {
        return $AllSources
    }
    $changed = Get-ChangedFiles
    if ($null -eq $changed -or ($changed -match $fullTidyTriggers)) {
        return $AllSources
    }
    $headers = @($changed | Where-Object { $_ -like '*.hpp' })
    $dependents = @()
    if ($headers) {
        $deps = @(ninja -C $buildDir -t deps)
        if ($LASTEXITCODE -ne 0) {
            return $AllSources
        }
        $dependents = @(Get-DependentSources $headers $deps)
    }
    $candidates = @($changed) + $dependents
    return @($AllSources | Where-Object { $candidates -contains $_ })
}

function Write-TidyDatabase([string]$Folder) {
    $pchFlag = '\s+(\\")?/(Yu|Fp|FI)[^"]*?cmake_pch\.(hxx|cxx\.pch)(\\")?'
    $commands = [IO.File]::ReadAllText((Join-Path $repoRoot "$buildDir/compile_commands.json"))
    $stripped = $commands -replace $pchFlag, ''
    if ($stripped -match '/(Yu|Fp|FI)[^"]*?cmake_pch') {
        Fail 'Could not strip the precompiled header flags from compile_commands.json.'
        return
    }
    New-Item -ItemType Directory -Force $Folder | Out-Null
    [IO.File]::WriteAllText((Join-Path $Folder 'compile_commands.json'), $stripped)
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

Invoke-Step 'clang-format' {
    $clangFormat = Find-Tool 'clang-format'
    if (-not $clangFormat) {
        return
    }
    $files = Get-TrackedFiles @('*.cpp', '*.hpp')
    for ($first = 0; $first -lt $files.Count -and $LASTEXITCODE -eq 0; $first += 100) {
        $batch = $files[$first..([Math]::Min($first + 100, $files.Count) - 1)]
        try {
            & $clangFormat --dry-run --Werror $batch
        }
        catch {
            Fail "clang-format could not run: $_"
        }
    }
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
        $problem = Enter-MsvcEnvironment
        if ($problem) {
            Fail $problem
        }
    }
}

$configureStep = if ($Full) { "Configure into empty $buildDir" } else { "Configure $buildDir" }
Invoke-Step $configureStep {
    if ($Full) {
        Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
    }
    cmake --preset $Preset
}
Invoke-Step "Build ($Preset)" { cmake --build --preset $Preset --parallel $jobs }
Invoke-Step "Test ($Preset)" { ctest --preset $Preset }

$allSources = Get-TrackedFiles @('src/*.cpp', 'tests/*.cpp', 'tools/*.cpp')
$tidySources = @(Get-TidySources $allSources)
Invoke-Step ('clang-tidy ({0} of {1} sources)' -f $tidySources.Count, $allSources.Count) {
    $clangTidy = Find-Tool 'clang-tidy'
    if ($clangTidy) {
        $database = Join-Path $repoRoot "$buildDir/tidy"
        Write-TidyDatabase $database
        if ($LASTEXITCODE -eq 0) {
            Invoke-ClangTidy $clangTidy $database ($tidySources | ForEach-Object { Join-Path $repoRoot $_ })
        }
    }
}

Write-Summary 'All checks passed.'
