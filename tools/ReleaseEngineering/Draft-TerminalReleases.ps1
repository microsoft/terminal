# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#################################
# Draft-TerminalReleases takes a directory full of Terminal build outputs:
# - zip files
# - preinstallation kits
# - MSIX bundles
# and produces tagged releases with hashes and assets on GitHub.
# It automatically cracks files to get their version numbers, arranges them into branding categories,
# and publishes drafts based on their contents.

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
	Zip
}

Enum Branding {
	Unknown
	Release
	Preview
	Canary
	Dev
}

$script:tar = "tar"

Class Asset {
	[string]$Name
	[Version]$Version
	[string]$ExpandedVersion

	[AssetType]$Type
	[Branding]$Branding

	[string]$Architecture

	[string]$Path

	[string]$VersionMoniker

	Asset([string]$Path) {
		$this.Path = $Path;
	}

	[void]Load() {
		$local:bundlePath = $this.Path

		$local:sentinelFile = New-TemporaryFile -Confirm:$false -WhatIf:$false
		$local:directory = New-Item -Type Directory "$($local:sentinelFile.FullName)_Package" -Confirm:$false -WhatIf:$false
		Remove-Item $local:sentinelFile -Force -EA:Ignore -Confirm:$false -WhatIf:$false

		$local:ext = [IO.Path]::GetExtension($this.Path)
		$local:filename = [IO.Path]::GetFileName($this.Path)
		If (".zip" -eq $local:ext -and $local:filename -like '*Preinstall*') {
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
			$this.Architecture = "all"
		} ElseIf (".zip" -eq $local:ext) {
			$this.Type = [AssetType]::Zip
		} ElseIf (".msixbundle" -eq $local:ext) {
			$this.Type = [AssetType]::ApplicationBundle
			$this.Architecture = "all"
		}

		If ($this.Type -Ne [AssetType]::Zip) {
			Write-Verbose "Cracking bundle $($local:bundlePath)"
			$local:firstMsixName = & $script:tar -t -f $local:bundlePath |
				Select-String 'Cascadia.*\.msix' |
				Select-Object -First 1 -Expand Line
			& $script:tar -x -f $local:bundlePath -C $local:directory $local:firstMsixName

			Write-Verbose "Found inner msix $($local:firstMsixName)"
			$local:msixPath = Join-Path $local:directory $local:firstMsixName
			& $script:tar -x -v -f $local:msixPath -C $local:directory AppxManifest.xml

			Write-Verbose "Parsing AppxManifest.xml"
			$local:Manifest = [xml](Get-Content (Join-Path $local:directory AppxManifest.xml))
			$this.ParseManifest($local:Manifest)
		} Else {
			& $script:tar -x -f $this.Path -C $local:directory --strip-components=1 '*/wt.exe'
			$this.ExpandedVersion = (Get-Item (Join-Path $local:directory wt.exe)).VersionInfo.ProductVersion

			# Zip files just encode everything in their filename. Not great, but workable.
			$this.ParseFilename($local:filename)
		}

		$v = [version]$this.Version
		$this.VersionMoniker = "{0}.{1}" -f ($v.Major, $v.Minor)

		$this.Branding = Switch($this.Name) {
			"Microsoft.WindowsTerminal" { [Branding]::Release }
			"Microsoft.WindowsTerminalPreview" { [Branding]::Preview }
			"Microsoft.WindowsTerminalCanary" { [Branding]::Canary }
			"WindowsTerminalDev" { [Branding]::Dev }
			Default { [Branding]::Unknown }
		}
	}

	[void]ParseManifest([xml]$Manifest) {
		$this.Name = $Manifest.Package.Identity.Name
		$this.Version = $Manifest.Package.Identity.Version
	}

	[void]ParseFilename([string]$filename) {
		$parts = [IO.Path]::GetFileNameWithoutExtension($filename).Split("_")
		$this.Name = $parts[0]
		$this.Version = $parts[1]
		$this.Architecture = $parts[2]
	}

	[string]IdealFilename() {
		$local:bundleName = "{0}_{1}_8wekyb3d8bbwe.msixbundle" -f ($this.Name, $this.Version)

		$local:filename = Switch($this.Type) {
			PreinstallKit {
				"{0}_Windows10_PreinstallKit.zip" -f $local:bundleName
			}
			ApplicationBundle {
				$local:bundleName
			}
			Zip {
				"{0}_{1}_{2}.zip" -f ($this.Name, $this.Version, $this.Architecture)
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
			Default { throw "Unknown Branding for release publication $_" }
		}
		$local:minVer = $a.Version | Measure -Minimum | Select-Object -Expand Minimum
		$this.TagVersion = $local:minVer
		$this.DisplayVersion = $local:minVer
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
		[System.Management.Automation.Host.ChoiceDescription]::new("No New &Tag", "Tag already exists, do not create a new tag"),
		[System.Management.Automation.Host.ChoiceDescription]::new("Tag &Branch"),
		[System.Management.Automation.Host.ChoiceDescription]::new("Tag &Commit")
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

Function New-ReleaseBody([Release]$Release) {
	$zipAssetVersion = $Release.Assets.ExpandedVersion | ? { $_.Length -Gt 0 } | Select -First 1
	$body = "---`n`n"
	If (-Not [String]::IsNullOrEmpty($zipAssetVersion)) {
		$body += "_Binary files inside the unpackaged distribution archive bear the version number ``$zipAssetVersion``._`n`n"
	}
	$body += "### Asset Hashes`n`n";
	ForEach($a in $Release.Assets) {
		$body += "- {0}`n   - SHA256 ``{1}```n" -f ($a.IdealFilename(), (Get-FileHash $a.Path -Algorithm SHA256 | Select-Object -Expand Hash))
	}
	Return $body
}

# Collect Assets from $Directory, figure out what those assets are
$Assets = Get-ChildItem $Directory -Recurse -Include *.msixbundle, *.zip | ForEach-Object {
	[Asset]::CreateFromFile($_.FullName)
}

$Releases = $Assets | Group VersionMoniker | ForEach-Object {
	[Release]::new($_.Group)
}

$ReleaseConfigs = $Releases | % { Read-ReleaseConfigFromHost $_ }

If($DumpOutput) {
	$ReleaseConfigs
	Return
}

$sentinelFile = New-TemporaryFile -Confirm:$false -WhatIf:$false
$directory = New-Item -Type Directory "$($sentinelFile.FullName)_PackageUploads" -Confirm:$false -WhatIf:$false
Remove-Item $sentinelFile -Force -EA:Ignore -Confirm:$false -WhatIf:$false

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

		$releaseParams.Body = New-ReleaseBody $c.Release

		$GitHubRelease = New-GitHubRelease @releaseParams -Draft:$true

		ForEach($a in $c.Release.Assets) {
			$idealPath = (Join-Path $directory $a.IdealFilename())
			Copy-Item $a.Path $idealPath -Confirm:$false
			Write-Verbose "Uploading $idealPath to Release $($GitHubRelease.id)"
			New-GitHubReleaseAsset -ReleaseId $GitHubRelease.id -Path $idealPath
		}
	}
}
