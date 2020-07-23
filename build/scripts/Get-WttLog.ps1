[CmdLetBinding()]
Param(
    [Parameter(Mandatory=$true, Position=0)][string]$BuildPlatform,
	[Parameter(Mandatory=$true, Position=1)][string]$RationalizedPlatform,
	[Parameter(Mandatory=$true, Position=2)][string]$Configuration,
)


$i = Get-Item .\packages\MuxCustomBuild*
$wtt = Join-Path $i[0].FullName 'tools' $BuildPlatform 'wttlog.dll'
$dest = Join-Path .\bin $RationalizedPlatform $Configuration
copy $wtt $dest


Exit 0
