# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$MsBuild,
    [Parameter(Mandatory)]
    [string]$OutputDirectory
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path "$output\measurements.json") { throw "Measurements already exist: $output" }
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Get-Item "$root\src\cascadia\TerminalSettingsModel\Profile.idl"
$originalTime = $source.LastWriteTimeUtc
$originalHash = (Get-FileHash $source.FullName).Hash
$records = [Collections.Generic.List[object]]::new()
try
{
    for ($round = 1; $round -le 2; ++$round)
    {
        $variants = if ($round % 2) { @('baseline', 'metadata-first') } else { @('metadata-first', 'baseline') }
        foreach ($variant in $variants)
        {
            $enabled = ($variant -eq 'metadata-first').ToString().ToLowerInvariant()
            foreach ($scenario in @('no-op', 'profile-idl-timestamp'))
            {
                if ($scenario -eq 'profile-idl-timestamp')
                {
                    [IO.File]::SetLastWriteTimeUtc($source.FullName, [DateTime]::UtcNow)
                }
                $prefix = "$output\$variant-$round-$scenario"
                $timer = [Diagnostics.Stopwatch]::StartNew()
                & $MsBuild "$root\OpenConsole.slnx" '/t:Terminal\Window\WindowsTerminal' `
                    /p:Configuration=Debug /p:Platform=x64 /p:TerminalLeanDllWrappers=false `
                    /p:TerminalPrecompileOwnProjection=false "/p:TerminalOptimizeMetadataSearch=$enabled" `
                    /p:CL_MPCount=4 /m:4 /p:GenerateAppxPackageOnBuild=false /p:AppxBundle=Never `
                    /nologo /v:quiet /nr:false "/bl:$prefix.binlog" *> "$prefix.console.log"
                $code = $LASTEXITCODE
                $timer.Stop()
                if ($code)
                {
                    Get-Content "$prefix.console.log" | Write-Host
                    throw "$variant/$scenario failed: $code"
                }
                & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\Export-BuildTiming.ps1" `
                    -Binlog "$prefix.binlog" -MsBuildDirectory (Split-Path $MsBuild) -OutputFile "$prefix.tasks.json"
                if ($LASTEXITCODE) { throw "Binlog extraction failed: $prefix" }
                $tasks = (Get-Content "$prefix.tasks.json" -Raw | ConvertFrom-Json).tasks
                $merges = @($tasks | Where-Object { $_.task -eq 'Exec' -and $_.target -eq 'CppWinRTMergeProjectWinMDInputs' })
                if ($scenario -eq 'profile-idl-timestamp' -and !$merges.Count)
                {
                    throw 'The timestamp edit did not trigger metadata merging'
                }
                $record = [pscustomobject]@{
                    variant = $variant
                    scenario = $scenario
                    round = $round
                    buildSeconds = $timer.Elapsed.TotalSeconds
                    metadataMergeSeconds = if ($merges.Count) { ($merges | Measure-Object seconds -Sum).Sum } else { 0.0 }
                    metadataMergeCount = $merges.Count
                    compilerTaskCount = @($tasks | Where-Object task -EQ 'CL').Count
                }
                $records.Add($record)
                $record | ConvertTo-Json | Set-Content "$prefix.sample.json"
                Write-Host "$variant $round $scenario`: $([Math]::Round($timer.Elapsed.TotalSeconds, 2)) seconds"
            }
        }
    }
}
finally
{
    [IO.File]::SetLastWriteTimeUtc($source.FullName, $originalTime)
}
if ((Get-FileHash $source.FullName).Hash -ne $originalHash) { throw 'IDL content changed during measurement' }
[ordered]@{
    scope = 'Real WindowsTerminal incremental Build; no-op and a timestamp-only touch of Profile.idl'
    caveat = 'The IDL touch exercises metadata invalidation without changing declarations; not a semantic API edit.'
    sourceSha256 = $originalHash
    records = $records.ToArray()
} | ConvertTo-Json -Depth 5 | Set-Content "$output\measurements.json"

[ordered]@{
    scope = 'Real WindowsTerminal incremental Build after a timestamp-only touch of Profile.idl'
    variants = @('baseline', 'metadata-first')
    title = 'Real Terminal IDL-triggered build'
    subtitle = 'Timestamp-only touch of Profile.idl; declarations unchanged; Debug x64, /m:4 and /MP4'
    footer = 'Includes the real solution dependency graph; this is not a semantic API edit or a C++ implementation edit.'
    phaseNames = @('metadata merge')
    phasesTitle = 'Aggregate incremental metadata merge time'
    phasesSubtitle = 'Only merge tasks shown, not the complete build; parallel tasks can overlap'
    phasesNote = 'The IDL file contents were unchanged and its original timestamp was restored.'
    records = @($records | Where-Object scenario -EQ 'profile-idl-timestamp' | ForEach-Object {
        [pscustomobject]@{
            variant = $_.variant
            round = $_.round
            buildSeconds = $_.buildSeconds
            phases = @([pscustomobject]@{ phase = 'metadata merge'; seconds = $_.metadataMergeSeconds })
        }
    })
} | ConvertTo-Json -Depth 6 | Set-Content "$output\graph-input.json"
& node "$PSScriptRoot\Graphs.mjs" "$output\graph-input.json" "$output\graphs"
if ($LASTEXITCODE) { throw 'Graph rendering failed' }
