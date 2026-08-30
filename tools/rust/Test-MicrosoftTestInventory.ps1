#Requires -Version 7
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$inventoryScript = Join-Path $PSScriptRoot 'Get-MicrosoftTestInventory.ps1'
$parserTests = Join-Path $root 'src\terminal\parser\ut_parser'
$adapterTests = Join-Path $root 'src\terminal\adapter\ut_adapter'

$parserInventory = @(& $inventoryScript -Path $parserTests -Suite terminal | ConvertFrom-Json)
$adapterInventory = @(& $inventoryScript -Path $adapterTests -Suite terminal | ConvertFrom-Json)
$inventory = @($parserInventory + $adapterInventory)
if ($inventory.Count -eq 0) {
    throw 'Microsoft source test inventory is empty.'
}

function Assert-SourceMethodSet {
    param(
        [Parameter(Mandatory)]
        [object[]] $Inventory,

        [Parameter(Mandatory)]
        [string] $Source,

        [Parameter(Mandatory)]
        [string[]] $Expected
    )

    $actual = @($Inventory | Where-Object source -eq $Source | Select-Object -ExpandProperty method | Sort-Object)
    $expectedSorted = @($Expected | Sort-Object)

    if (($actual -join ',') -ne ($expectedSorted -join ',')) {
        throw "$Source inventory changed: expected $($expectedSorted -join ', '), got $($actual -join ', ')."
    }
}

function Assert-SourceMethodsPresent {
    param(
        [Parameter(Mandatory)]
        [object[]] $Inventory,

        [Parameter(Mandatory)]
        [string] $Source,

        [Parameter(Mandatory)]
        [string[]] $Expected
    )

    $actual = @($Inventory | Where-Object source -eq $Source | Select-Object -ExpandProperty method)
    $missing = @($Expected | Where-Object { $_ -notin $actual })
    if ($missing.Count -ne 0) {
        throw "$Source mapped inventory changed: missing $($missing -join ', ')."
    }
}

Assert-SourceMethodSet -Inventory $inventory -Source 'Base64Test.cpp' -Expected @(
    'DecodeFuzz',
    'DecodeUTF8'
)

Assert-SourceMethodSet -Inventory $inventory -Source 'StateMachineTest.cpp' -Expected @(
    'BulkTextPrint',
    'DcsDataStringsReceivedByHandler',
    'PassThroughUnhandled',
    'PassThroughUnhandledSplitAcrossWrites',
    'RunStorageBeforeEscape',
    'TwoStateMachinesDoNotInterfereWithEachOther',
    'VtParameterSubspanTest'
)

Assert-SourceMethodSet -Inventory $inventory -Source 'InputEngineTest.cpp' -Expected @(
    'AlphanumericTest',
    'AltBackspaceEnterTest',
    'AltBackspaceTest',
    'AltCtrlDTest',
    'AltIntermediateTest',
    'C0Test',
    'CSICursorBackTabTest',
    'ChunkedSequence',
    'CtrlAltZCtrlAltXTest',
    'CursorPositioningTest',
    'EnhancedKeysTest',
    'NonAsciiTest',
    'RoundTripTest',
    'SGRMouseTest_ButtonClick',
    'SGRMouseTest_DoubleClick',
    'SGRMouseTest_Hover',
    'SGRMouseTest_Modifiers',
    'SGRMouseTest_Movement',
    'SGRMouseTest_Scroll',
    'SS3CursorKeyTest',
    'TestSs3Entry',
    'TestSs3Immediate',
    'TestSs3Param',
    'TestWin32InputOptionals',
    'TestWin32InputParsing'
)

Assert-SourceMethodSet -Inventory $inventory -Source 'OutputEngineTest.cpp' -Expected @(
    'NormalTestOscParam',
    'TestAddHyperlink',
    'TestAnsiMode',
    'TestC1CsiEntry',
    'TestC1DcsEntry',
    'TestC1Osc',
    'TestC1ParserMode',
    'TestC1StringTerminator',
    'TestControlCharacters',
    'TestCsiCursorMovementWithValues',
    'TestCsiCursorMovementWithoutValues',
    'TestCsiCursorPosition',
    'TestCsiCursorPositionWithOnlyRow',
    'TestCsiEntry',
    'TestCsiIgnore',
    'TestCsiImmediate',
    'TestCsiMaxParamCount',
    'TestCsiMaxSubParamCount',
    'TestCsiParam',
    'TestCsiSubParam',
    'TestCursorSaveLoad',
    'TestDeviceAttributes',
    'TestDeviceStatusReport',
    'TestDcsEntry',
    'TestDcsIgnore',
    'TestDcsImmediate',
    'TestDcsIntermediateAndPassThrough',
    'TestDcsInvalidTermination',
    'TestDcsLongStringPassThrough',
    'TestDcsParam',
    'TestErase',
    'TestEscapeImmediatePath',
    'TestEscapePath',
    'TestEscapeThenC0Path',
    'TestGroundPrint',
    'TestIdentifyDeviceReport',
    'TestLeadingZeroCsiParam',
    'TestLeadingZeroCsiSubParam',
    'TestLeadingZeroOscParam',
    'TestLineFeed',
    'TestLongOscParam',
    'TestLongOscString',
    'TestMultipleErase',
    'TestMultipleModes',
    'TestOscColorTableReset',
    'TestOscGetColorTableEntry',
    'TestOscSetColorTableEntry',
    'TestOscSetDefaultBackground',
    'TestOscSetDefaultForeground',
    'TestOscSetWindowTitle',
    'TestOscStringInvalidTermination',
    'TestOscStringSimple',
    'TestOscXtermResourceReport',
    'TestOscXtermResourceReset',
    'TestPrivateModes',
    'TestRequestTerminalParameters',
    'TestSecondaryDeviceAttributes',
    'TestSetClipboard',
    'TestSetGraphicsRendition',
    'TestSosPmApcString',
    'TestStrings',
    'TestTabClear',
    'TestTertiaryDeviceAttributes',
    'TestVt52Sequences'
)

Assert-SourceMethodSet -Inventory $inventory -Source 'MouseInputTest.cpp' -Expected @(
    'AlternateScrollModeTests',
    'DefaultModeTests',
    'ScrollWheelTests',
    'SgrModeTests',
    'Utf8ModeTests'
)

Assert-SourceMethodSet -Inventory $inventory -Source 'inputTest.cpp' -Expected @(
    'AutoRepeatModeTest',
    'BackarrowKeyModeTest',
    'CtrlNumTest',
    'DifferentModifiersTest',
    'SendC1ControlTest',
    'TerminalInputModifierKeyTests',
    'TerminalInputNullKeyTests',
    'TerminalInputTests',
    'TestFocusEvents'
)

# adapterTest.cpp mixes deterministic R03 semantics with TextBuffer, renderer,
# terminal input, and host/platform contracts. Lock only the source identities
# that have concrete Rust evidence instead of pretending the whole class has
# migrated equivalence.
Assert-SourceMethodsPresent -Inventory $inventory -Source 'adapterTest.cpp' -Expected @(
    'CursorMovementTest',
    'CursorPositionTest',
    'CursorSaveRestoreTest',
    'CursorSingleDimensionMoveTest',
    'MacroDefinitions',
    'PageMovementTests'
)

$duplicates = @($inventory | Group-Object suite, source, method | Where-Object Count -gt 1)
if ($duplicates.Count -ne 0) {
    throw 'Microsoft source test inventory contains duplicate method identities.'
}

Write-Host "Microsoft source inventory self-test passed ($($inventory.Count) terminal source methods)."
