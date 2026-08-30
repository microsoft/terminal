#Requires -Version 7
$ErrorActionPreference = 'Stop'

$root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$manifest = Get-Content -Raw (Join-Path $PSScriptRoot 'microsoft-test-suites.json') | ConvertFrom-Json -AsHashtable
$inventoryScript = Join-Path $PSScriptRoot 'Get-MicrosoftTestInventory.ps1'
$inventory = [System.Collections.Generic.List[object]]::new()

foreach ($suite in @($manifest.suites.Keys | Sort-Object)) {
    foreach ($relativeRoot in @($manifest.suites[$suite].sourceRoots)) {
        $sourceRoot = Join-Path $root $relativeRoot
        if (-not (Test-Path -LiteralPath $sourceRoot -PathType Container)) {
            throw "$suite source root does not exist: $relativeRoot"
        }
        $raw = (& $inventoryScript -Path $sourceRoot -Suite $suite | Out-String).Trim()
        if ($raw.Length -eq 0) {
            continue
        }
        foreach ($item in @($raw | ConvertFrom-Json)) {
            $inventory.Add($item)
        }
    }
}

$duplicates = @($inventory | Group-Object suite, source, method | Where-Object Count -gt 1)
if ($duplicates.Count -ne 0) {
    throw 'Microsoft global source inventory contains duplicate method identities.'
}

$inventory | Sort-Object suite, source, method | ConvertTo-Json -Depth 4
