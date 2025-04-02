# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

<#
.SYNOPSIS
    Draft-TerminalReleases processes a directory of Terminal build outputs (zip files, preinstallation kits, MSIX bundles)
    to generate tagged GitHub release drafts with appropriate assets and hashes.

.DESCRIPTION
    This script:
      - Cracks bundles to extract version numbers.
      - Categorizes builds into branding groups.
      - Generates a draft release with asset hashes.
      - Publishes the release as a draft on GitHub.
      
.PARAMETER Directory
    The directory or directories containing build outputs.
    
.PARAMETER Owner
    The GitHub owner. Default is "microsoft".
    
.PARAMETER RepositoryName
    The GitHub repository name. Default is "terminal".
    
.PARAMETER DumpOutput
    If specified, the script dumps the release configuration objects without publishing.
    
.EXAMPLE
    .\Draft-TerminalReleases.ps1 -Directory "C:\builds" -DumpOutput
#>

#Requires -Version 7.0
#Requires -Modules PSGitHub

[CmdletBinding(SupportsShouldProcess)]
Param(
    [Parameter(Mandatory, ValueFromPipeline)]
    [ValidateNotNullOrEmpty()]
    [string[]]$Directory,

    [Parameter()]
    [string]$Owner = "microsoft",

    [Parameter()]
    [string]$RepositoryName = "terminal",

    [Parameter()]
    [switch]$DumpOutput = $false
)

$ErrorActionPreference = 'Stop'

# Setup default GitHub parameter values
$PSDefaultParameterValues['*GitHub*:Owner'] = $Owner
$PSDefaultParameterValues['*GitHub*:RepositoryName'] = $RepositoryName

#======================================================================
#region Enums
#======================================================================
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

Enum TaggingType {
    TagAlreadyExists
    TagBranchLatest
    TagCommitSha1
}
#endregion

#======================================================================
#region Global Variables
#======================================================================
$script:tar = "tar"
#endregion

