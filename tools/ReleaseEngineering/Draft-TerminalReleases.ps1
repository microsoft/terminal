# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#################################
# Draft-TerminalReleases produces...

#Requires -Version 7.0

[CmdletBinding()]
Param(
    [string[]]$Directory
)

Enum AssetType {
	Unknown
	ApplicationBundle
	PreinstallKit
}

Enum Branding {
	Unknown
	Release
	Preview
	Dev
}

$script:tar = "bsdtar"

Class Asset {
	[string]$Path
	[string]$Name
	[Version]$Version

	[AssetType]$Type
	[Branding]$Branding
	[Version]$WindowsVersion

	hidden [xml]$Manifest

	Asset([string]$Path) {
		$this.Path = $Path;
	}

	[void]Load() {
		$local:bundlePath = $this.Path

		$local:sentinelFile = New-TemporaryFile
		$local:directory = New-Item -Type Directory "$($local:sentinelFile.FullName)_Package"
		Remove-Item $local:sentinelFile -Force -EA:Ignore

		$local:ext = [IO.Path]::GetExtension($this.Path)
		If (".zip" -eq $local:ext) {
			& $script:tar -x -f $this.Path -C $local:directory
			$local:bundlePath = Get-ChildItem $local:directory -Filter *.msixbundle | Select -First 1 -Expand FullName
			$this.Type = [AssetType]::PreinstallKit
		} ElseIf (".msixbundle" -eq $local:ext) {
			$this.Type = [AssetType]::ApplicationBundle
		}

		& $script:tar -x -f $local:bundlePath -C $local:directory
		$local:msixPath = Get-ChildItem $local:directory -Filter Cascadia*.msix | Select -First 1 -Expand FullName

		& $script:tar -x -v -f $local:msixPath -C $local:directory AppxManifest.xml

		$this.Manifest = [xml](Get-Content (Join-Path $local:directory AppxManifest.xml))

		$this.ParseManifest()
	}

	[void]ParseManifest() {
		$this.Name = $this.Manifest.Package.Identity.Name

		$this.Branding = Switch($this.Name) {
			"Microsoft.WindowsTerminal" { [Branding]::Release }
			"Microsoft.WindowsTerminalPreview" { [Branding]::Preview }
			"WindowsTerminalDev" { [Branding]::Dev }
			Default { [Branding]::Unknown }
		}

		$this.WindowsVersion = $this.Manifest.Package.Dependencies.TargetDeviceFamily | Where-Object Name -eq "Windows.Desktop" | Select -Expand MinVersion
		$this.Version = $this.Manifest.Package.Identity.Version
	}

	[string]IdealFilename() {
		$local:windowsVersionMoniker = Switch($this.WindowsVersion.Minor) {
			{ $_ -Lt 22000 } { "Win10" }
			{ $_ -Ge 22000 } { "Win11" }
		}

		$local:bundleName = "{0}_{1}_{2}_8wekyb3d8bbwe.msixbundle" -f ($this.Name, $local:windowsVersionMoniker, $this.Version)

		$local:filename = Switch($this.Type) {
			PreinstallKit {
				"{0}_Windows10_PreinstallKit.zip" -f $local:bundleName
			}
			ApplicationBundle {
				$local:bundleName
			}
			Default {
				Throw "Unknown type $($_.Type)"
			}
		}
		return $local:filename
	}

	static [Asset] CreateFromFile([string]$Path) {
		$a = [Asset]::new($Path)
		$a.Load()
		return $a
	}
}

# Get SHA1 from AppXManifest (no matter how deep)
#
# Create releases

# Collect Assets from $Directory, figure out what those assets are
$Assets = Get-ChildItem $Directory -Recurse -Include *.msixbundle, *.zip | ForEach-Object {
	[Asset]::CreateFromFile($_.FullName)
}

$Assets

# Line them up to Git Things
