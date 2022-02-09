[CmdLetBinding()]
Param(
    [Parameter(Mandatory=$true, Position=0)][string]$MatchPattern,
    [Parameter(Mandatory=$true, Position=1)][string]$Platform,
    [Parameter(Mandatory=$true, Position=2)][string]$Configuration,
    [Parameter(Mandatory=$false, Position=3)][string]$LogPath,
    [Parameter(Mandatory=$false)][string]$Root = ".\bin\$Platform\$Configuration"
)

$testdlls = Get-ChildItem -Path "$Root" -Recurse -Filter $MatchPattern


$args = @();

if ($LogPath)
{
    $args += '/enablewttlogging';
    $args += '/appendwttlogging';
    $args += "/logFile:$LogPath";
    Write-Host "Wtt Logging Enabled";
}

&"$Root\te.exe" $args $testdlls.FullName

if ($lastexitcode -Ne 0) { Exit $lastexitcode }

Exit 0
