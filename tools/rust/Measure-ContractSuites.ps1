#Requires -Version 7
[CmdletBinding()]
param(
    [ValidateSet('x64', 'x86')]
    [string] $Platform = 'x64',

    [ValidateSet('Debug', 'Release')]
    [string] $Configuration = 'Debug',

    [string] $OutputDirectory = (Join-Path (Resolve-Path (Join-Path $PSScriptRoot '..\..')) 'artifacts\contract-measurement')
)

$ErrorActionPreference = 'Stop'
$suites = @(
    'host',
    'textBuffer',
    'terminalCore',
    'terminalApp',
    'localTerminalApp',
    'unitSettingsModel',
    'unitControl',
    'interactivityWin32',
    'terminal',
    'adapter',
    'types',
    'til'
)

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$invokeScript = Join-Path $PSScriptRoot 'Invoke-ContractTest.ps1'
$results = [System.Collections.Generic.List[object]]::new()

foreach ($suite in $suites) {
    Write-Host ''
    Write-Host "=== Measuring $suite ===" -ForegroundColor Cyan

    $suiteOutput = Join-Path $OutputDirectory $suite
    try {
        & $invokeScript -Suite $suite -Platform $Platform -Configuration $Configuration -OutputDirectory $suiteOutput -MeasureOnly | Out-Host
        $resultPath = Join-Path $suiteOutput "$suite.json"
        $results.Add((Get-Content -Raw $resultPath | ConvertFrom-Json))
    }
    catch {
        $results.Add([pscustomobject]@{
            suite         = $suite
            platform      = $Platform
            configuration = $Configuration
            durationMs    = $null
            total         = $null
            passed        = $null
            failed        = $null
            blocked       = $null
            notRun        = $null
            skipped       = $null
            error         = $_.Exception.Message
        })
        Write-Warning "Measurement failed for '$suite': $($_.Exception.Message)"
    }
}

$resultsPath = Join-Path $OutputDirectory 'suite-measurements.json'
$results | ConvertTo-Json -Depth 5 | Set-Content -Path $resultsPath -Encoding utf8

$rows = @(
    '# Microsoft contract suite measurements'
    ''
    '| Suite | Duration | Total | Passed | Failed | Blocked | Skipped |'
    '|---|---:|---:|---:|---:|---:|---:|'
)

foreach ($result in $results) {
    $duration = if ($null -ne $result.durationMs) {
        [TimeSpan]::FromMilliseconds([double]$result.durationMs).ToString()
    } else {
        'ERROR'
    }

    $rows += "| $($result.suite) | $duration | $($result.total) | $($result.passed) | $($result.failed) | $($result.blocked) | $($result.skipped) |"
}

$markdownPath = Join-Path $OutputDirectory 'suite-measurements.md'
$rows | Set-Content -Path $markdownPath -Encoding utf8

Write-Host ''
Write-Host "Measurements: $resultsPath"
Write-Host "Report:       $markdownPath"
