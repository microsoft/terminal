# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 7
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$OutputDirectory,

    [ValidateRange(1, 20)]
    [int]$Repetitions = 3,

    [string]$SdkVersion = '10.0.26100.0'
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

foreach ($tool in @('cl.exe', 'lib.exe', 'link.exe', 'node.exe'))
{
    Get-Command $tool -ErrorAction Stop | Out-Null
}
$cppwinrt = "$root\packages\Microsoft.Windows.CppWinRT.3.0.260818.1\bin\cppwinrt.exe"
$modelDll = "$root\bin\x64\Debug\Microsoft.Terminal.Settings.Model\Microsoft.Terminal.Settings.Model.dll"
$metadata = @(
    "$root\bin\x64\Debug\Microsoft.Terminal.Settings.Model\Microsoft.Terminal.Settings.Model.winmd",
    "$root\bin\x64\Debug\TerminalCore\Microsoft.Terminal.Core.winmd",
    "$root\bin\x64\Debug\Microsoft.Terminal.Control\Microsoft.Terminal.Control.winmd",
    "$root\bin\x64\Debug\TerminalConnection\Microsoft.Terminal.TerminalConnection.winmd",
    "$root\bin\x64\Debug\Microsoft.Terminal.UI\Microsoft.Terminal.UI.winmd",
    "$root\packages\Microsoft.UI.Xaml.2.8.4\lib\uap10.0\Microsoft.UI.Xaml.winmd",
    "$root\packages\Microsoft.Web.WebView2.1.0.1661.34\lib\Microsoft.Web.WebView2.Core.winmd"
)
foreach ($path in @($cppwinrt, $modelDll) + $metadata)
{
    if (!(Test-Path $path))
    {
        throw "Missing prerequisite: $path. Build the Debug x64 settings model subtree first."
    }
}

$inputFiles = @(
    "$PSScriptRoot\Boundary.h", "$PSScriptRoot\Model.cpp", "$PSScriptRoot\Editor.cpp",
    "$PSScriptRoot\App.cpp", "$PSScriptRoot\Dll.cpp", "$PSScriptRoot\Host.cpp",
    "$PSScriptRoot\Measure.ps1", "$PSScriptRoot\Graphs.mjs", $cppwinrt, $modelDll
) + $metadata
function Get-InputHashes
{
    @(foreach ($file in $inputFiles)
    {
        [pscustomobject]@{
            path = $file.Substring($root.Length + 1)
            sha256 = (Get-FileHash $file -Algorithm SHA256).Hash
        }
    })
}
$inputHashes = Get-InputHashes
$records = [Collections.Generic.List[object]]::new()

function Invoke-MeasuredTool
{
    param([string]$Tool, [string[]]$Arguments, [string]$Log, [string]$Phase)
    $timer = [Diagnostics.Stopwatch]::StartNew()
    & $Tool @Arguments *> $Log
    $code = $LASTEXITCODE
    $timer.Stop()
    if ($code -ne 0)
    {
        Get-Content $Log | Write-Host
        throw "$Tool failed with exit code $code. See $Log"
    }
    [pscustomobject]@{ phase = $Phase; seconds = $timer.Elapsed.TotalSeconds }
}

