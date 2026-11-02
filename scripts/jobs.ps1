function Get-JobCount {
    <#
    .SYNOPSIS
    Returns CMAKE_BUILD_PARALLEL_LEVEL when it's a positive number, otherwise half the cores.
    #>
    $level = 0
    if ([int]::TryParse("$env:CMAKE_BUILD_PARALLEL_LEVEL", [ref]$level) -and $level -gt 0) {
        return $level
    }
    return [Math]::Max(1, [int][Math]::Floor([Environment]::ProcessorCount / 2))
}
