Param(
    [Parameter(Mandatory,
      HelpMessage="Root directory of extracted Terminal AppX")]
    [string[]]
    $TerminalRoot,

    [Parameter(Mandatory,
      HelpMessage="Root directory of extracted Xaml AppX")]
    [string[]]
    $XamlRoot,

    [Parameter(Mandatory,
      HelpMessage="Output Path")]
    [string]
    $OutputPath,

    [Parameter(HelpMessage="Path to makepri.exe")]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $MakePriPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\MakePri.exe"
)

$ErrorActionPreference = 'Stop'

$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "tmp$([Convert]::ToString((Get-Random 65535),16).PadLeft(4,'0')).tmp"
New-Item -ItemType Directory -Path $tempDir | Out-Null

$terminalDump = Join-Path $tempDir "terminal.pri.xml"

& $MakePriPath dump /if (Join-Path $TerminalRoot "resources.pri") /of $terminalDump /dt detailed

Write-Verbose "Removing Microsoft.UI.Xaml node from Terminal to prevent a collision with XAML"
$terminalXMLDocument = [xml](Get-Content $terminalDump)
$resourceMap = $terminalXMLDocument.PriInfo.ResourceMap
$fileSubtree = $resourceMap.ResourceMapSubtree | Where-Object { $_.Name -eq "Files" }
$subtrees = $fileSubtree.ResourceMapSubtree
$xamlSubtreeChild = ($subtrees | Where-Object { $_.Name -eq "Microsoft.UI.Xaml" })
if ($Null -Ne $xamlSubtreeChild) {
    $null = $fileSubtree.RemoveChild($xamlSubtreeChild)
    $terminalXMLDocument.Save($terminalDump)
}

$indexName = $terminalXMLDocument.PriInfo.ResourceMap.name

& (Join-Path $PSScriptRoot "Merge-PriFiles.ps1") -Path $terminalDump, (Join-Path $XamlRoot "resources.pri") -IndexName $indexName -OutputPath $OutputPath -MakePriPath $MakePriPath

Remove-Item -Recurse -Force $tempDir
