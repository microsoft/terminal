#Requires -Version 7
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [ValidateSet('host', 'textBuffer', 'terminalCore', 'terminalApp', 'localTerminalApp', 'unitSettingsModel', 'unitControl', 'interactivityWin32', 'terminal', 'adapter', 'types', 'til')]
    [string] $Suite,

    [ValidateSet('x64', 'x86')]
    [string] $Platform = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [string] $BaselinePath = (Join-Path $PSScriptRoot 'contract-baseline.json'),

    [string] $OutputDirectory = (Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..\..')) 'artifacts\contract'),

    [switch] $MeasureOnly
)

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$openConsoleModule = Join-Path $root 'tools\OpenConsole.psm1'
$contractModule = Join-Path $PSScriptRoot 'TaefContract.psm1'

Import-Module $openConsoleModule -Force
Import-Module $contractModule -Force

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$logPath = Join-Path $OutputDirectory "$Suite.log"
$jsonPath = Join-Path $OutputDirectory "$Suite.json"
$inventoryLogPath = Join-Path $OutputDirectory "$Suite.inventory.log"
$inventoryJsonPath = Join-Path $OutputDirectory "$Suite.inventory.json"

$baseline = $null
if (-not $MeasureOnly) {
    $baselineDocument = Get-Content -Raw -Path $BaselinePath | ConvertFrom-Json
    $baseline = $baselineDocument.suites.$Suite
    if ($null -eq $baseline) {
        throw "No contract baseline exists for suite '$Suite'."
    }
}

# TAEF /listProperties expands data-driven methods into their individual
# invocation identities without executing them. /runIgnoredTests keeps ignored
# tests in the inventory so its count can be compared with the suite total.
& {
    Invoke-OpenConsoleTests `
        -Test $Suite `
        -Platform $Platform `
        -Configuration $Configuration `
        -TaefArgs @('/listProperties', '/runIgnoredTests', '/coloredConsoleOutput:false')
} *>&1 | Tee-Object -FilePath $inventoryLogPath | Out-Host

$inventoryText = Get-Content -Raw -Path $inventoryLogPath
$inventory = @(Get-TaefTestInventory -Text $inventoryText)
if ($inventory.Count -eq 0) {
    throw "TAEF inventory for '$Suite' was empty."
}

$inventoryDocument = [ordered]@{
    suite         = $Suite
    platform      = $Platform
    configuration = $Configuration
    count         = $inventory.Count
    tests         = $inventory
}
$inventoryDocument | ConvertTo-Json -Depth 5 | Set-Content -Path $inventoryJsonPath -Encoding utf8

# Fail before the expensive suite execution if the cheap inventory already
# disagrees with the recorded contract total. This protects both evidence
# quality and runner time.
if ($null -ne $baseline -and $inventory.Count -ne [int]$baseline.total) {
    throw "TAEF inventory count changed for '$Suite': expected $($baseline.total), got $($inventory.Count)."
}

$stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
try {
    & {
        Invoke-OpenConsoleTests -Test $Suite -Platform $Platform -Configuration $Configuration
    } *>&1 | Tee-Object -FilePath $logPath | Out-Host
}
finally {
    $stopwatch.Stop()
}

$text = Get-Content -Raw -Path $logPath
$summary = Get-TaefSummary -Text $text

if ($inventory.Count -ne $summary.Total) {
    throw "TAEF inventory/result mismatch for '$Suite': inventory=$($inventory.Count), result total=$($summary.Total)."
}

$result = [ordered]@{
    suite          = $Suite
    platform       = $Platform
    configuration  = $Configuration
    durationMs     = $stopwatch.ElapsedMilliseconds
    inventoryCount = $inventory.Count
    total          = $summary.Total
    passed         = $summary.Passed
    failed         = $summary.Failed
    blocked        = $summary.Blocked
    notRun         = $summary.NotRun
    skipped        = $summary.Skipped
    baselinePass   = $null
    violations     = @()
}

if (-not $MeasureOnly) {
    $comparison = Test-TaefSummaryAgainstBaseline -Summary $summary -Baseline $baseline
    $result.baselinePass = $comparison.Passed
    $result.violations = @($comparison.Violations)
}

$result | ConvertTo-Json -Depth 5 | Set-Content -Path $jsonPath -Encoding utf8

Write-Host ''
Write-Host "Contract suite: $Suite"
Write-Host ("Duration:       {0}" -f $stopwatch.Elapsed)
Write-Host ("Inventory:      {0} canonical TAEF invocations" -f $inventory.Count)
Write-Host ("TAEF:           Total={0}, Passed={1}, Failed={2}, Blocked={3}, NotRun={4}, Skipped={5}" -f `
    $summary.Total, $summary.Passed, $summary.Failed, $summary.Blocked, $summary.NotRun, $summary.Skipped)

if (-not $MeasureOnly -and -not $result.baselinePass) {
    throw "Contract regression in '$Suite': $($result.violations -join ' ')"
}

[pscustomobject]$result
