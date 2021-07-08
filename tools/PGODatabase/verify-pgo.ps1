Param(
  [Parameter(Mandatory = $true, Position = 1)]
  [string] $module)

if ( !( Test-Path $module ) )
{
    throw "File does not exist $module"
}

$vsPath = &(Join-Path ${env:ProgramFiles(x86)} "\Microsoft Visual Studio\Installer\vswhere.exe") -property installationpath
Import-Module (Get-ChildItem $vsPath -Recurse -File -Filter Microsoft.VisualStudio.DevShell.dll).FullName
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation

$output = ( & link.exe /dump /headers /coffgroup $module )

$regex1 = [regex] '^\s*[A-F0-9]+ coffgrp(\s+[A-F0-9]+){4} \(PGU\)$'
$regex2 = [regex] '^\s*([A-F0-9]+\s+){2}\.text\$zz$'

$matchFlags = 0

foreach ( $line in $output )
{
    if ( $line -match $regex1 )
    {
        $matchFlags = $matchFlags -bor 1
    }

    if ( $line -match $regex2 )
    {
        $matchFlags = $matchFlags -bor 2
    }
}

$optimized = $matchFlags -eq 3

$message = "$module is $( if ( $optimized ) { "optimized" } else { "not optimized" } )"

if ( -not $optimized )
{
    throw $message
}
else
{
    write-host $message
}