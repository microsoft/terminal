# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$OutputDirectory,

    [ValidateRange(1, 20)]
    [int]$Repetitions = 3,

    [string]$MsBuild = 'msbuild.exe'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path "$output\measurements.json")
{
    throw "Output already contains measurements: $output. Select a new output directory."
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
Get-Command $MsBuild -ErrorAction Stop | Out-Null
$project = "$root\src\cascadia\TerminalSettingsModel\dll\Microsoft.Terminal.Settings.Model.vcxproj"
$modelLibrary = "$root\bin\x64\Debug\Microsoft.Terminal.Settings.Model.Lib\Microsoft.Terminal.Settings.Model.Lib.lib"
if (!(Test-Path $modelLibrary))
{
    throw "Missing $modelLibrary. Build the Debug x64 settings model subtree first."
}

$records = [Collections.Generic.List[object]]::new()
for ($round = 1; $round -le $Repetitions; ++$round)
{
    $variants = if ($round % 2) { @('baseline', 'lean') } else { @('lean', 'baseline') }
    foreach ($variant in $variants)
    {
        $prefix = "$output\$variant-$round"
        $enabled = if ($variant -eq 'lean') { 'true' } else { 'false' }
        $timer = [Diagnostics.Stopwatch]::StartNew()
        & $MsBuild $project /t:Rebuild /p:Configuration=Debug /p:Platform=x64 "/p:SolutionDir=$root\" `
            /p:BuildProjectReferences=false "/p:TerminalLeanDllWrappers=$enabled" /m:1 /nologo /v:quiet /nr:false `
            "/bl:$prefix.binlog" /fl "/flp:logfile=$prefix.log;verbosity=normal;performancesummary" *> "$prefix.console.log"
        $code = $LASTEXITCODE
        $timer.Stop()
        if ($code -ne 0)
        {
            Get-Content "$prefix.console.log" | Write-Host
            throw "Wrapper rebuild failed with exit code $code. See $prefix.log"
        }

        $text = Get-Content "$prefix.log" -Raw
        $targetSummary = ($text -split 'Target Performance Summary:', 2)[1] -split 'Task Performance Summary:', 2
        $phases = [Collections.Generic.List[object]]::new()
        foreach ($match in [regex]::Matches($targetSummary[0], '(?m)^\s*(\d+)\s+ms\s+(\S+)\s+\d+\s+calls'))
        {
            $name = $match.Groups[2].Value
            $phase = switch -Regex ($name)
            {
                '^CppWinRTMake(Platform|Reference|Component)Projection$' { 'projection'; break }
                '^Link$' { 'link'; break }
                '^ClCompile$' { 'compile'; break }
            }
            if ($phase)
            {
                $phases.Add([pscustomobject]@{ phase = $phase; seconds = [double]$match.Groups[1].Value / 1000 })
            }
        }
        $records.Add([pscustomobject]@{
            variant = $variant
            round = $round
            buildSeconds = $timer.Elapsed.TotalSeconds
            phases = $phases.ToArray()
        })
        Write-Host "$variant $round`: $([Math]::Round($timer.Elapsed.TotalSeconds, 2)) seconds"
    }
}

[ordered]@{
    scope = 'Real settings model DLL wrapper only, static-library dependencies already built'
    variants = @('baseline', 'lean')
    title = 'Real settings-model DLL wrapper rebuild'
    subtitle = 'MSBuild Rebuild with BuildProjectReferences=false; NOT a clean full Terminal build'
    footer = 'Metadata merge, DLL linking and runtime API preserved; only unused projection generation is disabled.'
    phasesNote = 'MSBuild target durations in baseline / lean order; excludes unclassified work and process startup.'
    commit = (git -C $root rev-parse HEAD)
    records = $records.ToArray()
} | ConvertTo-Json -Depth 8 | Set-Content "$output\measurements.json" -Encoding utf8

& node "$PSScriptRoot\Graphs.mjs" "$output\measurements.json" $output
if ($LASTEXITCODE -ne 0) { throw 'Graph generation failed' }
