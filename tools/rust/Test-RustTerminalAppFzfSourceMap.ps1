#Requires -Version 7
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '../..')
$map = Get-Content -Raw (Join-Path $PSScriptRoot 'r08-terminal-app-fzf-source-map.json') | ConvertFrom-Json -AsHashtable
$overlay = Get-Content -Raw (Join-Path $PSScriptRoot 'microsoft-rust-equivalence-h09.json') | ConvertFrom-Json -AsHashtable

if ([int]$map.schemaVersion -ne 1 -or [string]$map.stage -ne 'H09-R08-TerminalApp-FZF') {
    throw 'Unsupported H09 TerminalApp FZF source-map schema/stage.'
}

$blobCount = 0
foreach ($source in @($map.sources)) {
    $sourcePath = [string]$source.sourcePath
    $expectedBlobSha = [string]$source.sourceBlobSha
    if ([string]::IsNullOrWhiteSpace($sourcePath) -or $expectedBlobSha -notmatch '^[0-9a-f]{40}$') {
        throw 'H09 TerminalApp FZF source entry must provide sourcePath and a 40-character Git blob SHA.'
    }

    $sourceFullPath = Join-Path $repoRoot $sourcePath
    if (-not (Test-Path -LiteralPath $sourceFullPath -PathType Leaf)) {
        throw "H09 TerminalApp FZF source no longer exists: $sourcePath"
    }

    $actualBlobSha = ((& git -C $repoRoot rev-parse "HEAD:$sourcePath") | Out-String).Trim()
    if ($LASTEXITCODE -ne 0 -or $actualBlobSha -notmatch '^[0-9a-f]{40}$') {
        throw "Unable to resolve current Git blob for H09 TerminalApp FZF source: $sourcePath"
    }
    if ($actualBlobSha -ne $expectedBlobSha) {
        throw "H09 TerminalApp FZF source drift requires parity re-audit: ${sourcePath} expected=$expectedBlobSha actual=$actualBlobSha"
    }
    $blobCount++
}

$contractSourcePath = [string]$map.sources[0].sourcePath
$contractSource = Get-Content -Raw (Join-Path $repoRoot $contractSourcePath)
$sourceMethods = @([regex]::Matches($contractSource, 'TEST_METHOD\(([A-Za-z0-9_]+)\);') | ForEach-Object { $_.Groups[1].Value })
if ($sourceMethods.Count -ne [int]$map.expected.sourceMethods) {
    throw "H09 TerminalApp FZF source-method count changed: expected=$($map.expected.sourceMethods) actual=$($sourceMethods.Count)"
}
if (($sourceMethods | Sort-Object -Unique).Count -ne $sourceMethods.Count) {
    throw 'H09 TerminalApp FZF source contains duplicate TEST_METHOD identities.'
}

$ignoredMethod = [string]$map.ignoredMicrosoftMethod
if ($ignoredMethod -notin $sourceMethods) {
    throw "H09 ignored Microsoft FZF method disappeared: $ignoredMethod"
}
if (-not $contractSource.Contains('TEST_METHOD_PROPERTY(L"Ignore", L"true")')) {
    throw 'H09 expected Microsoft FZF Ignore property is missing.'
}

$exactEntries = @($overlay.entries | Where-Object {
    $_.suite -eq 'terminalApp' -and $_.source -eq 'FzfTests.cpp' -and $_.coverage -eq 'Exact'
})
if ($exactEntries.Count -ne [int]$map.expected.exactMethods) {
    throw "H09 TerminalApp FZF Exact ledger count changed: expected=$($map.expected.exactMethods) actual=$($exactEntries.Count)"
}
if ($exactEntries.method -contains $ignoredMethod) {
    throw "H09 ignored Microsoft FZF method must not be classified Exact: $ignoredMethod"
}

$exactMethods = @{}
foreach ($entry in $exactEntries) {
    $method = [string]$entry.method
    if ($method -notin $sourceMethods) {
        throw "H09 TerminalApp FZF Exact ledger entry references unknown source method: $method"
    }
    if ($exactMethods.ContainsKey($method)) {
        throw "Duplicate H09 TerminalApp FZF Exact method: $method"
    }
    $exactMethods[$method] = $true
}

foreach ($method in $sourceMethods) {
    if ($method -ne $ignoredMethod -and -not $exactMethods.ContainsKey($method)) {
        throw "Active H09 TerminalApp FZF source method lacks an Exact Rust contract: $method"
    }
}

$rustOwnerPath = [string]$map.rustOwner
$rustLibraryPath = [string]$map.rustLibrary
$rustTestsPath = [string]$map.rustContractTests
foreach ($path in @($rustOwnerPath, $rustLibraryPath, $rustTestsPath)) {
    if (-not (Test-Path -LiteralPath (Join-Path $repoRoot $path) -PathType Leaf)) {
        throw "H09 TerminalApp FZF Rust artifact no longer exists: $path"
    }
}

$libraryContent = Get-Content -Raw (Join-Path $repoRoot $rustLibraryPath)
if (-not $libraryContent.Contains('#![forbid(unsafe_code)]')) {
    throw 'H09 terminal-app must retain #![forbid(unsafe_code)].'
}

$testsContent = Get-Content -Raw (Join-Path $repoRoot $rustTestsPath)
$actualTests = @([regex]::Matches($testsContent, '(?ms)#\[test\]\s*fn\s+([A-Za-z0-9_]+)') | ForEach-Object { $_.Groups[1].Value })
if ($actualTests.Count -ne [int]$map.expected.rustWitnesses) {
    throw "H09 TerminalApp FZF Rust witness count changed: expected=$($map.expected.rustWitnesses) actual=$($actualTests.Count)"
}

$witnesses = @{}
foreach ($entry in $exactEntries) {
    $entryWitnesses = @($entry.rustWitnesses)
    if ($entryWitnesses.Count -ne 1) {
        throw "H09 TerminalApp FZF Exact entry requires exactly one direct witness: $($entry.method)"
    }
    $witness = [string]$entryWitnesses[0]
    if ($witnesses.ContainsKey($witness)) {
        throw "Duplicate H09 TerminalApp FZF Rust witness mapping: $witness"
    }
    if ($witness -notin $actualTests) {
        throw "H09 TerminalApp FZF Rust witness is missing from contract tests: $witness"
    }
    $witnesses[$witness] = $true
}

foreach ($test in $actualTests) {
    if (-not $witnesses.ContainsKey($test)) {
        throw "H09 TerminalApp FZF Rust test has no Exact Microsoft mapping: $test"
    }
}

if ($blobCount -ne [int]$map.expected.sourceBlobs -or
    $sourceMethods.Count -ne ([int]$map.expected.exactMethods + [int]$map.expected.ignoredBacklog)) {
    throw "H09 TerminalApp FZF summary changed unexpectedly: sourceMethods=$($sourceMethods.Count) Exact=$($exactEntries.Count) ignored=$($map.expected.ignoredBacklog) blobs=$blobCount witnesses=$($actualTests.Count)"
}

Write-Host "H09 TerminalApp FZF seam gate passed (source methods=$($sourceMethods.Count), Exact=$($exactEntries.Count), ignored backlog=$($map.expected.ignoredBacklog), Rust witnesses=$($actualTests.Count), pinned source blobs=$blobCount)."
