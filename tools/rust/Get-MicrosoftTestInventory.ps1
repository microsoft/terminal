#Requires -Version 7
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string] $Path,

    [string] $Suite = 'unknown'
)

$ErrorActionPreference = 'Stop'
$resolved = (Resolve-Path $Path).Path
$pattern = '\bTEST_METHOD\s*\(\s*(?<name>[A-Za-z_][A-Za-z0-9_]*)\s*\)'

$inventory = foreach ($file in Get-ChildItem -Path $resolved -Recurse -File -Include *.cpp,*.cc,*.cxx,*.h,*.hpp) {
    $text = Get-Content -Raw -Path $file.FullName
    foreach ($match in [regex]::Matches($text, $pattern)) {
        [pscustomobject]@{
            suite = $Suite
            method = $match.Groups['name'].Value
            source = [IO.Path]::GetRelativePath($resolved, $file.FullName).Replace('\\', '/')
        }
    }
}

$inventory |
    Sort-Object source, method -Unique |
    ConvertTo-Json -Depth 3
