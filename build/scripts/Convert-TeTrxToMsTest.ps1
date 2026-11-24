#.SYNOPSIS
# TAEF produces TRX files which differ from those produced by `dotnet test`:
# - They are lacking timing info
# - They use the keyword `Skipped` for skipped tests (surprise!)
# - For C++ tests, they use the C++ namespace separator ::
# This script fixes them.
Param(
    [Parameter(Mandatory,
      HelpMessage="Input TRX file")]
    [string]
    $Path,

    [Parameter(Mandatory,
      HelpMessage="Output Path")]
    [string]
    $OutputPath
)

$ErrorActionPreference = "Stop"

$trx=[xml](Get-Content $Path)

# Skipped -> NotExecuted
$trx.TestRun.Results.UnitTestResult | Where-Object outcome -eq "Skipped" | ForEach-Object {
    $_.outcome = "NotExecuted"
}

# TestRun.Times field
$earliestStart = ($trx.TestRun.Results.UnitTestResult | Measure -Min { [datetime]$_.startTime }).Minimum
$latestEnd = ($trx.TestRun.Results.UnitTestResult | Measure -Max { [datetime]$_.endTime }).Maximum

$start = $trx.CreateAttribute("start")
$start.InnerText = $earliestStart.ToString("o")
$finish = $trx.CreateAttribute("finish")
$finish.InnerText = $latestEnd.ToString("o")

$times = $trx.CreateElement("Times")
$null = $times.Attributes.Append($start)
$null = $times.Attributes.Append($finish)
$null = $trx.TestRun.AppendChild($times)

# Namespace generation
$trx.TestRun.TestDefinitions.UnitTest | ForEach-Object {
    If ($_.TestMethod.name -Like "*::*") {
        $c = $_.TestMethod.name -Split "::"
        $_.TestMethod.className = ($c[0..($c.Length-2)]) -Join "."
        $_.TestMethod.name = $c[$c.Length-1]
        $_.name = ($c -Join ".")
    }
}

$trx.TestRun.Results.UnitTestResult | ForEach-Object {
    If ($_.TestMethod.name -Like "*::*") {
        $c = $_.testName -Split "::"
        $_.testName = ($c -Join ".")
    }
}

# Done
$outPath = [System.IO.Path]::Combine($PWD, $OutputPath)
$trx.Save($outPath)
