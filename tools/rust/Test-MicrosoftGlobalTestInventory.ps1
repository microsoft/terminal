#Requires -Version 7
$ErrorActionPreference = 'Stop'

$globalScript = Join-Path $PSScriptRoot 'Get-MicrosoftGlobalTestInventory.ps1'
$census = Get-Content -Raw (Join-Path $PSScriptRoot 'microsoft-test-source-census.json') | ConvertFrom-Json -AsHashtable
$ledger = Get-Content -Raw (Join-Path $PSScriptRoot 'microsoft-rust-equivalence.json') | ConvertFrom-Json -AsHashtable
$baseline = Get-Content -Raw (Join-Path $PSScriptRoot 'contract-baseline.json') | ConvertFrom-Json -AsHashtable
$deferredMissing = Get-Content -Raw (Join-Path $PSScriptRoot 'microsoft-rust-deferred-missing.json') | ConvertFrom-Json -AsHashtable
$raw = (& $globalScript | Out-String).Trim()
$inventory = @($raw | ConvertFrom-Json)

if ([int]$deferredMissing.schemaVersion -ne 1) {
    throw 'Unsupported deferred-Missing manifest schema.'
}
if (-not $deferredMissing.ContainsKey('expectedMissingTotal') -or -not $deferredMissing.ContainsKey('sources')) {
    throw 'Deferred-Missing manifest requires expectedMissingTotal and sources.'
}

$expectedSuites = @($baseline.suites.Keys | Sort-Object)
if (($expectedSuites -join ',') -ne (@($census.suites.Keys | Sort-Object) -join ',')) {
    throw 'Microsoft source census suites do not match contract-baseline.json.'
}
if (($expectedSuites -join ',') -ne (@($ledger.suites.Keys | Sort-Object) -join ',')) {
    throw 'Microsoft equivalence ledger suites do not match contract-baseline.json.'
}

$allowedCoverage = @($ledger.coverageClasses)
$entryKeys = @{}
foreach ($entry in @($ledger.entries)) {
    if ($entry.coverage -notin $allowedCoverage) {
        throw "Unknown coverage '$($entry.coverage)' in equivalence ledger."
    }
    $key = "$($entry.suite)|$($entry.source)|$($entry.method)"
    if ($entryKeys.ContainsKey($key)) {
        throw "Duplicate equivalence ledger entry: $key"
    }
    $entryKeys[$key] = $entry
}

$deferredMissingSources = @{}
foreach ($source in @($deferredMissing.sources)) {
    $key = "$($source.suite)|$($source.source)"
    if ([string]::IsNullOrWhiteSpace([string]$source.suite) -or
        [string]::IsNullOrWhiteSpace([string]$source.source) -or
        [string]::IsNullOrWhiteSpace([string]$source.reason)) {
        throw "Deferred-Missing source requires suite, source and reason: $key"
    }
    if ($deferredMissingSources.ContainsKey($key)) {
        throw "Duplicate deferred-Missing source: $key"
    }
    if ($source.ContainsKey('expectedMissing') -and [int]$source.expectedMissing -lt 1) {
        throw "Deferred-Missing expectedMissing must be positive: $key"
    }
    $deferredMissingSources[$key] = $source
}

