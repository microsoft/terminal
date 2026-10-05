# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$OutputDirectory,
    [Parameter(Mandatory)]
    [string]$MsBuild,
    [ValidateRange(1, 10)]
    [int]$Repetitions = 2,
    [ValidateSet('OwnProjection', 'MetadataSearch')]
    [string]$Experiment = 'OwnProjection'
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path "$output\measurements.json")
{
    throw "Measurements already exist: $output"
}
if (!(Test-Path "$root\bin\x64\Debug\WindowsTerminal\WindowsTerminal.exe"))
{
    throw 'First build the solution target Terminal\Window\WindowsTerminal in Debug x64.'
}
Get-Command $MsBuild -ErrorAction Stop | Out-Null
Get-Command node.exe -ErrorAction Stop | Out-Null
$msbuildDirectory = Split-Path $MsBuild
New-Item -ItemType Directory -Force -Path $output | Out-Null

$inputs = @(
    "$root\src\cppwinrt.build.pre.props",
    "$root\src\common.nugetversions.targets",
    "$root\build\rules\Microsoft.Windows.CppWinRT.Additional.targets",
    "$root\src\cascadia\TerminalSettingsModel\pch.h",
    "$root\src\cascadia\TerminalSettingsEditor\pch.h",
    "$root\src\cascadia\TerminalApp\pch.h",
    "$PSScriptRoot\Measure-Terminal.ps1",
    "$PSScriptRoot\Export-BuildTiming.ps1",
    "$PSScriptRoot\Get-WinmdFingerprint.ps1",
    "$PSScriptRoot\Graphs.mjs"
)
function Get-InputHashes
{
    @(foreach ($file in $inputs)
    {
        [pscustomobject]@{ path = $file.Substring($root.Length + 1); sha256 = (Get-FileHash $file).Hash }
    })
}
$inputHashes = Get-InputHashes
$records = [Collections.Generic.List[object]]::new()
$optimizedVariant = if ($Experiment -eq 'MetadataSearch') { 'metadata-first' } else { 'own-pch' }

