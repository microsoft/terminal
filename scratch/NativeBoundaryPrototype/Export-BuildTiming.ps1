# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.

#Requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Binlog,
    [Parameter(Mandatory)]
    [string]$MsBuildDirectory,
    [Parameter(Mandatory)]
    [string]$OutputFile
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$script:assemblyDirectory = (Resolve-Path $MsBuildDirectory).Path
[AppDomain]::CurrentDomain.add_AssemblyResolve([ResolveEventHandler]{
    param($sender, $eventArgs)
    $name = ([Reflection.AssemblyName]$eventArgs.Name).Name
    $path = Join-Path $script:assemblyDirectory "$name.dll"
    if (Test-Path -LiteralPath $path)
    {
        return [Reflection.Assembly]::LoadFrom($path)
    }
    return $null
})
[Reflection.Assembly]::LoadFrom("$script:assemblyDirectory\Microsoft.Build.Framework.dll") | Out-Null
[Reflection.Assembly]::LoadFrom("$script:assemblyDirectory\Microsoft.Build.dll") | Out-Null

$script:starts = @{}
$script:targets = @{}
$script:tasks = New-Object 'Collections.Generic.List[object]'
$script:buildStart = $null
$script:buildFinish = $null
$script:success = $false
$replay = New-Object Microsoft.Build.Logging.BinaryLogReplayEventSource
$replay.add_BuildStarted([Microsoft.Build.Framework.BuildStartedEventHandler]{
    param($sender, $eventArgs)
    $script:buildStart = $eventArgs.Timestamp
})
$replay.add_BuildFinished([Microsoft.Build.Framework.BuildFinishedEventHandler]{
    param($sender, $eventArgs)
    $script:buildFinish = $eventArgs.Timestamp
    $script:success = $eventArgs.Succeeded
})
$replay.add_TargetStarted([Microsoft.Build.Framework.TargetStartedEventHandler]{
    param($sender, $eventArgs)
    $c = $eventArgs.BuildEventContext
    $script:targets["$($c.NodeId):$($c.ProjectContextId):$($c.TargetId)"] = $eventArgs.TargetName
})
$replay.add_TaskStarted([Microsoft.Build.Framework.TaskStartedEventHandler]{
    param($sender, $eventArgs)
    $c = $eventArgs.BuildEventContext
    $script:starts["$($c.NodeId):$($c.ProjectContextId):$($c.TargetId):$($c.TaskId)"] = $eventArgs
})
$replay.add_TaskFinished([Microsoft.Build.Framework.TaskFinishedEventHandler]{
    param($sender, $eventArgs)
    $c = $eventArgs.BuildEventContext
    $key = "$($c.NodeId):$($c.ProjectContextId):$($c.TargetId):$($c.TaskId)"
    if (!$script:starts.ContainsKey($key)) { throw "Missing task start: $key" }
    $start = $script:starts[$key]
    $seconds = ($eventArgs.Timestamp - $start.Timestamp).TotalSeconds
    if ($seconds -lt 0) { throw "Negative task duration: $key" }
    $script:tasks.Add([pscustomobject]@{
        project = [IO.Path]::GetFileNameWithoutExtension($start.ProjectFile)
        task = $start.TaskName
        target = $script:targets["$($c.NodeId):$($c.ProjectContextId):$($c.TargetId)"]
        seconds = $seconds
        succeeded = $eventArgs.Succeeded
    })
    $script:starts.Remove($key)
})
$replay.Replay((Resolve-Path $Binlog).Path)
if (!$script:success -or !$script:buildStart -or !$script:buildFinish -or $script:starts.Count)
{
    throw 'The binlog does not contain a complete successful build'
}
[ordered]@{
    buildSeconds = ($script:buildFinish - $script:buildStart).TotalSeconds
    tasks = $script:tasks.ToArray()
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $OutputFile -Encoding UTF8
