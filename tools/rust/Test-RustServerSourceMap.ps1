#Requires -Version 7
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '../..')
$mapPath = Join-Path $PSScriptRoot 'r06b-server-source-map.json'
$map = Get-Content -Raw $mapPath | ConvertFrom-Json -AsHashtable

if ([int]$map.schemaVersion -ne 2) {
    throw "Unsupported R06-B server source-map schema: $($map.schemaVersion)"
}

$allowedOwnership = @('split', 'native')
$seenSources = @{}
$splitCount = 0
$nativeCount = 0
$witnessCount = 0
$blobCount = 0

foreach ($entry in @($map.entries)) {
    $sourcePath = [string]$entry.sourcePath
    if ([string]::IsNullOrWhiteSpace($sourcePath)) {
        throw 'R06-B server source-map entry is missing sourcePath.'
    }
    if ($seenSources.ContainsKey($sourcePath)) {
        throw "Duplicate R06-B server source-map entry: $sourcePath"
    }
    $seenSources[$sourcePath] = $true

    $sourceFullPath = Join-Path $repoRoot $sourcePath
    if (-not (Test-Path -LiteralPath $sourceFullPath -PathType Leaf)) {
        throw "R06-B server source no longer exists: $sourcePath"
    }

    $expectedBlobSha = [string]$entry.sourceBlobSha
    if ($expectedBlobSha -notmatch '^[0-9a-f]{40}$') {
        throw "R06-B server entry must pin a 40-character Git blob SHA: $sourcePath"
    }
    $actualBlobSha = ((& git -C $repoRoot rev-parse "HEAD:$sourcePath") | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $actualBlobSha -notmatch '^[0-9a-f]{40}$') {
        throw "Unable to resolve current Git blob for R06-B server source: $sourcePath"
    }
    if ($actualBlobSha -ne $expectedBlobSha) {
        throw "R06-B server source drift requires ownership re-audit: $sourcePath expected=$expectedBlobSha actual=$actualBlobSha"
    }
    $blobCount++

    $ownership = [string]$entry.ownership
    if ($ownership -notin $allowedOwnership) {
        throw "Unknown R06-B server ownership '$ownership' for $sourcePath"
    }
    if ([string]::IsNullOrWhiteSpace([string]$entry.nativeBoundary)) {
        throw "R06-B server entry must document its native boundary: $sourcePath"
    }

    $rustPath = [string]$entry.rustPath
    $witnesses = @($entry.rustWitnesses)

    if ($ownership -eq 'split') {
        $splitCount++
        if ([string]::IsNullOrWhiteSpace($rustPath)) {
            throw "Split R06-B server entry requires rustPath: $sourcePath"
        }
        if ($witnesses.Count -eq 0) {
            throw "Split R06-B server entry requires Rust witnesses: $sourcePath"
        }

        $rustFullPath = Join-Path $repoRoot $rustPath
        if (-not (Test-Path -LiteralPath $rustFullPath -PathType Leaf)) {
            throw "R06-B Rust server owner no longer exists: $rustPath"
        }
        $rustContent = Get-Content -Raw $rustFullPath
        foreach ($witness in $witnesses) {
            $needle = "fn $witness"
            if (-not $rustContent.Contains($needle)) {
                throw "R06-B Rust witness '$witness' is missing from $rustPath"
            }
            $witnessCount++
        }
    }
    else {
        $nativeCount++
        if (-not [string]::IsNullOrWhiteSpace($rustPath)) {
            throw "Native R06-B server entry must not claim a Rust owner: $sourcePath"
        }
        if ($witnesses.Count -ne 0) {
            throw "Native R06-B server entry must not claim Rust witnesses: $sourcePath"
        }
    }
}

if ($splitCount -ne 3 -or $nativeCount -ne 3 -or $witnessCount -ne 16 -or $blobCount -ne 6) {
    throw "R06-B server source-map summary changed unexpectedly: split=$splitCount native=$nativeCount witnesses=$witnessCount blobs=$blobCount"
}

Write-Host "R06-B server seam gate passed (split=$splitCount, native=$nativeCount, Rust witnesses=$witnessCount, pinned source blobs=$blobCount)."
