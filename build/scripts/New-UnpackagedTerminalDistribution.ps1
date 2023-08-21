[CmdletBinding(DefaultParameterSetName = 'AppX')]
Param(
    [Parameter(Mandatory, HelpMessage="Path to Terminal AppX", ParameterSetName = 'AppX')]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $TerminalAppX,

    [Parameter(Mandatory, HelpMessage="Path to Terminal Layout Deployment", ParameterSetName='Layout')]
    [ValidateScript({Test-Path $_ -Type Container})]
    [string]
    $TerminalLayout,

    [Parameter(Mandatory, HelpMessage="Path to Xaml AppX", ParameterSetName='AppX')]
    [Parameter(Mandatory, HelpMessage="Path to Xaml AppX", ParameterSetName='Layout')]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $XamlAppX,

    [Parameter(HelpMessage="Output Directory", ParameterSetName='AppX')]
    [Parameter(HelpMessage="Output Directory", ParameterSetName='Layout')]
    [string]
    $Destination = ".",

    [Parameter(HelpMessage="Path to makeappx.exe", ParameterSetName='AppX')]
    [Parameter(HelpMessage="Path to makeappx.exe", ParameterSetName='Layout')]
    [ValidateScript({Test-Path $_ -Type Leaf})]
    [string]
    $MakeAppxPath = "C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\MakeAppx.exe"
)

$filesToRemove = @("*.xml", "*.winmd", "Appx*", "Images/*Tile*", "Images/*Logo*") # Remove from Terminal
$filesToKeep = @("Microsoft.Terminal.Remoting.winmd") # ... except for these
$filesToCopyFromXaml = @("Microsoft.UI.Xaml.dll", "Microsoft.UI.Xaml") # We don't need the .winmd

$ErrorActionPreference = 'Stop'

If ($null -Eq (Get-Item $MakeAppxPath -EA:SilentlyContinue)) {
    Write-Error "Could not find MakeAppx.exe at `"$MakeAppxPath`".`nMake sure that -MakeAppxPath points to a valid SDK."
    Exit 1
}

$tempDir = Join-Path ([System.IO.Path]::GetTempPath()) "tmp$([Convert]::ToString((Get-Random 65535),16).PadLeft(4,'0')).tmp"
New-Item -ItemType Directory -Path $tempDir | Out-Null

$XamlAppX = Get-Item $XamlAppX | Select-Object -Expand FullName

########
# Reading the AppX Manifest for preliminary info
########

If ($TerminalAppX) {
	$appxManifestPath = Join-Path $tempDir AppxManifest.xml
	& tar.exe -x -f "$TerminalAppX" -C $tempDir AppxManifest.xml
} ElseIf($TerminalLayout) {
	$appxManifestPath = Join-Path $TerminalLayout AppxManifest.xml
}
$manifest = [xml](Get-Content $appxManifestPath)
$pfn = $manifest.Package.Identity.Name
$version = $manifest.Package.Identity.Version
$architecture = $manifest.Package.Identity.ProcessorArchitecture

$distributionName = "{0}_{1}_{2}" -f ($pfn, $version, $architecture)
$terminalDir = "terminal-{0}" -f ($version)

########
# Unpacking Terminal and XAML
########

$terminalAppPath = Join-Path $tempdir $terminalDir

If ($TerminalAppX) {
	$TerminalAppX = Get-Item $TerminalAppX | Select-Object -Expand FullName
	New-Item -ItemType Directory -Path $terminalAppPath | Out-Null
	& $MakeAppxPath unpack /p $TerminalAppX /d $terminalAppPath /o | Out-Null
	If ($LASTEXITCODE -Ne 0) {
	    Throw "Unpacking $TerminalAppX failed"
	}
} ElseIf ($TerminalLayout) {
	Copy-Item -Recurse -Path $TerminalLayout -Destination $terminalAppPath
}

$xamlAppPath = Join-Path $tempdir "xaml"
New-Item -ItemType Directory -Path $xamlAppPath | Out-Null
& $MakeAppxPath unpack /p $XamlAppX /d $xamlAppPath /o | Out-Null
If ($LASTEXITCODE -Ne 0) {
    Throw "Unpacking $XamlAppX failed"
}

########
# Some sanity checking
########

$xamlManifest = [xml](Get-Content (Join-Path $xamlAppPath "AppxManifest.xml"))
If ($xamlManifest.Package.Identity.Name -NotLike "Microsoft.UI.Xaml*") {
    Throw "$XamlAppX is not a XAML package (instead, it looks like $($xamlManifest.Package.Identity.Name))"
}
If ($xamlManifest.Package.Identity.ProcessorArchitecture -Ne $architecture) {
    Throw "$XamlAppX is not built for $architecture (instead, it is built for $($xamlManifest.Package.Identity.ProcessorArchitecture))"
}

########
# Preparation of source files
########

$itemsToRemove = $filesToRemove | ForEach-Object {
    Get-Item (Join-Path $terminalAppPath $_) -EA:SilentlyContinue | Where-Object {
        $filesToKeep -NotContains $_.Name
    }
} | Sort-Object FullName -Unique
$itemsToRemove | Remove-Item -Recurse

$filesToCopyFromXaml | ForEach-Object {
    Get-Item (Join-Path $xamlAppPath $_)
} | Copy-Item -Recurse -Destination $terminalAppPath

########
# Resource Management
########

$finalTerminalPriFile = Join-Path $terminalAppPath "resources.pri"
& (Join-Path $PSScriptRoot "Merge-TerminalAndXamlResources.ps1") `
    -TerminalRoot $terminalAppPath `
    -XamlRoot $xamlAppPath `
    -OutputPath $finalTerminalPriFile `
    -Verbose:$Verbose | Out-Host

########
# Packaging
########

If ($PSCmdlet.ParameterSetName -Eq "AppX") {
	# We only produce a ZIP when we're combining two AppX directories.
	New-Item -ItemType Directory -Path $Destination -ErrorAction:SilentlyContinue | Out-Null
	$outputZip = (Join-Path $Destination ("{0}.zip" -f ($distributionName)))
	& tar -c --format=zip -f $outputZip -C $tempDir $terminalDir
	Remove-Item -Recurse -Force $tempDir -EA:SilentlyContinue
	Get-Item $outputZip
} ElseIf ($PSCmdlet.ParameterSetName -Eq "Layout") {
	Get-Item $terminalAppPath
}
