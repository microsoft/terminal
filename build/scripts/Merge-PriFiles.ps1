Param(
    [Parameter(Mandatory,
      HelpMessage="List of PRI files or XML dumps (detailed only) to merge")]
    [string[]]
    $Path,

    [Parameter(Mandatory,
      HelpMessage="Output Path")]
    [string]
    $OutputPath,

    [Parameter(HelpMessage="Name of index in output file; defaults to 'Application'")]
    [string]
    $IndexName = "Application",

    [Parameter(HelpMessage="Path to makepri.exe")]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $MakePriPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\MakePri.exe"
)

$ErrorActionPreference = 'Stop'

$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "tmp$([Convert]::ToString((Get-Random 65535),16).PadLeft(4,'0')).tmp"
New-Item -ItemType Directory -Path $tempDir | Out-Null
$priConfig = Join-Path $tempDir "priconfig.xml"
$priListFile = Join-Path $tempDir "pri.resfiles"
$dumpListFile = Join-Path $tempDir "dump.resfiles"

@"
<?xml version="1.0" encoding="utf-8"?>
<resources targetOsVersion="10.0.0" majorVersion="1">
  <index root="\" startIndexAt="dump.resfiles">
    <default>
      <qualifier name="Language" value="en-US" />
      <qualifier name="Contrast" value="standard" />
      <qualifier name="Scale" value="200" />
      <qualifier name="HomeRegion" value="001" />
      <qualifier name="TargetSize" value="256" />
      <qualifier name="LayoutDirection" value="LTR" />
      <qualifier name="DXFeatureLevel" value="DX9" />
      <qualifier name="Configuration" value="" />
      <qualifier name="AlternateForm" value="" />
      <qualifier name="Platform" value="UAP" />
    </default>
    <indexer-config type="PRIINFO" />
    <indexer-config type="RESFILES" qualifierDelimiter="." />
  </index>
  <index root="\" startIndexAt="pri.resfiles">
    <default>
      <qualifier name="Language" value="en-US" />
      <qualifier name="Contrast" value="standard" />
      <qualifier name="Scale" value="200" />
      <qualifier name="HomeRegion" value="001" />
      <qualifier name="TargetSize" value="256" />
      <qualifier name="LayoutDirection" value="LTR" />
      <qualifier name="DXFeatureLevel" value="DX9" />
      <qualifier name="Configuration" value="" />
      <qualifier name="AlternateForm" value="" />
      <qualifier name="Platform" value="UAP" />
    </default>
    <indexer-config type="PRI" />
    <indexer-config type="RESFILES" qualifierDelimiter="." />
  </index>
</resources>
"@ | Out-File -Encoding:utf8NoBOM $priConfig

$Path | Where { $_ -Like "*.pri" } | ForEach-Object {
    Get-Item $_ | Select -Expand FullName
} | Out-File -Encoding:utf8NoBOM $priListFile

$Path | Where { $_ -Like "*.xml" } | ForEach-Object {
    Get-Item $_ | Select -Expand FullName
} | Out-File -Encoding:utf8NoBOM $dumpListFile

& $MakePriPath new /pr $tempDir /cf $priConfig /o /in $IndexName /of $OutputPath

Remove-Item -Recurse -Force $tempDir
