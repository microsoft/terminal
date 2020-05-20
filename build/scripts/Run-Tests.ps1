[CmdLetBinding()]
Param(
    [Parameter(Mandatory=$true, Position=0)][string]$MatchPattern,
    [Parameter(Mandatory=$true, Position=1)][string]$Platform,
    [Parameter(Mandatory=$true, Position=2)][string]$Configuration,
    [Parameter(Mandatory=$false, Position=3)][string]$SearchPath=".\bin\$Platform\$Configuration"
)

$testdlls = Get-ChildItem -Path $SearchPath -Recurse -Filter $MatchPattern

&".\bin\$Platform\$Configuration\te.exe" $testdlls.FullName

if ($lastexitcode -Ne 0) { Exit $lastexitcode }

Exit 0
