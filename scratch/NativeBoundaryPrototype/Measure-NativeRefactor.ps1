# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$MsBuild,
    [Parameter(Mandatory)]
    [string]$OutputDirectory,
    [Parameter(Mandatory)]
    [ValidatePattern('^[a-z0-9-]+$')]
    [string]$Variant,
    [ValidateSet('Rebuild', 'Build', 'Touch')]
    [string]$Scenario = 'Rebuild',
    [string]$InvalidationPath,
    [ValidateRange(1, 10)]
    [int]$Repetitions = 2
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path "$output\$Variant-$Scenario-sources.json")
{
    throw 'This variant/scenario already has recorded inputs; use a fresh output directory or variant label'
}
New-Item -ItemType Directory -Force -Path $output | Out-Null
Get-Command $MsBuild -ErrorAction Stop | Out-Null
$touchFile = $null
$originalTime = $null
if ($Scenario -eq 'Touch')
{
    if (!$InvalidationPath) { throw 'Touch requires a repository-relative InvalidationPath' }
    $touchFile = Get-Item -LiteralPath (Join-Path $root $InvalidationPath)
    if ($touchFile.PSIsContainer -or !$touchFile.FullName.StartsWith("$root\", [StringComparison]::OrdinalIgnoreCase))
    {
        throw 'InvalidationPath must name a file inside this worktree'
    }
    $originalTime = $touchFile.LastWriteTimeUtc
}
elseif ($InvalidationPath)
{
    throw 'InvalidationPath is only valid with the Touch scenario'
}

function Get-SourceFingerprints
{
    $paths = @(git -C $root ls-files --cached --others --exclude-standard -- src build\rules common.openconsole.props OpenConsole.slnx)
    if ($LASTEXITCODE) { throw 'Cannot enumerate source inputs' }
    @(foreach ($path in ($paths | Sort-Object -Unique))
    {
        if ($path -notmatch '\.(cpp|c|h|hpp|idl|xaml|resw|props|targets|vcxproj|slnx)$') { continue }
        $fullPath = Join-Path $root $path
        if (!(Test-Path -LiteralPath $fullPath)) { continue }
        [pscustomobject]@{ path = $path; sha256 = (Get-FileHash -LiteralPath $fullPath).Hash }
    })
}

function Invoke-ProductBuild([string]$prefix, [bool]$rebuild)
{
    $target = if ($rebuild) { '/t:Terminal\Window\WindowsTerminal:Rebuild' } else { '/t:Terminal\Window\WindowsTerminal' }
    $timer = [Diagnostics.Stopwatch]::StartNew()
    & $MsBuild "$root\OpenConsole.slnx" $target /p:Configuration=Debug /p:Platform=x64 `
        /p:CL_MPCount=4 /m:4 /p:TerminalLeanDllWrappers=false /p:TerminalOptimizeMetadataSearch=true `
        /p:TerminalPrecompileOwnProjection=false /p:GenerateAppxPackageOnBuild=false /p:AppxBundle=Never `
        /nologo /v:quiet /nr:false "/bl:$prefix.binlog" *> "$prefix.console.log"
    $code = $LASTEXITCODE
    $timer.Stop()
    if ($code)
    {
        Get-Content "$prefix.console.log" -Tail 50 | Write-Host
        throw "Product build failed: $code ($prefix)"
    }
    $timer.Elapsed.TotalSeconds
}

$fingerprints = Get-SourceFingerprints
$fingerprints | ConvertTo-Json -Depth 3 | Set-Content "$output\$Variant-$Scenario-sources.json"
$records = [Collections.Generic.List[object]]::new()
try
{
    if ($Scenario -ne 'Rebuild')
    {
        Invoke-ProductBuild "$output\$Variant-$Scenario-prepare" $false | Out-Null
    }
    for ($round = 1; $round -le $Repetitions; ++$round)
    {
        $prefix = "$output\$Variant-$Scenario-$round"
        if (Test-Path "$prefix.sample.json") { throw "Sample already exists: $prefix" }
        if ($touchFile) { [IO.File]::SetLastWriteTimeUtc($touchFile.FullName, [DateTime]::UtcNow) }
        $seconds = Invoke-ProductBuild $prefix ($Scenario -eq 'Rebuild')
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\Export-BuildTiming.ps1" `
            -Binlog "$prefix.binlog" -MsBuildDirectory (Split-Path $MsBuild) -OutputFile "$prefix.tasks.json"
        if ($LASTEXITCODE) { throw "Binlog extraction failed: $prefix" }
        $tasks = (Get-Content "$prefix.tasks.json" -Raw | ConvertFrom-Json).tasks
        $phases = @(foreach ($task in $tasks)
        {
            $phase = switch ($task.task)
            {
                'CL' { 'compile' }
                'MIDL' { 'IDL' }
                'CompileXaml' { 'XAML' }
                'LIB' { 'archive' }
                'Link' { 'link' }
                'Exec' {
                    if ($task.target -eq 'CppWinRTMergeProjectWinMDInputs') { 'metadata merge' }
                    elseif ($task.target -match '^CppWinRTMake(Platform|Reference|Component)Projection$') { 'projection' }
                }
            }
            if ($phase) { [pscustomobject]@{ phase = $phase; seconds = $task.seconds } }
        })
        $record = [pscustomobject]@{
            variant = $Variant
            scenario = $Scenario
            round = $round
            buildSeconds = $seconds
            phases = $phases
            compilerProjects = @($tasks | Where-Object task -EQ CL | Group-Object project | ForEach-Object {
                [pscustomobject]@{ project = $_.Name; seconds = ($_.Group | Measure-Object seconds -Sum).Sum }
            })
            executableSha256 = (Get-FileHash "$root\bin\x64\Debug\WindowsTerminal\WindowsTerminal.exe").Hash
        }
        $record | ConvertTo-Json -Depth 6 | Set-Content "$prefix.sample.json"
        $records.Add($record)
        Write-Host "$Variant $Scenario $round`: $([Math]::Round($seconds, 2)) seconds"
    }
}
finally
{
    if ($touchFile) { [IO.File]::SetLastWriteTimeUtc($touchFile.FullName, $originalTime) }
}
if (($fingerprints | ConvertTo-Json -Compress) -ne ((Get-SourceFingerprints) | ConvertTo-Json -Compress))
{
    throw 'Source inputs changed during measurement'
}
[ordered]@{
    scope = 'Actual WindowsTerminal solution target and dependency closure at the current source revision'
    configuration = 'Debug x64, /m:4 /MP4; committed metadata optimization enabled; lean-wrapper/own-PCH experiments disabled'
    cache = 'Installed dependencies and OS caches retained; packaging/restore excluded; non-Rebuild scenarios have an untimed settling build'
    commit = (git -C $root rev-parse HEAD)
    invalidationPath = $InvalidationPath
    note = 'Touch changes only the timestamp, not declarations. Task sums overlap; WinMD identity is not assumed when the refactor intentionally retires IDL.'
    records = $records.ToArray()
} | ConvertTo-Json -Depth 7 | Set-Content "$output\$Variant-$Scenario-measurements.json"
