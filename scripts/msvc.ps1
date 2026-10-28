function Enter-MsvcEnvironment {
    <#
    .SYNOPSIS
    Loads the x64 MSVC environment unless cl.exe is on PATH. Returns why it couldn't, or nothing.
    #>
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        return 'vswhere.exe not found. Install Visual Studio 2022 Build Tools.'
    }
    $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if (-not $installPath) {
        return 'MSVC x64 build tools not found. Install Visual Studio 2022 Build Tools.'
    }
    $vcvars = Join-Path $installPath 'VC\Auxiliary\Build\vcvars64.bat'
    cmd /c "`"$vcvars`" >nul 2>&1 && set" | ForEach-Object {
        if ($_ -match '^([^=]+)=(.*)$') {
            Set-Item "env:$($Matches[1])" $Matches[2]
        }
    }
}
