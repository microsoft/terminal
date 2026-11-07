<#
.SYNOPSIS
    Generates a plain-text version metadata sidecar file for a Windows Terminal
    release artifact.

.DESCRIPTION
    Writes a small key=value text file that lets callers (update checkers, CI
    scripts, end-users) discover the exact version of an artifact without having
    to parse the filename or extract the MSIX/ZIP.

    The companion download URL convention used by GitHub Releases is:
        https://github.com/microsoft/terminal/releases/latest/download/<artifact>

    Callers can send a HEAD request to that URL and follow the redirect to learn
    which release tag is current, or they can fetch this version.txt sidecar for
    the structured version string.

.PARAMETER PackageName
    The Windows package identity name, e.g. "Microsoft.WindowsTerminalCanary".

.PARAMETER Version
    The four-part version string read from the AppxManifest, e.g. "1.22.3141.0".

.PARAMETER Architecture
    The processor architecture string, e.g. "x64", "x86", or "arm64".

.PARAMETER OutputPath
    Full path where version.txt should be written.

.EXAMPLE
    .\New-VersionMetadataFile.ps1 `
        -PackageName  "Microsoft.WindowsTerminalCanary" `
        -Version      "1.22.3141.0" `
        -Architecture "x64" `
        -OutputPath   "_out\version.txt"
#>

[CmdletBinding()]
Param(
    [Parameter(Mandatory, HelpMessage="Windows package identity name")]
    [string]
    $PackageName,

    [Parameter(Mandatory, HelpMessage="Four-part version string from AppxManifest")]
    [string]
    $Version,

    [Parameter(Mandatory, HelpMessage="Processor architecture (x64, x86, arm64)")]
    [string]
    $Architecture,

    [Parameter(Mandatory, HelpMessage="Output path for the version.txt file")]
    [string]
    $OutputPath
)

$ErrorActionPreference = 'Stop'

# Ensure the destination directory exists.
$OutputDir = Split-Path -Parent $OutputPath
If ($OutputDir -And -Not (Test-Path $OutputDir)) {
    New-Item -ItemType Directory -Path $OutputDir | Out-Null
}

# Build the sidecar content.  The format is intentionally simple key=value so
# that it is easy to parse from PowerShell, bash, curl + grep, or any language.
$timestamp = (Get-Date -Format 'o')  # ISO 8601 / RFC 3339

$content = @"
# Windows Terminal release version metadata
# Generated: $timestamp
PackageName=$PackageName
Version=$Version
Architecture=$Architecture
"@

# Write as UTF-8 without BOM so the file is usable on all platforms.
[System.IO.File]::WriteAllText($OutputPath, $content, [System.Text.UTF8Encoding]::new($false))

Write-Verbose "Version metadata written to: $OutputPath"
Get-Item $OutputPath