$sourceRules = @{}
$overlayExpectations = @{}
$globalCoverageExpectation = $null
$globalCoverageExpectationSource = $null
$globalCoverageExpectationPriority = $null
$overlayFiles = @(Get-ChildItem -Path $PSScriptRoot -Filter 'microsoft-rust-equivalence-*.json' -File | Sort-Object Name)
foreach ($overlayFile in $overlayFiles) {
    $overlay = Get-Content -Raw $overlayFile.FullName | ConvertFrom-Json -AsHashtable
    if ($overlay.ContainsKey('entries')) {
        foreach ($entry in @($overlay.entries)) {
            if ($entry.coverage -notin $allowedCoverage) {
                throw "Unknown coverage '$($entry.coverage)' in $($overlayFile.Name)."
            }
            $key = "$($entry.suite)|$($entry.source)|$($entry.method)"
            if ($entryKeys.ContainsKey($key)) {
                throw "Duplicate equivalence ledger entry across overlays: $key"
            }
            $entryKeys[$key] = $entry
        }
    }
    if ($overlay.ContainsKey('sourceRules')) {
        foreach ($rule in @($overlay.sourceRules)) {
            if ($rule.coverage -notin $allowedCoverage) {
                throw "Unknown coverage '$($rule.coverage)' in $($overlayFile.Name)."
            }
            if ($rule.coverage -notin @('Missing', 'Platform-only', 'UI-managed') -and @($rule.rustWitnesses).Count -eq 0) {
                throw "Non-missing source rule requires at least one Rust witness: $($rule.suite)|$($rule.source)"
            }
            $key = "$($rule.suite)|$($rule.source)"
            if ($sourceRules.ContainsKey($key)) {
                throw "Duplicate source equivalence rule across overlays: $key"
            }
            $sourceRules[$key] = $rule
        }
    }
    if ($overlay.ContainsKey('expectedCoverage')) {
        $priority = if ($overlay.ContainsKey('expectedCoveragePriority')) {
            [int]$overlay.expectedCoveragePriority
        }
        else {
            0
        }

        foreach ($suite in @($overlay.expectedCoverage.Keys)) {
            $candidate = @{
                coverage = $overlay.expectedCoverage[$suite]
                priority = $priority
                source = $overlayFile.Name
            }

            if (-not $overlayExpectations.ContainsKey($suite) -or
                $priority -gt [int]($overlayExpectations[$suite].priority)) {
                $overlayExpectations[$suite] = $candidate
            }
            elseif ($priority -eq [int]($overlayExpectations[$suite].priority)) {
                throw "Duplicate expectedCoverage priority $priority for suite ${suite}: $($overlayExpectations[$suite].source) and $($overlayFile.Name)"
            }
        }
    }
    if ($overlay.ContainsKey('expectedGlobalCoverage')) {
        $priority = if ($overlay.ContainsKey('expectedGlobalCoveragePriority')) {
            [int]$overlay.expectedGlobalCoveragePriority
        }
        else {
            0
        }

        if ($null -eq $globalCoverageExpectation -or $priority -gt $globalCoverageExpectationPriority) {
            $globalCoverageExpectation = $overlay.expectedGlobalCoverage
            $globalCoverageExpectationSource = $overlayFile.Name
            $globalCoverageExpectationPriority = $priority
        }
        elseif ($priority -eq $globalCoverageExpectationPriority) {
            throw "Duplicate expectedGlobalCoverage priority $priority across overlays: $globalCoverageExpectationSource and $($overlayFile.Name)"
        }
    }
}

$currentKeys = @{}
$currentSources = @{}
$suiteCoverage = @{}
$observedDeferredMissing = @{}
$bootstrapRequired = $false
$reconciledSuites = @(
    'terminal',
    'adapter',
    'textBuffer',
    'types',
    'til',
    'terminalCore',
    'host',
    'interactivityWin32',
    'localTerminalApp',
    'terminalApp',
    'unitControl',
    'unitSettingsModel'
)
foreach ($suite in $expectedSuites) {
    $items = @($inventory | Where-Object suite -eq $suite)
    if ($items.Count -eq 0) {
        throw "$suite source inventory is empty."
    }
    foreach ($item in $items) {
        $currentKeys["$($item.suite)|$($item.source)|$($item.method)"] = $true
        $currentSources["$($item.suite)|$($item.source)"] = $true
    }

    $identities = @($items | ForEach-Object { "$($_.suite)|$($_.source)|$($_.method)" } | Sort-Object -Unique)
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($identities -join "`n")
    $hash = [Convert]::ToHexString([System.Security.Cryptography.SHA256]::HashData($bytes)).ToLowerInvariant()
    $frozen = $census.suites[$suite]
    $runtimeTotal = [int]$baseline.suites[$suite].total

    if ([int]$frozen.runtimeBaseline -ne $runtimeTotal) {
        throw "$suite runtime baseline differs from contract-baseline.json."
    }
    if ($ledger.suites[$suite].defaultCoverage -notin $allowedCoverage) {
        throw "$suite has an invalid default coverage."
    }

    if ($null -eq $frozen.sourceMethodCount -or [string]::IsNullOrWhiteSpace([string]$frozen.identitySha256)) {
        $bootstrapRequired = $true
        Write-Host "CENSUS_BOOTSTRAP|$suite|$($items.Count)|$hash"
    }
    elseif ([int]$frozen.sourceMethodCount -ne $items.Count -or [string]$frozen.identitySha256 -ne $hash) {
        throw "$suite Microsoft source contract changed: expected $($frozen.sourceMethodCount) methods / $($frozen.identitySha256), got $($items.Count) / $hash. Reconcile the ledger before updating the census."
    }

    if ($suite -eq 'host') {
        @($items | Group-Object source | Sort-Object Name) | ForEach-Object {
            Write-Host "Microsoft host source census: $($_.Name)=$($_.Count)"
        }
    }

    $coverageCounts = @{}
    foreach ($item in $items) {
        $key = "$($item.suite)|$($item.source)|$($item.method)"
        $sourceKey = "$($item.suite)|$($item.source)"
        if ($entryKeys.ContainsKey($key)) {
            $coverage = $entryKeys[$key].coverage
        }
        elseif ($sourceRules.ContainsKey($sourceKey)) {
            $coverage = $sourceRules[$sourceKey].coverage
        }
        else {
            $coverage = $ledger.suites[$suite].defaultCoverage
        }

        if ($suite -in $reconciledSuites -and -not $entryKeys.ContainsKey($key) -and -not $sourceRules.ContainsKey($sourceKey)) {
            throw "Reconciled-stage contract has not been deliberately classified: $key"
        }

        if ($coverage -eq 'Missing') {
            if (-not $deferredMissingSources.ContainsKey($sourceKey)) {
                throw "Missing Microsoft contract is not explicitly deferred: $key"
            }
            if (-not $observedDeferredMissing.ContainsKey($sourceKey)) {
                $observedDeferredMissing[$sourceKey] = 0
            }
            $observedDeferredMissing[$sourceKey]++
        }

        if (-not $coverageCounts.ContainsKey($coverage)) { $coverageCounts[$coverage] = 0 }
        $coverageCounts[$coverage]++
    }
    $suiteCoverage[$suite] = $coverageCounts
    $summary = @($coverageCounts.Keys | Sort-Object | ForEach-Object { "$_=$($coverageCounts[$_])" }) -join ', '
    Write-Host "Microsoft source census: $suite=$($items.Count); runtime=$runtimeTotal; $summary"
}

