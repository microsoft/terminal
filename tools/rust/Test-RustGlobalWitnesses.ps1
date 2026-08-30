#Requires -Version 7
$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot '../..')
$ledgerFiles = @(
    Get-Item (Join-Path $PSScriptRoot 'microsoft-rust-equivalence.json')
) + @(Get-ChildItem -Path $PSScriptRoot -Filter 'microsoft-rust-equivalence-*.json' -File | Sort-Object Name)

$rustFiles = @(Get-ChildItem -Path (Join-Path $repoRoot 'rust') -Filter '*.rs' -File -Recurse)
if ($rustFiles.Count -eq 0) {
    throw 'Rust source inventory is empty.'
}
$rustText = ($rustFiles | ForEach-Object { Get-Content -Raw $_.FullName }) -join "`n"

$semanticCoverage = @('Exact', 'Stronger', 'Partial')
$witnessCount = 0
$semanticContractCount = 0
$uniqueWitnesses = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)

foreach ($ledgerFile in $ledgerFiles) {
    $ledger = Get-Content -Raw $ledgerFile.FullName | ConvertFrom-Json -AsHashtable
    foreach ($collectionName in @('entries', 'sourceRules')) {
        if (-not $ledger.ContainsKey($collectionName)) { continue }
        foreach ($item in @($ledger[$collectionName])) {
            if ($item.coverage -notin $semanticCoverage) { continue }
            $semanticContractCount++
            $witnesses = @($item.rustWitnesses)
            if ($witnesses.Count -eq 0) {
                $identity = if ($collectionName -eq 'entries') {
                    "$($item.suite)|$($item.source)|$($item.method)"
                }
                else {
                    "$($item.suite)|$($item.source)"
                }
                throw "Semantic coverage requires at least one Rust witness: $identity ($($item.coverage))"
            }

            foreach ($witness in $witnesses) {
                if ([string]::IsNullOrWhiteSpace([string]$witness)) {
                    throw "Blank Rust witness in $($ledgerFile.Name)."
                }
                $witnessCount++
                [void]$uniqueWitnesses.Add([string]$witness)

                if ($witness.StartsWith('file:')) {
                    $relativePath = $witness.Substring(5)
                    $fullPath = Join-Path $repoRoot $relativePath
                    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
                        throw "Rust witness file does not exist: $witness ($($ledgerFile.Name))"
                    }
                    continue
                }

                $leaf = ([string]$witness -split '::')[-1]
                if ([string]::IsNullOrWhiteSpace($leaf)) {
                    throw "Rust witness has no searchable leaf: $witness ($($ledgerFile.Name))"
                }
                $pattern = "(?m)\b$([regex]::Escape($leaf))\b"
                if (-not [regex]::IsMatch($rustText, $pattern)) {
                    throw "Rust witness was not found in the Rust source tree: $witness ($($ledgerFile.Name))"
                }
            }
        }
    }
}

Write-Host "Rust global witness gate passed (semantic contracts/rules=$semanticContractCount; witness references=$witnessCount; unique witnesses=$($uniqueWitnesses.Count); Rust files=$($rustFiles.Count))."
