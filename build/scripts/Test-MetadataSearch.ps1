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
    @{
        name = 'no-metadata'
        files = @()
        directories = @()
        expected = @()
        enabled = ''
    },
    @{
        name = 'common-first'
        files = @('sdk\Other\1\Other.winmd', 'sdk\Foundation\4\Windows.Foundation.FoundationContract.winmd',
            'sdk\Universal\19\Windows.Foundation.UniversalApiContract.winmd')
        directories = @('vendor\', 'sdk\Other\1\', 'sdk\Foundation\4\', 'sdk\Universal\19\')
        expected = @('sdk\Foundation\4\', 'sdk\Universal\19\', 'vendor\', 'sdk\Other\1\')
        enabled = 'true'
    },
    @{
        name = 'disabled'
        files = @('sdk\Other\1\Other.winmd', 'sdk\Foundation\4\Windows.Foundation.FoundationContract.winmd')
        directories = @('vendor\', 'sdk\Other\1\', 'sdk\Foundation\4\')
        expected = @('vendor\', 'sdk\Other\1\', 'sdk\Foundation\4\')
        enabled = 'false'
    },
    @{
        name = 'no-common-contracts'
        files = @('sdk\Other\1\Other.winmd')
        directories = @('vendor\', 'sdk\Other\1\')
        expected = @('vendor\', 'sdk\Other\1\')
        enabled = ''
    },
    @{
        name = 'shared-directory'
        files = @('sdk\Windows.Foundation.FoundationContract.winmd', 'sdk\Windows.Foundation.UniversalApiContract.winmd')
        directories = @('vendor\', 'sdk\')
        expected = @('sdk\', 'vendor\')
        enabled = ''
    },
    @{
        name = 'already-ordered'
        files = @('sdk\Windows.Foundation.FoundationContract.winmd')
        directories = @('sdk\', 'vendor\')
        expected = @('sdk\', 'vendor\')
        enabled = ''
    }
)
foreach ($case in $cases)
{
    $references = if ($case.files.Count) { '<CppWinRTPlatformWinMDReferences Include="' + ($case.files -join ';') + '" />' }
    $directories = if ($case.directories.Count) { '<CppWinRTMdMergeMetadataDirectories Include="' + ($case.directories -join ';') + '" />' }
    $project = @"
<Project xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup><TerminalOptimizeMetadataSearch>$($case.enabled)</TerminalOptimizeMetadataSearch></PropertyGroup>
  <Target Name="GetCppWinRTMdMergeInputs">
    <ItemGroup>
      $references
      $directories
    </ItemGroup>
  </Target>
  <Import Project="$rule" />
</Project>
"@
    $path = "$output\$($case.name).proj"
    $project | Set-Content -LiteralPath $path
    $json = & $MsBuild $path /t:GetCppWinRTMdMergeInputs /getItem:CppWinRTMdMergeMetadataDirectories /nologo /nr:false
    if ($LASTEXITCODE) { throw "MSBuild failed: $($case.name)" }
    $items = ($json | ConvertFrom-Json).Items.CppWinRTMdMergeMetadataDirectories
    $actual = @($items | ForEach-Object Identity)
    if (($actual -join ';') -ne ($case.expected -join ';'))
    {
        throw "$($case.name): expected $($case.expected -join ';'), got $($actual -join ';')"
    }
    Write-Host "$($case.name): passed"
}