foreach ($key in $entryKeys.Keys) {
    if (-not $currentKeys.ContainsKey($key)) {
        throw "Equivalence ledger references a removed Microsoft contract: $key"
    }
}
foreach ($key in $sourceRules.Keys) {
    if (-not $currentSources.ContainsKey($key)) {
        throw "Source equivalence rule references a removed Microsoft source: $key"
    }
}
foreach ($key in $deferredMissingSources.Keys) {
    if (-not $currentSources.ContainsKey($key)) {
        throw "Deferred-Missing manifest references a removed Microsoft source: $key"
    }
    $actualMissing = if ($observedDeferredMissing.ContainsKey($key)) {
        [int]$observedDeferredMissing[$key]
    }
    else {
        0
    }
    if ($actualMissing -eq 0) {
        throw "Deferred-Missing source no longer contains Missing contracts; remove it from the manifest: $key"
    }
    $deferredSource = $deferredMissingSources[$key]
    if ($deferredSource.ContainsKey('expectedMissing') -and
        [int]$deferredSource.expectedMissing -ne $actualMissing) {
        throw "Deferred-Missing count changed for ${key}: expected $($deferredSource.expectedMissing), got $actualMissing."
    }
}
foreach ($suite in $overlayExpectations.Keys) {
    $expectation = $overlayExpectations[$suite]
    $expected = $expectation.coverage
    $actual = $suiteCoverage[$suite]
    foreach ($coverage in $allowedCoverage) {
        $expectedCount = if ($expected.ContainsKey($coverage)) { [int]$expected[$coverage] } else { 0 }
        $actualCount = if ($actual.ContainsKey($coverage)) { [int]$actual[$coverage] } else { 0 }
        if ($expectedCount -ne $actualCount) {
            throw "$suite expectedCoverage mismatch for ${coverage}: expected $expectedCount, got $actualCount ($($expectation.source), priority $($expectation.priority))."
        }
    }
}

$globalCoverage = @{}
foreach ($coverage in $allowedCoverage) { $globalCoverage[$coverage] = 0 }
foreach ($suite in $expectedSuites) {
    foreach ($coverage in $allowedCoverage) {
        if ($suiteCoverage[$suite].ContainsKey($coverage)) {
            $globalCoverage[$coverage] += [int]$suiteCoverage[$suite][$coverage]
        }
    }
}
$globalSummary = @($allowedCoverage | ForEach-Object { "$_=$($globalCoverage[$_])" }) -join ', '
Write-Host "Microsoft global coverage: $globalSummary"

if ([int]$deferredMissing.expectedMissingTotal -ne [int]$globalCoverage['Missing']) {
    throw "Deferred-Missing total mismatch: expected $($deferredMissing.expectedMissingTotal), got $($globalCoverage['Missing'])."
}
if ($null -ne $globalCoverageExpectation) {
    foreach ($coverage in $allowedCoverage) {
        $expectedCount = if ($globalCoverageExpectation.ContainsKey($coverage)) { [int]$globalCoverageExpectation[$coverage] } else { 0 }
        $actualCount = [int]$globalCoverage[$coverage]
        if ($expectedCount -ne $actualCount) {
            throw "Global expectedCoverage mismatch for ${coverage}: expected $expectedCount, got $actualCount ($globalCoverageExpectationSource, priority $globalCoverageExpectationPriority)."
        }
    }
}

if ($bootstrapRequired) {
    throw 'Global Microsoft source census requires bootstrap fingerprints. Copy all CENSUS_BOOTSTRAP values into microsoft-test-source-census.json.'
}

Write-Host "Microsoft global source inventory gate passed ($($inventory.Count) source methods across $($expectedSuites.Count) suites; deferred Missing=$($globalCoverage['Missing']))."
