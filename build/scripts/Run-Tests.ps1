[CmdLetBinding()]
Param(
    [Parameter(Mandatory=$true, Position=0)]
    [string]$MatchPattern,
    [Parameter(Mandatory=$true, Position=1)]
    [string]$Platform,
    [Parameter(Mandatory=$true, Position=2)]
    [string]$Configuration,
    [Parameter(Mandatory=$false, Position=3)]
    [string]$LogPath,
    [Parameter(Mandatory=$false)]
    [string]$Root = ".\bin\$Platform\$Configuration",
    [string[]]$AdditionalTaefArguments
)

# Find test DLLs based on the provided root, match pattern, and recursion
$testDlls = Get-ChildItem -Path $Root -Recurse -Filter $MatchPattern

$teArgs = @()

# Check if the LogPath parameter is provided and enable WTT logging
if ($LogPath) {
    $teArgs += "/logFile:$LogPath"
    if ($LogPath.EndsWith('.trx')) {
        $teArgs += '/logger:trx'
        Write-Host "TRX Logging Enabled"
    } elseif ($LogPath.EndsWith('.wtl')) {
        $teArgs += '/enablewttlogging'
        $teArgs += '/appendwttlogging'
        Write-Host "WTT Logging Enabled"
    }
}

& "$Root\te.exe" $teArgs $testDlls.FullName $AdditionalTaefArguments
Exit $LASTEXITCODE
