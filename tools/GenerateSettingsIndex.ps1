<#
Copyright (c) Microsoft Corporation.
Licensed under the MIT license.
.SYNOPSIS
Scans XAML files for local:SettingContainer entries and generates GeneratedSettingsIndex.g.h / .g.cpp.

.PARAMETER SourceDir
Directory to scan recursively for .xaml files.

.PARAMETER OutputDir
Directory to place generated C++ files.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceDir,
    [Parameter(Mandatory=$true)][string]$OutputDir
)

# Prohibited UIDs (exact match, case-insensitive by default)
$ProhibitedUids = @(
    'NewTabMenu_AddRemainingProfiles'
)

# Prohibited XAML files (full paths preferred)
$ProhibitedXamlFiles = @(
    # 'd:\projects\terminal\src\cascadia\TerminalSettingsEditor\SomePage.xaml'
)

if (-not (Test-Path $SourceDir)) { throw "SourceDir not found: $SourceDir" }
if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }

$entries = @()

Get-ChildItem -Path $SourceDir -Recurse -Filter *.xaml | ForEach-Object {
    $file = $_.FullName

    # Skip whole file if prohibited
    if ($ProhibitedXamlFiles -contains $file) { return }

    $text = Get-Content -Raw -LiteralPath $file

    # Extract Page x:Class
    $pageClass = $null
    if ($text -match '<Page\b[^>]*\bx:Class="([^"]+)"') {
        $pageClass = $matches[1]
    } elseif ($text -match '<UserControl\b[^>]*\bx:Class="([^"]+)"') {
        # Needed for Appearances.xaml
        $pageClass = $matches[1]
    } else {
        return
    }

    # Convert XAML namespace dots to C++ scope operators
    $cppPageType = ($pageClass -replace '\.', '::')

    # Find all local:SettingContainer start tags
    $pattern = '<local:SettingContainer\b([^>/]*)(/?>)'
    $matchesAll = [System.Text.RegularExpressions.Regex]::Matches($text, $pattern, 'IgnoreCase')

    foreach ($m in $matchesAll) {
        $attrBlock = $m.Groups[1].Value

        if ($attrBlock -match '\bx:Uid="([^"]+)"') {
            $uid = $matches[1]
        }
        else {
            continue
        }

        if ($attrBlock -match '\bx:Name="([^"]+)"') {
            $name = $matches[1]
        }
        elseif ($attrBlock -match '\bName="([^"]+)"') {
            $name = $matches[1]
        }
        else {
            $name = ""
        }

        # Skip entry if UID prohibited
        if ($ProhibitedUids -contains $uid) { continue }

        $entries += [pscustomobject]@{
            PageClass   = $pageClass
            CppPageType = $cppPageType
            Uid         = $uid
            Name        = $name
            File        = $file
        }
    }
}

# Ensure there aren't any duplicate entries (PageClass, Uid, Name)
$entries = $entries | Sort-Object PageClass, Uid, Name -Unique

$headerPath = Join-Path $OutputDir 'GeneratedSettingsIndex.g.h'
$cppPath    = Join-Path $OutputDir 'GeneratedSettingsIndex.g.cpp'

$header = @"
/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once
#include <winrt/Windows.UI.Xaml.Interop.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct IndexEntry
    {
        std::wstring_view ElementName;
        std::wstring_view ElementUID;
        winrt::Windows::UI::Xaml::Interop::TypeName ParentPage;
    };

    std::span<const IndexEntry> LoadBuildTimeIndex();
}
"@

$entryLines = foreach ($e in $entries) {
    # Ensure we emit valid wide string literals (escape backslashes and quotes)
    $uidEsc = ($e.Uid   -replace '\\','\\\\') -replace '"','\"'
    $nameEsc = ($e.Name -replace '\\','\\\\') -replace '"','\"'
    "            { L`"$nameEsc`", L`"$uidEsc`", winrt::xaml_typename<$($e.CppPageType)>() }"
}

$cpp = @"
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <winrt/Microsoft.Terminal.Settings.Editor.h>
#include "GeneratedSettingsIndex.g.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    namespace
    {
        static const IndexEntry s_entries[] =
        {
$( ($entryLines -join ",`r`n") )
        };
    }

    std::span<const IndexEntry> LoadBuildTimeIndex()
    {
        static auto entries = std::span<const IndexEntry>(s_entries, std::size(s_entries));
        return entries;
    }
}
"@

Set-Content -LiteralPath $headerPath -Value $header -NoNewline
Set-Content -LiteralPath $cppPath -Value $cpp -NoNewline

Write-Host "Generated:"
Write-Host "  $headerPath"
Write-Host "  $cppPath"
Write-Host "Entries: $($entries.Count)"