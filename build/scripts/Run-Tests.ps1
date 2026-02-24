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
    $teArgs += '/enablewttlogging'
    $teArgs += '/appendwttlogging'
    $teArgs += "/logFile:$LogPath"
    Write-Host "WTT Logging Enabled"
}

$rootTe = "$Root\te.exe"

# Some of our test fixtures depend on resources.pri in the same folder as the .exe hosting them.
# Unfortunately, that means that we need to run the te.exe *next to* each test DLL we discover.
# This code establishes a mapping from te.exe to test DLL (or DLLs)
$testDllTaefGroups = $testDlls | % {
	$localTe = Get-Item (Join-Path (Split-Path $_ -Parent) "te.exe") -EA:Ignore
	If ($null -eq $localTe) {
		$finalTePath = $rootTe
	} Else {
		$finalTePath = $localTe.FullName
	}
	[PSCustomObject]@{
		TePath = $finalTePath;
		TestDll = $_;
	}
}

# Invoke the te.exe executables with arguments and test DLLs
$anyFailed = $false
$testDllTaefGroups | Group-Object TePath | % {
	$te = $_.Group[0].TePath
	$dlls = $_.Group.TestDll
	Write-Verbose "Running $te (for $($dlls.Name))"
	& $te $teArgs $dlls.FullName $AdditionalTaefArguments
	if ($LASTEXITCODE -ne 0) {
		$anyFailed = $true
	}
}

if ($anyFailed) {
    Exit 1
}

Exit 0