$sources = @('Model', 'Editor', 'App', 'Dll', 'Host')
for ($round = 1; $round -le $Repetitions; ++$round)
{
    # Rotate order so that each variant runs first once in a three-round experiment.
    $allVariants = @('projected', 'shared', 'native')
    $offset = ($round - 1) % $allVariants.Count
    $variants = @(for ($i = 0; $i -lt $allVariants.Count; ++$i) { $allVariants[($i + $offset) % $allVariants.Count] })
    foreach ($variant in $variants)
    {
        $run = "$output\$variant-$round"
        if (Test-Path $run) { throw "Run directory already exists: $run" }
        New-Item -ItemType Directory -Path $run | Out-Null
        $phases = [Collections.Generic.List[object]]::new()
        $timer = [Diagnostics.Stopwatch]::StartNew()
        foreach ($stage in $sources)
        {
            $generated = if ($variant -eq 'shared') { "$run\Model-generated" } else { "$run\$stage-generated" }
            if ($variant -eq 'projected' -or $stage -eq 'Model')
            {
                $arguments = @('-input', $SdkVersion)
                foreach ($winmd in $metadata) { $arguments += @('-input', $winmd) }
                $arguments += @('-output', $generated)
                $phases.Add((Invoke-MeasuredTool $cppwinrt $arguments "$run\$stage-projection.log" 'projection'))
            }

            $arguments = @(
                '/nologo', '/c', '/std:c++20', '/EHsc', '/permissive-', '/MDd', '/Od', '/Zi',
                '/W4', '/WX', '/bigobj', '/DWINRT_LEAN_AND_MEAN', '/DNOMINMAX',
                '/DWIN32_LEAN_AND_MEAN', "/I$PSScriptRoot", "/I$generated",
                "/Fo$run\$stage.obj", "/Fd$run\$stage.pdb", "$PSScriptRoot\$stage.cpp"
            )
            if ($variant -eq 'native') { $arguments += '/DNATIVE_BOUNDARY' }
            if ($stage -eq 'Dll') { $arguments += '/DBUILD_BOUNDARY_DLL' }
            $phases.Add((Invoke-MeasuredTool 'cl.exe' $arguments "$run\$stage-compile.log" 'compile'))
            if ($stage -in @('Model', 'Editor', 'App'))
            {
                $phases.Add((Invoke-MeasuredTool 'lib.exe' @('/nologo', "/OUT:$run\$stage.lib", "$run\$stage.obj") "$run\$stage-archive.log" 'archive'))
            }
        }
        $arguments = @(
            '/nologo', '/DLL', '/DEBUG', "/OUT:$run\Boundary.dll", "/IMPLIB:$run\Boundary.lib",
            "$run\Dll.obj", "$run\Model.lib", "$run\Editor.lib", "$run\App.lib", 'windowsapp.lib'
        )
        $phases.Add((Invoke-MeasuredTool 'link.exe' $arguments "$run\dll-link.log" 'link'))
        $phases.Add((Invoke-MeasuredTool 'link.exe' @(
            '/nologo', '/DEBUG', "/OUT:$run\BoundaryHost.exe", "$run\Host.obj",
            "$run\Boundary.lib", 'windowsapp.lib', 'ole32.lib'
        ) "$run\host-link.log" 'link'))
        $timer.Stop()
        $test = Invoke-MeasuredTool "$run\BoundaryHost.exe" @($modelDll) "$run\round-trip.log" 'test'
        $records.Add([pscustomobject]@{
            variant = $variant
            round = $round
            buildSeconds = $timer.Elapsed.TotalSeconds
            phases = $phases.ToArray()
            testSeconds = $test.seconds
            projectionPasses = $(if ($variant -eq 'projected') { 5 } else { 1 })
        })
        Write-Host "$variant $round`: $([Math]::Round($timer.Elapsed.TotalSeconds, 2)) seconds"
    }
}

if (($inputHashes | ConvertTo-Json -Compress) -ne ((Get-InputHashes) | ConvertTo-Json -Compress))
{
    throw 'Sources, model binary or metadata changed during measurement; discard these runs and retry.'
}

$machine = Get-CimInstance Win32_ComputerSystem
[ordered]@{
    scope = 'Isolated real-NewTerminalArgs boundary slice; NOT a full Terminal build'
    variants = @('projected', 'shared', 'native')
    title = 'Projection vs native boundary: build time'
    subtitle = 'Isolated real-model slice, not WindowsTerminal.exe; fresh outputs, warm OS caches'
    footer = 'Same three static libraries, final DLL and host; model implementation remains WinRT internally.'
    phasesNote = 'projected = 5 generation passes; shared/native = 1. No PCH in any variant.'
    configuration = 'x64 Debug-like /MDd /Od /Zi, no PCH, serial tools'
    cache = 'Fresh generated headers and objects each run; warm OS/dependency caches'
    commit = (git -C $root rev-parse HEAD)
    sdk = $SdkVersion
    compiler = $env:VCToolsVersion
    inputHashes = $inputHashes
    logicalProcessors = $machine.NumberOfLogicalProcessors
    memoryBytes = $machine.TotalPhysicalMemory
    records = $records.ToArray()
} | ConvertTo-Json -Depth 8 | Set-Content "$output\measurements.json" -Encoding utf8

& node "$PSScriptRoot\Graphs.mjs" "$output\measurements.json" $output
if ($LASTEXITCODE -ne 0) { throw 'Graph generation failed' }