for ($round = 1; $round -le $Repetitions; ++$round)
{
    $variants = if ($round % 2) { @('baseline', $optimizedVariant) } else { @($optimizedVariant, 'baseline') }
    foreach ($variant in $variants)
    {
        $ownProjection = if ($variant -eq 'own-pch') { 'true' } else { 'false' }
        $metadataSearch = if ($variant -eq 'metadata-first') { 'true' } else { 'false' }
        $phases = [Collections.Generic.List[object]]::new()
        $stages = [Collections.Generic.List[object]]::new()
        $prefix = "$output\$variant-$round"
        $timer = [Diagnostics.Stopwatch]::StartNew()
        & $MsBuild "$root\OpenConsole.slnx" '/t:Terminal\Window\WindowsTerminal:Rebuild' `
            /p:Configuration=Debug /p:Platform=x64 /p:TerminalLeanDllWrappers=false `
            "/p:TerminalPrecompileOwnProjection=$ownProjection" "/p:TerminalOptimizeMetadataSearch=$metadataSearch" `
            /p:CL_MPCount=4 /m:4 `
            /p:GenerateAppxPackageOnBuild=false /p:AppxBundle=Never /nologo /v:quiet /nr:false `
            "/bl:$prefix.binlog" /fl "/flp:logfile=$prefix.log;verbosity=normal;performancesummary" *> "$prefix.console.log"
        $code = $LASTEXITCODE
        $timer.Stop()
        if ($code)
        {
            Get-Content "$prefix.console.log" | Write-Host
            throw "$variant round $round failed: exit $code"
        }
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\Export-BuildTiming.ps1" `
            -Binlog "$prefix.binlog" -MsBuildDirectory $msbuildDirectory -OutputFile "$prefix.tasks.json"
        if ($LASTEXITCODE) { throw "Binlog extraction failed for $prefix" }
        $timings = Get-Content "$prefix.tasks.json" -Raw | ConvertFrom-Json
        foreach ($task in $timings.tasks)
        {
            $phase = switch ($task.task)
            {
                'CL' { 'compile'; break }
                'LIB' { 'archive'; break }
                'Link' { 'link'; break }
                'Exec' {
                    if ($task.target -match '^CppWinRTMake(Platform|Reference|Component)Projection$') { 'projection' }
                    elseif ($task.target -eq 'CppWinRTMergeProjectWinMDInputs') { 'metadata merge' }
                    break
                }
            }
            if ($phase) { $phases.Add([pscustomobject]@{ phase = $phase; seconds = $task.seconds }) }
        }
        foreach ($project in ($timings.tasks | Where-Object task -EQ CL | Group-Object project))
        {
            $stages.Add([pscustomobject]@{
                name = $project.Name
                compileSeconds = ($project.Group | Measure-Object seconds -Sum).Sum
            })
        }
        $exe = "$root\bin\x64\Debug\WindowsTerminal\WindowsTerminal.exe"
        if (!(Test-Path $exe) -or (Get-Item $exe).Length -eq 0) { throw 'Missing real WindowsTerminal.exe output' }
        $metadata = @(foreach ($file in (Get-ChildItem "$root\obj\x64\Debug" -Filter *.winmd -Recurse -File |
            Where-Object { $_.Directory.Name -eq 'Merged' } | Sort-Object FullName))
        {
            $fingerprint = & "$PSScriptRoot\Get-WinmdFingerprint.ps1" -Path $file.FullName
            [pscustomobject]@{
                path = $file.FullName.Substring($root.Length + 1)
                metadataBytes = $fingerprint.metadataBytes
                sha256 = $fingerprint.sha256
            }
        })
        if (!$metadata.Count) { throw 'No merged product metadata was found' }
        $record = [pscustomobject]@{
            variant = $variant
            round = $round
            buildSeconds = $timer.Elapsed.TotalSeconds
            phases = $phases.ToArray()
            stages = $stages.ToArray()
            executableSha256 = (Get-FileHash $exe).Hash
            metadata = $metadata
        }
        $record | ConvertTo-Json -Depth 8 | Set-Content "$prefix.sample.json" -Encoding utf8
        if ($records.Count -and ($records[0].metadata | ConvertTo-Json -Compress) -ne ($metadata | ConvertTo-Json -Compress))
        {
            throw "Merged metadata changed beyond module version IDs: $prefix"
        }
        $records.Add($record)
        Write-Host "$variant $round`: $([Math]::Round($timer.Elapsed.TotalSeconds, 2)) seconds"
    }
}
if (($inputHashes | ConvertTo-Json -Compress) -ne ((Get-InputHashes) | ConvertTo-Json -Compress))
{
    throw 'Measured inputs changed during the experiment'
}
[ordered]@{
    scope = 'Real Terminal\Window\WindowsTerminal solution-target Rebuild including dependency closure'
    variants = @('baseline', $optimizedVariant)
    title = 'Real Windows Terminal source rebuild'
    subtitle = 'Debug x64; full WindowsTerminal solution-target Rebuild, /m:4 and /MP4'
    footer = if ($Experiment -eq 'MetadataSearch') {
        'Same code, projections, compiler settings and metadata validation; only SDK search-directory order changes.'
    } else {
        'Native interfaces are not substituted: only own-component projected headers are added to production PCHs.'
    }
    phaseNames = @('metadata merge', 'projection', 'compile', 'archive', 'link')
    phasesTitle = 'Aggregate real-build task time'
    phasesSubtitle = 'Task wall times summed across projects; parallel tasks overlap, so this is not elapsed build time'
    phasesNote = 'CL includes PCH creation and source compilation. Build wall time excludes binlog extraction.'
    configuration = 'Debug x64, four MSBuild nodes, /MP4, existing production PCHs'
    cache = 'Solution-target Rebuild; installed NuGet/vcpkg dependencies and OS caches retained; packaging disabled'
    commit = (git -C $root rev-parse HEAD)
    inputHashes = $inputHashes
    records = $records.ToArray()
} | ConvertTo-Json -Depth 8 | Set-Content "$output\measurements.json" -Encoding utf8
& node "$PSScriptRoot\Graphs.mjs" "$output\measurements.json" $output
if ($LASTEXITCODE) { throw 'Graph rendering failed' }
