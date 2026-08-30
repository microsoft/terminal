#Requires -Version 7
param(
    [switch]$RequireZero,
    [string]$JsonPath
)

$ErrorActionPreference = 'Stop'

$globalScript = Join-Path $PSScriptRoot 'Get-MicrosoftGlobalTestInventory.ps1'
$ledger = Get-Content -Raw (Join-Path $PSScriptRoot 'microsoft-rust-equivalence.json') | ConvertFrom-Json -AsHashtable
$manifest = Get-Content -Raw (Join-Path $PSScriptRoot 'microsoft-rust-partial-debt.json') | ConvertFrom-Json -AsHashtable
$raw = (& $globalScript | Out-String).Trim()
$inventory = @($raw | ConvertFrom-Json)

if ([int]$manifest.schemaVersion -ne 1) {
    throw 'Unsupported R08 Partial-debt manifest schema.'
}
if (-not $manifest.ContainsKey('expectedPartialTotal') -or
    -not $manifest.ContainsKey('defaultClass') -or
    -not $manifest.ContainsKey('allowedClasses') -or
    -not $manifest.ContainsKey('exceptions')) {
    throw 'R08 Partial-debt manifest requires expectedPartialTotal, defaultClass, allowedClasses and exceptions.'
}

$allowedClasses = @($manifest.allowedClasses)
if ([string]$manifest.defaultClass -notin $allowedClasses) {
    throw "Unknown default Partial-debt class '$($manifest.defaultClass)'."
}

$entryKeys = @{}
foreach ($entry in @($ledger.entries)) {
    $key = "$($entry.suite)|$($entry.source)|$($entry.method)"
    if ($entryKeys.ContainsKey($key)) {
        throw "Duplicate equivalence ledger entry: $key"
    }
    $entryKeys[$key] = $entry
}

$sourceRules = @{}
$overlayFiles = @(Get-ChildItem -Path $PSScriptRoot -Filter 'microsoft-rust-equivalence-*.json' -File | Sort-Object Name)
foreach ($overlayFile in $overlayFiles) {
    $overlay = Get-Content -Raw $overlayFile.FullName | ConvertFrom-Json -AsHashtable
    if ($overlay.ContainsKey('entries')) {
        foreach ($entry in @($overlay.entries)) {
            $key = "$($entry.suite)|$($entry.source)|$($entry.method)"
            if ($entryKeys.ContainsKey($key)) {
                throw "Duplicate equivalence ledger entry across overlays: $key"
            }
            $entryKeys[$key] = $entry
        }
    }
    if ($overlay.ContainsKey('sourceRules')) {
        foreach ($rule in @($overlay.sourceRules)) {
            $key = "$($rule.suite)|$($rule.source)"
            if ($sourceRules.ContainsKey($key)) {
                throw "Duplicate source equivalence rule across overlays: $key"
            }
            $sourceRules[$key] = $rule
        }
    }
}

$methodExceptions = @{}
$sourceExceptions = @{}
foreach ($exception in @($manifest.exceptions)) {
    if ([string]::IsNullOrWhiteSpace([string]$exception.suite) -or
        [string]::IsNullOrWhiteSpace([string]$exception.source) -or
        [string]::IsNullOrWhiteSpace([string]$exception.class) -or
        [string]::IsNullOrWhiteSpace([string]$exception.reason)) {
        throw 'Every Partial-debt exception requires suite, source, class and reason.'
    }
    if ([string]$exception.class -notin $allowedClasses -or [string]$exception.class -eq 'functional') {
        throw "Partial-debt exception must use a non-functional allowed class: $($exception.class)"
    }

    $sourceKey = "$($exception.suite)|$($exception.source)"
    if ($exception.ContainsKey('method') -and -not [string]::IsNullOrWhiteSpace([string]$exception.method)) {
        $key = "$sourceKey|$($exception.method)"
        if ($methodExceptions.ContainsKey($key)) {
            throw "Duplicate method Partial-debt exception: $key"
        }
        $methodExceptions[$key] = $exception
    }
    else {
        if ($sourceExceptions.ContainsKey($sourceKey)) {
            throw "Duplicate source Partial-debt exception: $sourceKey"
        }
        $sourceExceptions[$sourceKey] = $exception
    }
}

$counts = @{}
foreach ($class in $allowedClasses) { $counts[$class] = 0 }
$partialKeys = @{}
$partialRows = [System.Collections.Generic.List[object]]::new()
$missingCount = 0

