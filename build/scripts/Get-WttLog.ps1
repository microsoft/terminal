[CmdLetBinding()]
Param(
    [Parameter(Mandatory=$true, Position=0)][string]$BuildPlatform,
    [Parameter(Mandatory=$true, Position=1)][string]$RationalizedPlatform,
    [Parameter(Mandatory=$true, Position=2)][string]$Configuration
)


$i = Get-Item .\packages\MuxCustomBuild*
$wtt = Join-Path -Path $i[0].FullName -ChildPath (Join-Path -Path 'tools' -ChildPath (Join-Path -Path $BuildPlatform -ChildPath 'wttlog.dll'))
$dest = Join-Path -Path .\bin -ChildPath (Join-Path -Path $RationalizedPlatform -ChildPath ($Configuration))
copy $wtt $dest


Exit 0
