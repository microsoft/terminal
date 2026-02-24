[CmdletBinding()]
Param(
    [Parameter(Mandatory,
      HelpMessage="Path to the .msixbundle")]
    [string]
    $BundlePath,

    [Parameter(Mandatory,
      HelpMessage="Path to the .appinstaller template")]
    [string]
    $AppInstallerTemplatePath,

    [string]
    $AppInstallerRoot,

    [Parameter(Mandatory,
      HelpMessage="Output Path")]
    [string]
    $OutputPath
)

$ErrorActionPreference = "Stop"

$sentinelFile = New-TemporaryFile
$directory = New-Item -Type Directory "$($sentinelFile.FullName)_Package"
Remove-Item $sentinelFile -Force -EA:Ignore

$bundle = (Get-Item $BundlePath)
& tar.exe -x -f $bundle.FullName -C $directory AppxMetadata/AppxBundleManifest.xml

$xml = [xml](Get-Content (Join-Path $directory "AppxMetadata\AppxBundleManifest.xml"))
$name = $xml.Bundle.Identity.Name
$version = $xml.Bundle.Identity.Version

$doc = (Get-Content -ReadCount 0 $AppInstallerTemplatePath)
$doc = $doc -Replace '\$\$ROOT\$\$',$AppInstallerRoot
$doc = $doc -Replace '\$\$NAME\$\$',$name
$doc = $doc -Replace '\$\$VERSION\$\$',$version
$doc = $doc -Replace '\$\$PACKAGE\$\$',$bundle.Name
$doc | Out-File -Encoding utf8NoBOM $OutputPath

Get-Item $OutputPath
