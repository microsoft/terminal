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
New-Item -ItemType Directory -Force -Path $output | Out-Null
$rule = [Security.SecurityElement]::Escape("$root\build\rules\Microsoft.Windows.CppWinRT.Additional.targets")
$cases = @(
    @{ name='winrt-default'; enabled=''; winrt='true'; noMidl='true'; header='nul'; second='b.winmd'; expected='true' },
    @{ name='disabled'; enabled='false'; winrt='true'; noMidl='true'; header='nul'; second='b.winmd'; expected='false' },
    @{ name='classic-midl'; enabled=''; winrt='false'; noMidl='false'; header='nul'; second='b.winmd'; expected='false' },
    @{ name='native-stubs'; enabled=''; winrt='true'; noMidl='false'; header='nul'; second='b.winmd'; expected='false' },
    @{ name='shared-header'; enabled=''; winrt='true'; noMidl='true'; header='shared.h'; second='b.winmd'; expected='false' },
    @{ name='duplicate-metadata'; enabled=''; winrt='true'; noMidl='true'; header='nul'; second='a.winmd'; expected='false' },
    @{ name='aliased-metadata'; enabled=''; winrt='true'; noMidl='true'; header='nul'; second='folder\..\a.winmd'; expected='false' },
    @{ name='missing-metadata'; enabled=''; winrt='true'; noMidl='true'; header='nul'; second=''; expected='false' }
)
foreach ($case in $cases)
{
    @"
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup>
    <TerminalParallelWinrtMidl>$($case.enabled)</TerminalParallelWinrtMidl>
    <MultiProcMIDL>false</MultiProcMIDL>
    <MultiProcMaxCount>0</MultiProcMaxCount>
    <CL_MPCount>4</CL_MPCount>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <Midl>
      <EnableWindowsRuntime>$($case.winrt)</EnableWindowsRuntime>
      <NoMidl>$($case.noMidl)</NoMidl>
      <HeaderFileName>$($case.header)</HeaderFileName>
      <DllDataFileName>nul</DllDataFileName>
      <InterfaceIdentifierFileName>nul</InterfaceIdentifierFileName>
      <ProxyFileName>nul</ProxyFileName>
    </Midl>
  </ItemDefinitionGroup>
  <ItemGroup>
    <Midl Include="a.idl"><MetadataFileName>a.winmd</MetadataFileName></Midl>
    <Midl Include="b.idl"><MetadataFileName>$($case.second)</MetadataFileName></Midl>
  </ItemGroup>
  <Target Name="Midl" />
  <Import Project="$rule" />
</Project>
"@ | Set-Content "$output\$($case.name).proj"
    $json = & $MsBuild "$output\$($case.name).proj" /t:Midl `
        /getProperty:MultiProcMIDL,MultiProcMaxCount,EnforceProcessCountAcrossBuilds /nologo /v:quiet /nr:false
    if ($LASTEXITCODE) { throw "MSBuild failed: $($case.name)" }
    $properties = ($json | ConvertFrom-Json).Properties
    if ($properties.MultiProcMIDL -ne $case.expected) { throw "Unexpected scheduler for $($case.name)" }
    if ($case.expected -eq 'true' -and
        ($properties.MultiProcMaxCount -ne '4' -or $properties.EnforceProcessCountAcrossBuilds -ne 'true'))
    {
        throw "Parallel scheduling was not bounded for $($case.name)"
    }
    Write-Host "$($case.name): passed"
}

foreach ($limit in @('2', '8'))
{
    $json = & $MsBuild "$output\winrt-default.proj" /t:Midl "/p:MultiProcMaxCount=$limit" `
        /p:EnforceProcessCountAcrossBuilds=false `
        /getProperty:MultiProcMIDL,MultiProcMaxCount,EnforceProcessCountAcrossBuilds /nologo /v:quiet /nr:false
    if ($LASTEXITCODE) { throw "MSBuild failed for explicit limit $limit" }
    $properties = ($json | ConvertFrom-Json).Properties
    if ($properties.MultiProcMIDL -ne 'true' -or $properties.MultiProcMaxCount -ne $limit -or
        $properties.EnforceProcessCountAcrossBuilds -ne 'false')
    {
        throw "Explicit scheduler settings were not preserved: $limit"
    }
    Write-Host "explicit limit $limit`: passed"
}

$json = & $MsBuild "$output\winrt-default.proj" /t:Midl /p:CL_MPCount= `
    /getProperty:MultiProcMIDL,MultiProcMaxCount,NUMBER_OF_PROCESSORS /nologo /v:quiet /nr:false
if ($LASTEXITCODE) { throw 'MSBuild failed for automatic processor limit' }
$properties = ($json | ConvertFrom-Json).Properties
if ($properties.MultiProcMIDL -ne 'true' -or $properties.MultiProcMaxCount -ne $properties.NUMBER_OF_PROCESSORS)
{
    throw 'Automatic processor limit was not selected'
}
Write-Host 'automatic processor limit: passed'

$emptyProject = @"
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup><MultiProcMIDL>false</MultiProcMIDL></PropertyGroup>
  <Target Name="Midl" />
  <Import Project="$rule" />
</Project>
"@
$emptyProject | Set-Content "$output\no-inputs.proj"
$result = & $MsBuild "$output\no-inputs.proj" /t:Midl /getProperty:MultiProcMIDL /nologo /v:quiet /nr:false
if ($LASTEXITCODE -or $result.Trim() -ne 'false') { throw 'Empty MIDL inputs changed the scheduler' }
Write-Host 'no inputs: passed'

foreach ($setting in @('UseMultiToolTask=true', 'MultiProcCL=true'))
{
    $json = & $MsBuild "$output\winrt-default.proj" /t:Midl "/p:$setting" `
        /getProperty:MultiProcMIDL,MultiProcMaxCount,EnforceProcessCountAcrossBuilds /nologo /v:quiet /nr:false
    if ($LASTEXITCODE) { throw "MSBuild failed for existing scheduler: $setting" }
    $properties = ($json | ConvertFrom-Json).Properties
    if ($properties.MultiProcMIDL -ne 'false' -or $properties.MultiProcMaxCount -ne '0' -or
        $properties.EnforceProcessCountAcrossBuilds -ne '')
    {
        throw "Existing scheduler settings were changed: $setting"
    }
    Write-Host "existing scheduler $setting`: passed"
}
