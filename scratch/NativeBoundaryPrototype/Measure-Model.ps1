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
    [string]$Variant,
    [ValidateRange(1, 5)]
    [int]$Repetitions = 1
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $output | Out-Null
$pch = "$root\src\cascadia\TerminalSettingsModel\pch.h"
$hash = (Get-FileHash $pch).Hash
for ($round = 1; $round -le $Repetitions; ++$round)
{
    $prefix = "$output\$Variant-$round"
    if (Test-Path "$prefix.json") { throw "Sample already exists: $prefix" }
    $timer = [Diagnostics.Stopwatch]::StartNew()
    & $MsBuild "$root\src\cascadia\TerminalSettingsModel\Microsoft.Terminal.Settings.ModelLib.vcxproj" `
        /t:Rebuild /p:BuildProjectReferences=false "/p:SolutionDir=$root\" /p:Configuration=Debug /p:Platform=x64 `
        /p:TerminalOptimizeMetadataSearch=true /p:TerminalLeanDllWrappers=false /p:TerminalPrecompileOwnProjection=false `
        /p:CL_MPCount=4 /m:4 /p:GenerateAppxPackageOnBuild=false /p:AppxBundle=Never `
        /nologo /v:quiet /nr:false "/bl:$prefix.binlog" *> "$prefix.console.log"
    $code = $LASTEXITCODE
    $timer.Stop()
    if ($code)
    {
        Get-Content "$prefix.console.log" | Write-Host
        throw "$Variant failed: $code"
    }
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\Export-BuildTiming.ps1" `
        -Binlog "$prefix.binlog" -MsBuildDirectory (Split-Path $MsBuild) -OutputFile "$prefix.tasks.json"
    if ($LASTEXITCODE) { throw 'Binlog extraction failed' }
    $tasks = (Get-Content "$prefix.tasks.json" -Raw | ConvertFrom-Json).tasks
    if ((Get-FileHash $pch).Hash -ne $hash) { throw 'PCH source changed while measuring' }
    [ordered]@{
        variant = $Variant
        round = $round
        buildSeconds = $timer.Elapsed.TotalSeconds
        compilerTasks = @($tasks | Where-Object task -EQ 'CL')
        pchBytes = (Get-Item "$root\obj\x64\Debug\Microsoft.Terminal.Settings.Model.Lib\pch.pch").Length
        pchSourceSha256 = $hash
        metadata = & "$PSScriptRoot\Get-WinmdFingerprint.ps1" `
            -Path "$root\obj\x64\Debug\Microsoft.Terminal.Settings.Model.Lib\Merged\Microsoft.Terminal.Settings.Model.winmd"
    } | ConvertTo-Json -Depth 6 | Set-Content "$prefix.json"
    Write-Host "$Variant $round`: $([Math]::Round($timer.Elapsed.TotalSeconds, 2)) seconds"
}
