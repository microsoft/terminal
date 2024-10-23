Param(
    [Parameter(Mandatory,
      HelpMessage="Root Directory")]
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

$projectRoot = Join-Path $tempDir Root
New-Item -ItemType Directory -Path $projectRoot | Out-Null

Copy-Item $Path -Destination $projectRoot -Recurse -Verbose

@"
<?xml version="1.0" encoding="utf-8"?>
<resources targetOsVersion="10.0.0" majorVersion="1">
  <index root="\" startIndexAt=".">
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
    <indexer-config type="FOLDER" foldernameAsQualifier="true" filenameAsQualifier="true" qualifierDelimiter="." />
  </index>
</resources>
"@ | Out-File -Encoding:utf8NoBOM $priConfig

& $MakePriPath new /pr $projectRoot /cf $priConfig /o /in $IndexName /of $OutputPath

Remove-Item -Recurse -Force $tempDir
