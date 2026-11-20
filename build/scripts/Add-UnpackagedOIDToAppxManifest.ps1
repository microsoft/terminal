#.SYNOPSIS
# Modifies an AppX Manifest to allow the package to be installed with Add-AppxPackage -AllowUnsigned
# This requires us to change the publisher identity (for security reasons, so that unsigned packages
# do not get access to data belonging to their signed counterparts.)
# See https://learn.microsoft.com/windows/msix/package/unsigned-package
Param(
    [Parameter(Mandatory,
      HelpMessage="AppxManifest to patch")]
    [string]
    $Path,

    [Parameter(Mandatory,
      HelpMessage="Output Path")]
    [string]
    $OutputPath
)

$ErrorActionPreference = 'Stop'

$Manifest = [xml](Get-Content $Path)
$Manifest.Package.Identity.Publisher += ", OID.2.25.311729368913984317654407730594956997722=1"
$Manifest.Package.Properties.PublisherDisplayName += " (Unsigned)"

$writerSettings = [System.Xml.XmlWriterSettings]::new()
$writerSettings.NewLineChars = "`r`n"
$writerSettings.Indent = $true
$writerSettings.Encoding = [System.Text.UTF8Encoding]::new($false) # suppress the BOM
$writer = [System.Xml.XmlWriter]::Create([System.IO.Path]::Combine($PWD, $OutputPath), $writerSettings)
$Manifest.Save($writer)
