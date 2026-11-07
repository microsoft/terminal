---
author: Community Contribution
created on: 2026-11-07
last updated: 2026-11-07
issue id: 20902
---

# Uniform Release Artifact Naming (without versioning in filenames)

## Abstract

This spec describes a change to the Windows Terminal release pipeline so that
published artifact filenames are **stable across releases** (i.e. the version
number is no longer embedded in the filename).  A small `version.txt` sidecar
file is attached to every release to let callers discover the exact version
without parsing filenames or extracting archives.

## Inspiration

Downstream tools and scripts that consume Terminal artifacts (CI jobs, update
checkers, winget manifests helpers, shell one-liners) today must discover the
current version before they can construct the download URL, because the filename
embeds the version:

```
Microsoft.WindowsTerminalCanary_1.22.3141.0_x64.zip
```

GitHub Releases already has a **stable redirect URL** mechanism:

```
https://github.com/microsoft/terminal/releases/latest/download/<filename>
```

But this only works if `<filename>` is constant.  When the version is baked in,
callers must first scrape the latest release tag — an extra HTTP round-trip
that is fragile and GitHub-API-rate-limited.

## Solution Design

### Artifact filename changes

| Artifact | Old name | New stable name |
|---|---|---|
| Unpackaged x64 ZIP | `Microsoft.WindowsTerminalCanary_1.22.0_x64.zip` | `Microsoft.WindowsTerminalCanary_x64.zip` |
| Unpackaged x86 ZIP | `Microsoft.WindowsTerminalCanary_1.22.0_x86.zip` | `Microsoft.WindowsTerminalCanary_x86.zip` |
| Unpackaged arm64 ZIP | `Microsoft.WindowsTerminalCanary_1.22.0_arm64.zip` | `Microsoft.WindowsTerminalCanary_arm64.zip` |
| MSIX bundle | `Microsoft.WindowsTerminalCanary_1.22.0_8wekyb3d8bbwe.msixbundle` | *(unchanged — required by MSIX install protocol)* |
| GPO templates ZIP | `GroupPolicyTemplates_1.22.0.zip` | `GroupPolicyTemplates.zip` |

The `.msixbundle` filename must retain the version because the Windows package
manager validates it against the bundle manifest; this is an OS constraint, not
a build constraint.

### version.txt sidecar

A plain UTF-8 text file is published alongside each artifact:

```
# Windows Terminal release version metadata
# Generated: 2026-11-07T17:30:00.0000000+00:00
PackageName=Microsoft.WindowsTerminalCanary
Version=1.22.3141.0
Architecture=x64
```

The format is intentionally `key=value` so it can be consumed from PowerShell,
bash, Python, `curl | grep`, or any language without a parser library.

For the MSIX bundle, a similar `version.txt` is written (without an
`Architecture` field, since the bundle is multi-arch).

### Discovering the latest version

**Option A — HTTP HEAD redirect** (no download, works from any HTTP client):

```powershell
$resp = Invoke-WebRequest -Method Head `
    "https://github.com/microsoft/terminal/releases/latest/download/Microsoft.WindowsTerminalCanary_x64.zip" `
    -MaximumRedirection 0 -ErrorAction Ignore
# $resp.Headers.Location contains the versioned URL, e.g.:
# https://github.com/microsoft/terminal/releases/download/v1.22.3141.0/Microsoft.WindowsTerminalCanary_x64.zip
$tag = ([uri]$resp.Headers.Location).Segments[-2].TrimEnd('/')
```

**Option B — version.txt sidecar** (structured, cacheable):

```powershell
$meta = (Invoke-RestMethod `
    "https://github.com/microsoft/terminal/releases/latest/download/version.txt") -Split "`n" |
  Where-Object { $_ -match '=' } |
  ForEach-Object { $k,$v=$_ -split '=',2; [pscustomobject]@{Key=$k;Value=$v} }
$version = ($meta | Where-Object Key -eq 'Version').Value
```

**Option C — always-current download** (simplest, for scripts that just need
the bits and don't need the version):

```
https://github.com/microsoft/terminal/releases/latest/download/Microsoft.WindowsTerminalCanary_x64.zip
```

## Implementation

### New files

- **`build/scripts/New-VersionMetadataFile.ps1`** — standalone helper that
  writes `version.txt`; called by `New-UnpackagedTerminalDistribution.ps1`.

### Modified files

| File | Change |
|---|---|
| `build/scripts/New-UnpackagedTerminalDistribution.ps1` | Add `-UniformNaming` switch; always call `New-VersionMetadataFile.ps1` |
| `build/pipelines/templates-v2/job-build-project.yml` | Pass `-UniformNaming` to the distribution script |
| `build/pipelines/templates-v2/job-merge-msix-into-bundle.yml` | Use `GroupPolicyTemplates.zip`; write `version.txt` for the bundle |
| `build/pipelines/templates-v2/job-deploy-to-azure-storage.yml` | Download `version.txt` sidecars; simplify rename logic |

## UI/UX Design

This is a pure build/release-pipeline change.  End users are unaffected unless
they were hardcoding versioned URLs — in which case the new stable URLs are
easier to use.

## Capabilities

### Accessibility
No impact.

### Security
No impact.  The `version.txt` sidecar is informational only and contains no
credentials or secrets.

### Reliability
Improved: downstream tooling no longer needs a GitHub API call to discover the
current version before downloading.

### Compatibility

**Breaking change for direct-URL consumers**: Anyone hardcoding a versioned
artifact URL (e.g. in a script or CI job) will continue to get the same file
from that URL because GitHub Releases keeps all historical assets.  However,
scripts that *discover* the latest URL by scraping the versioned filename must
be updated to use the new stable names.

The `.msixbundle` filename is unchanged.

### Performance, Power, and Efficiency
Slight improvement: update-checkers can use a single HEAD request instead of
two (one to get the tag, one to download).

## Potential Issues

- **Multiple architectures, one version.txt per platform**: Each architecture's
  `version.txt` is renamed to `version_x64.txt`, `version_arm64.txt`, etc.
  when uploaded to Azure Storage, to avoid collisions.  GitHub Release assets
  only carry a single copy per filename; callers must pick the right one.

- **MSIX bundle filename**: The `.msixbundle` retains the version in its name
  because Windows' `Add-AppxPackage` and the Microsoft Store pipeline use the
  filename as an identity hint.  The bundle's `version.txt` fills the gap.

## Future Considerations

- A JSON-formatted `release-metadata.json` could be introduced later to carry
  additional fields (commit SHA, build timestamp, changelog URL) without
  breaking the simple `version.txt` consumers.

- The `version.txt` format could be versioned itself (add a `MetadataVersion=1`
  field) to allow forward-compatible evolution.

## Resources

- GitHub issue: https://github.com/microsoft/terminal/issues/20902
- GitHub Releases "latest" redirect documentation:
  https://docs.github.com/en/repositories/releasing-projects-on-github/linking-to-releases