#======================================================================
#region Classes
#======================================================================

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
        $this.Path = $Path
    }

    [void]Load() {
        # Create a temporary directory for extraction.
        $local:sentinelFile = New-TemporaryFile -Confirm:$false
        $local:directory = New-Item -Type Directory ("{0}_Package" -f $local:sentinelFile.FullName) -Confirm:$false
        Remove-Item $local:sentinelFile -Force -EA:Ignore

        $local:ext = [IO.Path]::GetExtension($this.Path)
        $local:filename = [IO.Path]::GetFileName($this.Path)

        # Determine asset type based on file extension and name pattern.
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

        # For non-zip types, extract and parse the inner bundle.
        If ($this.Type -Ne [AssetType]::Zip) {
            Write-Verbose "Cracking bundle $($local:bundlePath)"
            $local:firstMsixName = & $script:tar -t -f $local:bundlePath |
                Select-String 'Cascadia.*\.msix' |
                Select-Object -First 1 -ExpandProperty Line
            & $script:tar -x -f $local:bundlePath -C $local:directory $local:firstMsixName

            Write-Verbose "Found inner msix $($local:firstMsixName)"
            $local:msixPath = Join-Path $local:directory $local:firstMsixName
            & $script:tar -x -v -f $local:msixPath -C $local:directory AppxManifest.xml

            Write-Verbose "Parsing AppxManifest.xml"
            $local:Manifest = [xml](Get-Content (Join-Path $local:directory AppxManifest.xml))
            $this.ParseManifest($local:Manifest)
        } Else {
            # For zip files, use the filename to encode version details.
            & $script:tar -x -f $this.Path -C $local:directory --strip-components=1 '*/wt.exe'
            $this.ExpandedVersion = (Get-Item (Join-Path $local:directory wt.exe)).VersionInfo.ProductVersion
            $this.ParseFilename($local:filename)
        }

        # Create a version moniker in the form "Major.Minor"
        $v = [version]$this.Version
        $this.VersionMoniker = "{0}.{1}" -f $v.Major, $v.Minor

        # Determine branding based on the asset name.
        $this.Branding = Switch ($this.Name) {
            "Microsoft.WindowsTerminal"         { [Branding]::Release }
            "Microsoft.WindowsTerminalPreview"    { [Branding]::Preview }
            "Microsoft.WindowsTerminalCanary"       { [Branding]::Canary }
            "WindowsTerminalDev"                  { [Branding]::Dev }
            Default                               { [Branding]::Unknown }
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
        $local:bundleName = "{0}_{1}_8wekyb3d8bbwe.msixbundle" -f $this.Name, $this.Version
        $local:filename = Switch ($this.Type) {
            [AssetType]::PreinstallKit { "{0}_Windows10_PreinstallKit.zip" -f $local:bundleName }
            [AssetType]::ApplicationBundle { $local:bundleName }
            [AssetType]::Zip           { "{0}_{1}_{2}.zip" -f $this.Name, $this.Version, $this.Architecture }
            Default                   { Throw "Unknown type $($this.Type)" }
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
        $this.Name = Switch ($this.Branding) {
            [Branding]::Release   { "Windows Terminal" }
            [Branding]::Preview   { "Windows Terminal Preview" }
            Default               { throw "Unknown Branding for release publication: $($this.Branding)" }
        }
        $local:minVer = $a.Version | Measure -Minimum | Select-Object -ExpandProperty Minimum
        $this.TagVersion = $local:minVer
        $this.DisplayVersion = $local:minVer
        $this.Branch = "release-{0}.{1}" -f $local:minVer.Major, $local:minVer.Minor
    }

    [string]TagName() {
        return "v{0}" -f $this.TagVersion
    }
}

class ReleaseConfig {
    [Release]$Release
    [TaggingType]$TaggingType
    [string]$TagTarget  # May be null
}
#endregion

#======================================================================
#region Functions
#======================================================================

Function Read-ReleaseConfigFromHost([Release]$Release) {
    $choices = @(
        [System.Management.Automation.Host.ChoiceDescription]::new("No New &Tag", "Tag already exists, do not create a new tag"),
        [System.Management.Automation.Host.ChoiceDescription]::new("Tag &Branch"),
        [System.Management.Automation.Host.ChoiceDescription]::new("Tag &Commit")
    )
    
    $existingTagSha1 = & git rev-parse --verify $Release.TagName() 2>$null
    If ($existingTagSha1) {
        Write-Verbose "Found existing tag $($Release.TagName())"
        return [ReleaseConfig]@{
            Release    = $Release
            TaggingType = [TaggingType]::TagAlreadyExists
            TagTarget   = $null
        }
    }

    $choice = $Host.UI.PromptForChoice("How should I tag $("{0} v{1}" -f $Release.Name, $Release.DisplayVersion)?", $null, $choices, 0)
    $target = $null
    Switch ($choice) {
        0 { } # No new tag needed.
        1 { $target = $Release.Branch }
        2 { $target = Read-Host "What commit corresponds to $($Release.DisplayVersion)?" }
    }

    return [ReleaseConfig]@{
        Release    = $Release
        TaggingType = [TaggingType]$choice
        TagTarget   = $target
    }
}

Function New-ReleaseBody([Release]$Release) {
    # Retrieve the version from the first asset with a non-empty ExpandedVersion.
    $zipAssetVersion = $Release.Assets.ExpandedVersion | Where-Object { $_.Length -gt 0 } | Select-Object -First 1
    $body = "---`n`n"
    If (-Not [String]::IsNullOrEmpty($zipAssetVersion)) {
        $body += "_Binary files inside the unpackaged distribution archive bear the version number ``$zipAssetVersion``._`n`n"
    }
    $body += "### Asset Hashes`n`n"
    ForEach ($a in $Release.Assets) {
        $hash = (Get-FileHash $a.Path -Algorithm SHA256 | Select-Object -ExpandProperty Hash)
        $body += "- {0}`n   - SHA256 ``{1}```n" -f ($a.IdealFilename()), $hash
    }
    return $body
}
#endregion

#======================================================================
#region Main Script Logic
#======================================================================

# Collect Assets from the provided directory (searching recursively for *.msixbundle and *.zip files)
$Assets = Get-ChildItem -Path $Directory -Recurse -Include *.msixbundle, *.zip | ForEach-Object {
    [Asset]::CreateFromFile($_.FullName)
}

# Group assets by their version moniker (Major.Minor) and create Release objects.
$Releases = $Assets | Group VersionMoniker | ForEach-Object {
    [Release]::new($_.Group)
}

$ReleaseConfigs = $Releases | ForEach-Object { Read-ReleaseConfigFromHost $_ }

If ($DumpOutput) {
    $ReleaseConfigs
    Return
}

# Prepare a temporary directory for uploading packages.
$sentinelFile = New-TemporaryFile -Confirm:$false
$uploadDirectory = New-Item -Type Directory ("{0}_PackageUploads" -f $sentinelFile.FullName) -Confirm:$false
Remove-Item $sentinelFile -Force -EA:Ignore

# Publish each release as a draft on GitHub.
ForEach ($c in $ReleaseConfigs) {
    $releaseName = "{0} v{1}" -f $c.Release.Name, $c.Release.DisplayVersion
    if ($PSCmdlet.ShouldProcess("Release $releaseName", "Publish")) {
        Write-Verbose "Preparing release $releaseName"

        $releaseParams = @{
            TagName = $c.Release.TagName()
            Name    = $releaseName
        }

        If ($c.TagTarget) {
            Switch ($c.TaggingType) {
                [TaggingType]::TagCommitSha1 { $releaseParams.CommitSHA = $c.TagTarget }
                [TaggingType]::TagBranchLatest  { $releaseParams.Branch = $c.Release.Branch }
            }
            Write-Verbose " - Will create tag $($c.Release.TagName()) to point to $($c.TagTarget)"
        }
        Else {
            Write-Verbose " - Tag already exists"
        }

        $releaseParams.PreRelease = Switch ($c.Release.Branding) {
            [Branding]::Release { $false }
            [Branding]::Preview { $true }
            Default           { $false }
        }

        $releaseParams.Body = New-ReleaseBody $c.Release

        $GitHubRelease = New-GitHubRelease @releaseParams -Draft:$true

        ForEach ($a in $c.Release.Assets) {
            $idealPath = Join-Path $uploadDirectory $a.IdealFilename()
            Copy-Item $a.Path $idealPath -Confirm:$false
            Write-Verbose "Uploading $idealPath to Release $($GitHubRelease.id)"
            New-GitHubReleaseAsset -ReleaseId $GitHubRelease.id -Path $idealPath
        }
    }
}
#endregion
