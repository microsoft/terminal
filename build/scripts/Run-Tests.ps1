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

$args = @()

# Check if the LogPath parameter is provided and enable WTT logging
if ($LogPath) {
    $args += '/enablewttlogging'
    $args += '/appendwttlogging'
    $args += "/logFile:$LogPath"
    Write-Host "WTT Logging Enabled"
}

# Invoke the te.exe executable with arguments and test DLLs
& "$Root\te.exe" $args $testDlls.FullName $AdditionalTaefArguments

# Check the exit code of the te.exe process and exit accordingly
if ($LASTEXITCODE -ne 0) {
    Exit $LASTEXITCODE
}

Exit 0