foreach ($item in $inventory) {
    $key = "$($item.suite)|$($item.source)|$($item.method)"
    $sourceKey = "$($item.suite)|$($item.source)"
    $rule = $null

    if ($entryKeys.ContainsKey($key)) {
        $rule = $entryKeys[$key]
        $coverage = [string]$rule.coverage
    }
    elseif ($sourceRules.ContainsKey($sourceKey)) {
        $rule = $sourceRules[$sourceKey]
        $coverage = [string]$rule.coverage
    }
    else {
        $coverage = [string]$ledger.suites[$item.suite].defaultCoverage
    }

    if ($coverage -eq 'Missing') {
        $missingCount++
        continue
    }
    if ($coverage -ne 'Partial') {
        continue
    }

    $partialKeys[$key] = $true
    $exception = $null
    if ($methodExceptions.ContainsKey($key)) {
        $exception = $methodExceptions[$key]
        $class = [string]$exception.class
    }
    elseif ($sourceExceptions.ContainsKey($sourceKey)) {
        $exception = $sourceExceptions[$sourceKey]
        $class = [string]$exception.class
    }
    else {
        $class = [string]$manifest.defaultClass
    }
    $counts[$class]++

    $witnesses = @()
    $notes = $null
    if ($null -ne $rule) {
        if ($rule.ContainsKey('rustWitnesses')) { $witnesses = @($rule.rustWitnesses) }
        if ($rule.ContainsKey('notes')) { $notes = [string]$rule.notes }
    }

    $partialRows.Add([pscustomobject][ordered]@{
        suite = [string]$item.suite
        source = [string]$item.source
        method = [string]$item.method
        class = $class
        rustWitnesses = $witnesses
        notes = $notes
        exceptionReason = if ($null -ne $exception) { [string]$exception.reason } else { $null }
    })
}

foreach ($key in $methodExceptions.Keys) {
    if (-not $partialKeys.ContainsKey($key)) {
        throw "Method Partial-debt exception no longer references an effective Partial contract: $key"
    }
}
foreach ($sourceKey in $sourceExceptions.Keys) {
    $prefix = "$sourceKey|"
    if (@($partialKeys.Keys | Where-Object { $_.StartsWith($prefix, [System.StringComparison]::Ordinal) }).Count -eq 0) {
        throw "Source Partial-debt exception no longer references any effective Partial contract: $sourceKey"
    }
}

$partialTotal = 0
foreach ($class in $allowedClasses) { $partialTotal += [int]$counts[$class] }
if ($partialTotal -ne [int]$manifest.expectedPartialTotal) {
    throw "R08 Partial-debt total changed: expected $($manifest.expectedPartialTotal), got $partialTotal. Re-audit the classification manifest before accepting the new census."
}

if (-not [string]::IsNullOrWhiteSpace($JsonPath)) {
    $resolvedPath = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($JsonPath)
    $parent = Split-Path -Parent $resolvedPath
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }

    $orderedRows = @($partialRows | Sort-Object suite, source, method)
    $functionalRows = @($orderedRows | Where-Object class -eq 'functional')
    $bySuite = @($functionalRows | Group-Object suite | Sort-Object @{ Expression = 'Count'; Descending = $true }, Name | ForEach-Object {
        [pscustomobject][ordered]@{ suite = $_.Name; count = $_.Count }
    })
    $bySource = @($functionalRows | Group-Object suite, source | Sort-Object @{ Expression = 'Count'; Descending = $true }, Name | ForEach-Object {
        $first = $_.Group[0]
        [pscustomobject][ordered]@{ suite = $first.suite; source = $first.source; count = $_.Count }
    })

    $payload = [ordered]@{
        schemaVersion = 1
        partialTotal = $partialTotal
        missing = $missingCount
        counts = [ordered]@{
            functional = [int]$counts['functional']
            'platform-boundary' = [int]$counts['platform-boundary']
            'language/API-shape' = [int]$counts['language/API-shape']
            'upstream-ignored' = [int]$counts['upstream-ignored']
        }
        functionalBySuite = $bySuite
        functionalBySource = $bySource
        functional = $functionalRows
        allPartial = $orderedRows
    }
    $payload | ConvertTo-Json -Depth 10 | Set-Content -Encoding utf8 -Path $resolvedPath
    Write-Host "R08 Partial backlog JSON: $resolvedPath"
}

$summary = @($allowedClasses | ForEach-Object { "$_=$($counts[$_])" }) -join ', '
Write-Host "R08 Partial debt: total=$partialTotal; $summary; Missing=$missingCount"

if ($RequireZero) {
    if ($missingCount -ne 0) {
        throw "R08 exit gate failed: Missing=$missingCount; expected 0."
    }
    if ([int]$counts['functional'] -ne 0) {
        throw "R08 exit gate failed: Partial(functional)=$($counts['functional']); expected 0."
    }
    Write-Host 'R08 functional-debt exit gate passed (Missing=0; Partial(functional)=0).'
}
else {
    Write-Host "R08 functional-debt classification gate passed (Partial(functional)=$($counts['functional']))."
}
