# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#################################
# Draft-TerminalReleases produces...

#Requires -Version 7.0
#Requires -Modules PSGitHub

[CmdletBinding(SupportsShouldProcess)]
Param(
    [string[]]$Directory,
    [string]$Owner = "microsoft",
    [string]$RepositoryName = "terminal",
    [switch]$DumpOutput = $false
)

$ErrorActionPreference = 'Stop'

$PSDefaultParameterValues['*GitHub*:Owner'] = $Owner
$PSDefaultParameterValues['*GitHub*:RepositoryName'] = $RepositoryName

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

$script:tar = "tar"

Class Asset {
	[string]$Name
	[Version]$Version

	[AssetType]$Type
	[Branding]$Branding
	[Version]$WindowsVersion

	[string]$Path

	Asset([string]$Path) {
		$this.Path = $Path;
	}

	[void]Load() {
		$local:bundlePath = $this.Path

		$local:sentinelFile = New-TemporaryFile -Confirm:$false -WhatIf:$false
		$local:directory = New-Item -Type Directory "$($local:sentinelFile.FullName)_Package" -Confirm:$false -WhatIf:$false
		Remove-Item $local:sentinelFile -Force -EA:Ignore -Confirm:$false -WhatIf:$false

		$local:ext = [IO.Path]::GetExtension($this.Path)
		If (".zip" -eq $local:ext) {
			Write-Verbose "Cracking Preinstall Kit $($this.Path)"
			$local:licenseFile = & $script:tar -t -f $this.Path | Select-String "_License1.xml"
			If (-Not $local:licenseFile) {
				Throw ("$($this.Path) does not appear to be a preinstall kit")
			}
			$local:bundleName = ($local:licenseFile -Split "_")[0] + ".msixbundle"
			Write-Verbose "Found inner bundle $($local:bundleName)"
			& $script:tar -x -f $this.Path -C $local:directory $local:bundleName

			$local:bundlePath = Join-Path $local:directory $local:bundleName
			$this.Type = [AssetType]::PreinstallKit
		} ElseIf (".msixbundle" -eq $local:ext) {
			$this.Type = [AssetType]::ApplicationBundle
		}

		Write-Verbose "Cracking bundle $($local:bundlePath)"
		$local:firstMsixName = & $script:tar -t -f $local:bundlePath |
			Select-String 'Casc.*\.msix' |
			Select-Object -First 1 -Expand Line
		& $script:tar -x -f $local:bundlePath -C $local:directory $local:firstMsixName

		Write-Verbose "Found inner msix $($local:firstMsixName)"
		$local:msixPath = Join-Path $local:directory $local:firstMsixName
		& $script:tar -x -v -f $local:msixPath -C $local:directory AppxManifest.xml

		Write-Verbose "Parsing AppxManifest.xml"
		$local:Manifest = [xml](Get-Content (Join-Path $local:directory AppxManifest.xml))
		$this.ParseManifest($local:Manifest)
	}

	[void]ParseManifest([xml]$Manifest) {
		$this.Name = $Manifest.Package.Identity.Name

		$this.Branding = Switch($this.Name) {
			"Microsoft.WindowsTerminal" { [Branding]::Release }
			"Microsoft.WindowsTerminalPreview" { [Branding]::Preview }
			"WindowsTerminalDev" { [Branding]::Dev }
			Default { [Branding]::Unknown }
		}

		$this.WindowsVersion = $Manifest.Package.Dependencies.TargetDeviceFamily |
			Where-Object Name -eq "Windows.Desktop" |
			Select-Object -Expand MinVersion
		$this.Version = $Manifest.Package.Identity.Version
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

class Release {
	[string]$Name
	[Branding]$Branding
	[Version]$TagVersion
	[Version]$DisplayVersion
	[string]$Branch

	[Asset[]]$Assets

	Release([Asset[]]$a) {
		$this.Assets = $a
		$this.Branding = $a[0].Branding
		$this.Name = Switch($this.Branding) {
			Release { "Windows Terminal" }
			Preview { "Windows Terminal Preview" }
			Default { throw "Unknown Branding $_" }
		}
		$local:minVer = $a.Version | Measure -Minimum | Select-Object -Expand Minimum
		$this.TagVersion = $local:minVer
		$this.DisplayVersion = [Version]::new($local:minVer.Major, $local:minVer.Minor, $local:minVer.Build / 10)
		$this.Branch = "release-{0}.{1}" -f ($local:minVer.Major, $local:minVer.Minor)
	}

	[string]TagName() {
		return "v{0}" -f $this.TagVersion
	}
}

enum TaggingType {
	TagAlreadyExists
	TagBranchLatest
	TagCommitSha1
}

class ReleaseConfig {
	[Release]$Release
	[TaggingType]$TaggingType
	[string]$TagTarget # may be null
}

Function Read-ReleaseConfigFromHost([Release]$Release) {
	$choices = @(
		[Host.ChoiceDescription]::new("No New &Tag", "Tag already exists, do not create a new tag"),
		[Host.ChoiceDescription]::new("Tag &Branch"),
		[Host.ChoiceDescription]::new("Tag &Commit")
	)
	
	$existingTagSha1 = & git rev-parse --verify $Release.TagName() 2>$null
	If($existingTagSha1) {
		### If there is already a tag, trust the release engineer.
		Write-Verbose "Found existing tag $($Release.TagName())"
		return [ReleaseConfig]@{
			Release = $Release;
			TaggingType = [TaggingType]::TagAlreadyExists;
			TagTarget = $null;
		}
	}

	$choice = $Host.UI.PromptForChoice("How should I tag $("{0} v{1}" -f ($Release.Name,$Release.DisplayVersion))?", $null, $choices, 0)
	$target = $null
	Switch($choice) {
		0 { }
		1 { $target = $Release.Branch }
		2 { $target = Read-Host "What commit corresponds to $($Release.DisplayVersion)?" }
	}

	Return [ReleaseConfig]@{
		Release = $Release;
		TaggingType = [TaggingType]$choice;
		TagTarget = $target;
	}
}

# Get SHA1 from AppXManifest (no matter how deep)
#
# Create releases

# Collect Assets from $Directory, figure out what those assets are
$Assets = Get-ChildItem $Directory -Recurse -Include *.msixbundle, *.zip | ForEach-Object {
	[Asset]::CreateFromFile($_.FullName)
}

$Releases = $Assets | Group Branding | ForEach-Object {
	[Release]::new($_.Group)
}

$ReleaseConfigs = $Releases | % { Read-ReleaseConfigFromHost $_ }

If($DumpOutput) {
	$ReleaseConfigs
	Return
}

ForEach($c in $ReleaseConfigs) {
	$releaseName = "{0} v{1}" -f ($c.Release.Name, $c.Release.DisplayVersion)
	if($PSCmdlet.ShouldProcess("Release $releaseName", "Publish")) {
		Write-Verbose "Preparing release $releaseName"

		$releaseParams = @{
			TagName = $c.Release.TagName();
			Name = $releaseName;
		}

		If($c.TagTarget) {
			Switch($c.TaggingType) {
				[TagCommitSha1] { $releaseParams.CommitSHA = $c.TagTarget }
				[TagBranchLatest] { $releaseParams.Branch = $c.Release.Branch }
			}
			Write-Verbose " - Will create tag $($c.Release.TagName()) to point to $($c.TagTarget)"
		} Else {
			Write-Verbose " - Tag already exists"
		}

		$releaseParams.PreRelease = Switch($c.Release.Branding) {
			Release { $false }
			Preview { $true }
		}

		$GitHubRelease = New-GitHubRelease @releaseParams -Draft:$true

		ForEach($a in $c.Release.Assets) {
			Write-Verbose "Uploading $($a.Path) to Release $($GitHubRelease.id)"
			New-GitHubReleaseAsset -UploadUrl $GitHubRelease.UploadUrl -Path $a.Path -Label $a.IdealFilename()
		}
	}
}
