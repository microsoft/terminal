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
    [int]$Repetitions = 2,
    [ValidateRange(1, 16)]
    [int]$CompilerCount = 4
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$root = (Resolve-Path "$PSScriptRoot\..\..").Path
$output = [IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Force -Path $output | Out-Null
$source = Get-Item "$root\src\cascadia\TerminalSettingsModel\pch.h"
$originalTime = $source.LastWriteTimeUtc
$hash = (Get-FileHash $source.FullName).Hash
try
{
    # Settle unrelated project work and any compiler-option changes before timing.
    & $MsBuild "$root\OpenConsole.slnx" '/t:Terminal\Window\WindowsTerminal' `
        /p:Configuration=Debug /p:Platform=x64 /p:TerminalOptimizeMetadataSearch=true `
        /p:TerminalLeanDllWrappers=false /p:TerminalPrecompileOwnProjection=false `
        "/p:CL_MPCount=$CompilerCount" /m:4 /p:GenerateAppxPackageOnBuild=false /p:AppxBundle=Never `
        /nologo /v:quiet /nr:false *> "$output\$Variant-prepare.console.log"
    if ($LASTEXITCODE)
    {
        Get-Content "$output\$Variant-prepare.console.log" | Write-Host
        throw 'Product preparation failed'
    }
    for ($round = 1; $round -le $Repetitions; ++$round)
    {
        $prefix = "$output\$Variant-$round"
        if (Test-Path "$prefix.json") { throw "Sample already exists: $prefix" }
        [IO.File]::SetLastWriteTimeUtc($source.FullName, [DateTime]::UtcNow)
        $timer = [Diagnostics.Stopwatch]::StartNew()
        & $MsBuild "$root\OpenConsole.slnx" '/t:Terminal\Window\WindowsTerminal' `
            /p:Configuration=Debug /p:Platform=x64 /p:TerminalOptimizeMetadataSearch=true `
            /p:TerminalLeanDllWrappers=false /p:TerminalPrecompileOwnProjection=false `
            "/p:CL_MPCount=$CompilerCount" /m:4 /p:GenerateAppxPackageOnBuild=false /p:AppxBundle=Never `
            /nologo /v:quiet /nr:false "/bl:$prefix.binlog" *> "$prefix.console.log"
        $code = $LASTEXITCODE
        $timer.Stop()
        if ($code)
        {
            Get-Content "$prefix.console.log" | Write-Host
            throw "Product sample failed: $prefix"
        }
        & powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$PSScriptRoot\Export-BuildTiming.ps1" `
            -Binlog "$prefix.binlog" -MsBuildDirectory (Split-Path $MsBuild) -OutputFile "$prefix.tasks.json"
        if ($LASTEXITCODE) { throw 'Binlog extraction failed' }
        $tasks = (Get-Content "$prefix.tasks.json" -Raw | ConvertFrom-Json).tasks
        $compilation = @($tasks | Where-Object { $_.task -eq 'CL' -and $_.project -eq 'Microsoft.Terminal.Settings.ModelLib' })
        if (($compilation | Measure-Object seconds -Sum).Sum -lt 10) { throw 'The Model did not actually recompile' }
        $metadata = @(foreach ($file in (Get-ChildItem "$root\obj\x64\Debug" -Filter *.winmd -Recurse -File |
            Where-Object { $_.Directory.Name -eq 'Merged' } | Sort-Object FullName))
        {
            $fp = & "$PSScriptRoot\Get-WinmdFingerprint.ps1" -Path $file.FullName
            [pscustomobject]@{ path = $file.FullName.Substring($root.Length + 1); sha256 = $fp.sha256 }
        })
        [ordered]@{
            variant = $Variant
            round = $round
            compilerCount = $CompilerCount
            buildSeconds = $timer.Elapsed.TotalSeconds
            modelCompilerSeconds = ($compilation | Measure-Object seconds -Sum).Sum
            pchSourceSha256 = $hash
            pchBytes = (Get-Item "$root\obj\x64\Debug\Microsoft.Terminal.Settings.Model.Lib\pch.pch").Length
            executableSha256 = (Get-FileHash "$root\bin\x64\Debug\WindowsTerminal\WindowsTerminal.exe").Hash
            metadata = $metadata
        } | ConvertTo-Json -Depth 6 | Set-Content "$prefix.json"
        Write-Host "$Variant $round`: $([Math]::Round($timer.Elapsed.TotalSeconds, 2)) seconds"
    }
}
finally
{
    [IO.File]::SetLastWriteTimeUtc($source.FullName, $originalTime)
}
if ((Get-FileHash $source.FullName).Hash -ne $hash) { throw 'PCH source content changed during measurement' }
