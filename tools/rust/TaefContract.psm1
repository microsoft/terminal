Set-StrictMode -Version Latest

function Get-TaefSummary {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Text
    )

    $pattern = 'Summary:\s+Total=(?<Total>\d+),\s+Passed=(?<Passed>\d+),\s+Failed=(?<Failed>\d+),\s+Blocked=(?<Blocked>\d+),\s+Not Run=(?<NotRun>\d+),\s+Skipped=(?<Skipped>\d+)'
    $matches = [regex]::Matches($Text, $pattern)

    if ($matches.Count -eq 0) {
        throw 'No TAEF summary was found in the captured output.'
    }

    $match = $matches[$matches.Count - 1]

    [pscustomobject]@{
        Total   = [int]$match.Groups['Total'].Value
        Passed  = [int]$match.Groups['Passed'].Value
        Failed  = [int]$match.Groups['Failed'].Value
        Blocked = [int]$match.Groups['Blocked'].Value
        NotRun  = [int]$match.Groups['NotRun'].Value
        Skipped = [int]$match.Groups['Skipped'].Value
    }
}

function Get-TaefTestInventory {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [string] $Text
    )

    # /listProperties emits a hierarchy of binary -> class -> test invocation,
    # followed by more deeply-indented property/data lines. Test invocations
    # are therefore the leaf names in that hierarchy. This also preserves the
    # #<index>/#metadataSet<index> suffixes used by data-driven TAEF tests.
    $candidates = [System.Collections.Generic.List[object]]::new()

    foreach ($line in ($Text -split "`r?`n")) {
        $match = [regex]::Match($line, '^(?<Indent>\s+)(?<Name>\S.*)$')
        if (-not $match.Success) {
            continue
        }

        $name = $match.Groups['Name'].Value.TrimEnd()
        if (
            $name -match '^(Property|Data)\[' -or
            $name -match '^(Setup|Teardown):' -or
            $name -match '\.dll$'
        ) {
            continue
        }

        $indent = $match.Groups['Indent'].Value.Replace("`t", '    ').Length
        $candidates.Add([pscustomobject]@{
            Indent = $indent
            Name   = $name
        })
    }

    $inventory = [System.Collections.Generic.List[string]]::new()
    for ($i = 0; $i -lt $candidates.Count; $i++) {
        $current = $candidates[$i]
        $nextIndent = if ($i + 1 -lt $candidates.Count) {
            $candidates[$i + 1].Indent
        }
        else {
            -1
        }

        if ($nextIndent -le $current.Indent) {
            $inventory.Add([string]$current.Name)
        }
    }

    $inventory.ToArray()
}

function Test-TaefSummaryAgainstBaseline {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [psobject] $Summary,

        [Parameter(Mandatory)]
        [psobject] $Baseline
    )

    $violations = [System.Collections.Generic.List[string]]::new()

    if ($Summary.Total -ne [int]$Baseline.total) {
        $violations.Add("Total changed: expected $($Baseline.total), got $($Summary.Total).")
    }
    if ($Summary.Failed -gt [int]$Baseline.maxFailed) {
        $violations.Add("Failed regressed: ceiling $($Baseline.maxFailed), got $($Summary.Failed).")
    }
    if ($Summary.Blocked -gt [int]$Baseline.maxBlocked) {
        $violations.Add("Blocked regressed: ceiling $($Baseline.maxBlocked), got $($Summary.Blocked).")
    }
    if ($Summary.NotRun -gt [int]$Baseline.maxNotRun) {
        $violations.Add("NotRun regressed: ceiling $($Baseline.maxNotRun), got $($Summary.NotRun).")
    }
    if ($Summary.Skipped -gt [int]$Baseline.maxSkipped) {
        $violations.Add("Skipped regressed: ceiling $($Baseline.maxSkipped), got $($Summary.Skipped).")
    }

    [pscustomobject]@{
        Passed     = $violations.Count -eq 0
        Violations = @($violations)
    }
}

Export-ModuleMember -Function Get-TaefSummary, Get-TaefTestInventory, Test-TaefSummaryAgainstBaseline
