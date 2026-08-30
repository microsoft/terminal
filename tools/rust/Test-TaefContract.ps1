#Requires -Version 7
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'TaefContract.psm1') -Force

$sample = @'
Summary: Total=760, Passed=759, Failed=0, Blocked=0, Not Run=0, Skipped=1
'@

$summary = Get-TaefSummary -Text $sample
if ($summary.Total -ne 760 -or $summary.Passed -ne 759 -or $summary.Skipped -ne 1) {
    throw 'TAEF summary parser self-test failed.'
}

$inventorySample = @'
Test Authoring and Execution Framework v10 for x64

        C:\work\ConParser.Unit.Tests.dll
            Microsoft::Console::VirtualTerminal::Base64Test
                Microsoft::Console::VirtualTerminal::Base64Test::DecodeFuzz
                Microsoft::Console::VirtualTerminal::Base64Test::DecodeUTF8
            Microsoft::Console::VirtualTerminal::StateMachineTest
                Microsoft::Console::VirtualTerminal::StateMachineTest::DcsDataStringsReceivedByHandler#metadataSet0
                    Property[Data:terminatorType] = { 0, 1, 2, 3 }
                    Data[terminatorType] = 0
                Microsoft::Console::VirtualTerminal::StateMachineTest::DcsDataStringsReceivedByHandler#metadataSet1
                    Property[Data:terminatorType] = { 0, 1, 2, 3 }
                    Data[terminatorType] = 1
'@

$inventory = @(Get-TaefTestInventory -Text $inventorySample)
$expectedInventory = @(
    'Microsoft::Console::VirtualTerminal::Base64Test::DecodeFuzz',
    'Microsoft::Console::VirtualTerminal::Base64Test::DecodeUTF8',
    'Microsoft::Console::VirtualTerminal::StateMachineTest::DcsDataStringsReceivedByHandler#metadataSet0',
    'Microsoft::Console::VirtualTerminal::StateMachineTest::DcsDataStringsReceivedByHandler#metadataSet1'
)

if ($inventory.Count -ne $expectedInventory.Count) {
    throw "TAEF inventory parser self-test count failed: expected $($expectedInventory.Count), got $($inventory.Count)."
}

for ($i = 0; $i -lt $expectedInventory.Count; $i++) {
    if ($inventory[$i] -ne $expectedInventory[$i]) {
        throw "TAEF inventory parser self-test failed at index ${i}: expected '$($expectedInventory[$i])', got '$($inventory[$i])'."
    }
}

$baseline = [pscustomobject]@{
    total      = 760
    maxFailed  = 0
    maxBlocked = 0
    maxNotRun  = 0
    maxSkipped = 1
}

$result = Test-TaefSummaryAgainstBaseline -Summary $summary -Baseline $baseline
if (-not $result.Passed) {
    throw "Baseline self-test failed: $($result.Violations -join '; ')"
}

$regression = Get-TaefSummary -Text 'Summary: Total=760, Passed=758, Failed=1, Blocked=0, Not Run=0, Skipped=1'
$result = Test-TaefSummaryAgainstBaseline -Summary $regression -Baseline $baseline
if ($result.Passed) {
    throw 'Regression self-test failed to detect a new failure.'
}

Write-Host 'TAEF contract harness self-test passed.'
