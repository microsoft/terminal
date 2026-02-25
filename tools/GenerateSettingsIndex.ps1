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
    [Parameter(Mandatory=$false)][string]$SourceDir = "$PSScriptRoot\..\src\cascadia\TerminalSettingsEditor\",
    [Parameter(Mandatory=$false)][string]$OutputDir = "$PSScriptRoot\..\src\cascadia\TerminalSettingsEditor\Generated Files\"
)

# Prohibited UIDs (exact match, case-insensitive by default)
$ProhibitedUids = @(
    'Extensions_Scope',
    'Profile_MissingFontFaces',
    'Profile_ProportionalFontFaces',
    'ColorScheme_InboxSchemeDuplicate',
    'ColorScheme_ColorsHeader',
    'ColorScheme_Rename'
)

# Prohibited XAML files (already limited to Page root elements)
$ProhibitedXamlFiles = @(
    'AISettings.xaml',
    'Profiles_Base_Orphaned.xaml',
    "EditAction.xaml",
    "MainPage.xaml"
)

# Grouped metadata for each page class
$ClassMap = @{
    "Microsoft::Terminal::Settings::Editor::Launch" = @{
        ResourceName    = "Nav_Launch/Content"
        NavigationParam = "Launch_Nav"
        SubPage         = "BreadcrumbSubPage::None"
    }
    "Microsoft::Terminal::Settings::Editor::Interaction" = @{
        ResourceName    = "Nav_Interaction/Content"
        NavigationParam = "Interaction_Nav"
        SubPage         = "BreadcrumbSubPage::None"
    }
    "Microsoft::Terminal::Settings::Editor::GlobalAppearance" = @{
        ResourceName    = "Nav_Appearance/Content"
        NavigationParam = "GlobalAppearance_Nav"
        SubPage         = "BreadcrumbSubPage::None"
    }
    "Microsoft::Terminal::Settings::Editor::ColorSchemes" = @{
        ResourceName    = "Nav_ColorSchemes/Content"
        NavigationParam = "ColorSchemes_Nav"
        SubPage         = "BreadcrumbSubPage::None"
    }
    "Microsoft::Terminal::Settings::Editor::Rendering" = @{
        ResourceName    = "Nav_Rendering/Content"
        NavigationParam = "Rendering_Nav"
        SubPage         = "BreadcrumbSubPage::None"
    }
    "Microsoft::Terminal::Settings::Editor::Compatibility" = @{
        ResourceName    = "Nav_Compatibility/Content"
        NavigationParam = "Compatibility_Nav"
        SubPage         = "BreadcrumbSubPage::None"
    }
    "Microsoft::Terminal::Settings::Editor::Actions" = @{
        ResourceName    = "Nav_Actions/Content"
        NavigationParam = "Actions_Nav"
        SubPage         = "BreadcrumbSubPage::None"
    }
    "Microsoft::Terminal::Settings::Editor::NewTabMenu" = @{
        ResourceName    = "Nav_NewTabMenu/Content" # [Folders] Replaced with folder name
        NavigationParam = "NewTabMenu_Nav" # [Folders] Replaced with folder VM
        SubPage         = "BreadcrumbSubPage::None" # [Folders] Replaced with BreadcrumbSubPage::NewTabMenu_Folder
    }
    "Microsoft::Terminal::Settings::Editor::Extensions" = @{
        ResourceName    = "Nav_Extensions/Content" # [Extension] Replaced with extension name
        NavigationParam = "Extensions_Nav" # [Extension] Replaced with extension VM
        SubPage         = "BreadcrumbSubPage::None" # [Extension] Replaced with BreadcrumbSubPage::Extensions_Extension
    }
    "Microsoft::Terminal::Settings::Editor::Profiles_Base" = @{
        ResourceName    = "Nav_ProfileDefaults/Content"
        NavigationParam = "GlobalProfile_Nav"
        SubPage         = "BreadcrumbSubPage::None"
    }
    "Microsoft::Terminal::Settings::Editor::Profiles_Appearance" = @{
        ResourceName    = "Nav_ProfileDefaults/Content"
        NavigationParam = "GlobalProfile_Nav"
        SubPage         = "BreadcrumbSubPage::Profile_Appearance"
    }
    "Microsoft::Terminal::Settings::Editor::Profiles_Terminal" = @{
        ResourceName    = "Nav_ProfileDefaults/Content"
        NavigationParam = "GlobalProfile_Nav"
        SubPage         = "BreadcrumbSubPage::Profile_Terminal"
    }
    "Microsoft::Terminal::Settings::Editor::Profiles_Advanced" = @{
        ResourceName    = "Nav_ProfileDefaults/Content"
        NavigationParam = "GlobalProfile_Nav"
        SubPage         = "BreadcrumbSubPage::Profile_Advanced"
    }
    "Microsoft::Terminal::Settings::Editor::AddProfile" = @{
        ResourceName    = "Nav_AddNewProfile/Content"
        NavigationParam = "AddProfile"
        SubPage         = "BreadcrumbSubPage::None"
    }
}

function IsProfileSubPage($pageClass)
{
    return $pageClass -match "Editor::Profiles_Appearance" -or
           $pageClass -match "Editor::Profiles_Terminal" -or
           $pageClass -match "Editor::Profiles_Advanced"
}

if (-not (Test-Path $SourceDir)) { throw "SourceDir not found: $SourceDir" }
if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }

$entries = @()
foreach ($xamlFile in Get-ChildItem -Path $SourceDir -Filter *.xaml)
{
    # Skip whole file if prohibited
    $filename = $xamlFile.Name
    if ($ProhibitedXamlFiles -contains $filename)
    {
        continue
    }

    # Load XAML and namespace manager
    [xml]$xml = Get-Content -LiteralPath $xamlFile.FullName
    $xm = New-Object System.Xml.XmlNamespaceManager($xml.NameTable)
    $xm.AddNamespace('local', 'using:Microsoft.Terminal.Settings.Editor')
    $xm.AddNamespace('x', 'http://schemas.microsoft.com/winfx/2006/xaml')

    if ($xml.DocumentElement.LocalName -ne 'Page' -and $filename -ne 'Appearances.xaml')
    {
        # Only allow xaml files for Page elements (or Appearances.xaml)
        continue
    }

    # Extract Page x:Class
    # Appearances.xaml: UserControl hosted in Profiles_Appearance.xaml
    $pageClass = $filename -eq 'Appearances.xaml' ?
                   'Microsoft::Terminal::Settings::Editor::Profiles_Appearance' :
                   $xml.DocumentElement.SelectSingleNode('@x:Class', $xm).Value

    # Convert XAML namespace dots to C++ scope operators
    $pageClass = ($pageClass -replace '\.', '::')
    if ($ClassMap.ContainsKey(($pageClass)) -and -not (IsProfileSubPage $pageClass))
    {
        $entries += [pscustomobject]@{
            ResourceName    = $ClassMap[$pageClass].ResourceName
            ParentPage      = $pageClass
            NavigationParam = $ClassMap[$pageClass].NavigationParam
            SubPage         = $ClassMap[$pageClass].SubPage
            ElementName     = $null # No specific element to navigate to for the page itself
            File            = $filename
        }
    }
    elseif ($pageClass -notmatch "Editor::EditColorScheme" -and -not (IsProfileSubPage $pageClass))
    {
        # Special case: EditColorScheme is only valid if a color scheme is associated,
        #                 so don't register it in ClassMap and don't skip over it here.
        Write-Warning "No class map entry for page class $pageClass (file: $filename). Skipping automatic index generation for this page."
        continue
    }

    # Manually register special entries
    if ($filename -eq 'ColorSchemes.xaml')
    {
        # "add new" button
        $entries += [pscustomobject]@{
            ResourceName    = "ColorScheme_AddNewButton/Text"
            ParentPage      = $pageClass
            NavigationParam = $ClassMap[$pageClass].NavigationParam
            SubPage         = $ClassMap[$pageClass].SubPage
            ElementName     = "AddNewButton"
            File            = $filename
        }
    }
    elseif ($filename -eq 'AddProfile.xaml')
    {
        # "add new" button
        $entries += [pscustomobject]@{
            ResourceName    = "AddProfile_AddNewTextBlock/Text"
            ParentPage      = $pageClass
            NavigationParam = $ClassMap[$pageClass].NavigationParam
            SubPage         = $ClassMap[$pageClass].SubPage
            ElementName     = "AddNewButton"
            File            = $filename
        }
    }

    # Iterate over all local:SettingContainer nodes
    foreach ($settingContainer in $xml.SelectNodes("//local:SettingContainer", $xm))
    {
        # Extract Uid
        if ($null -eq $settingContainer.Uid)
        {
            Write-Warning "No x:Uid found for a SettingContainer in file $filename. Skipping entry."
            continue
        }
        elseif ($ProhibitedUids -contains $settingContainer.Uid)
        {
            continue
        }

        # Extract Name (prefer x:Name over Name)
        $name = $null -ne $settingContainer.Name ? $settingContainer.Name : ""
        if ($filename -eq 'Appearances.xaml')
        {
            # Profile.Appearance settings need a special prefix for the ElementName.
            # This allows us to bring the element into view at runtime.
            $name = 'App.' + $name
        }

        # Deduce NavigationParam and SubPage
        # includeInBuildIndex: include the entry in the build-time index (no special param at runtime)
        # includeInPartialIndex: include the entry in the partial index, where the NavigationParam is the view model at runtime (i.e. profile vs profile defaults)
        $includeInBuildIndex = $true
        $includeInPartialIndex = $false
        $navigationParam = $ClassMap[$pageClass].NavigationParam
        $subPage = $ClassMap[$pageClass].SubPage ?? "BreadcrumbSubPage::None"
        if ($pageClass -match 'Editor::NewTabMenu')
        {
            if ($settingContainer.Uid -match 'NewTabMenu_CurrentFolder')
            {
                $navigationParam = $null # VM param at runtime
                $subPage = "BreadcrumbSubPage::NewTabMenu_Folder"
                $includeInBuildIndex = $false
                $includeInPartialIndex = $true
            }
            else
            {
                $includeInPartialIndex = $true
            }
        }
        elseif ($pageClass -match 'Editor::Profiles_Base' -or
                $pageClass -match 'Editor::Profiles_Appearance' -or
                $pageClass -match 'Editor::Profiles_Terminal' -or
                $pageClass -match 'Editor::Profiles_Advanced')
        {
            $navigationParam = "GlobalProfile_Nav"
            $includeInBuildIndex = !($name -eq "Name" -or $name -eq "Commandline")
            $includeInPartialIndex = $true
        }
        elseif ($pageClass -match 'Editor::EditColorScheme')
        {
            $subPage = "BreadcrumbSubPage::ColorSchemes_Edit"
            $includeInBuildIndex = $false
            $includeInPartialIndex = $true
        }

        if ($includeInBuildIndex)
        {
            $entries += [pscustomobject]@{
                ResourceName      = "$($settingContainer.Uid)/Header"
                ParentPage        = $pageClass
                NavigationParam   = $navigationParam
                SubPage           = $subPage
                ElementName       = $name
                File              = $filename
            }
        }

        if ($includeInPartialIndex)
        {
            $entries += [pscustomobject]@{
                ResourceName      = "$($settingContainer.Uid)/Header"
                ParentPage        = $pageClass
                NavigationParam   = $null # VM param at runtime
                SubPage           = $pageClass -match "Editor::NewTabMenu" ? "BreadcrumbSubPage::NewTabMenu_Folder" : $subPage
                ElementName       = $name
                File              = $filename
            }
        }
    }
}

function FormatEntry($e)
{
    $formattedResourceName = "USES_RESOURCE(L`"$($e.ResourceName)`")"
    $formattedNavigationParam = "L`"$($e.NavigationParam)`"" # null Navigation param resolves to empty string
    $formattedElementName = "L`"$($e.ElementName)`""

    return "            IndexEntry{{ {0}, {1}, {2}, {3} }}, // {4}" -f ($formattedResourceName, $formattedNavigationParam, $e.SubPage, $formattedElementName, $e.File)
}

function FormatEntries($es) {
    return ($es | ForEach-Object { FormatEntry $_ }) -join "`r`n"
}

# Sort and remove duplicates
$entries = $entries | Sort-Object ResourceName, ParentPage, NavigationParam, SubPage, ElementName, File -Unique

$buildTimeEntries = @()
$profileEntries = @()
$schemeEntries = @()
$ntmEntries = @()
foreach ($e in $entries)
{    
    if ($null -eq $e.NavigationParam -and
        ($e.ParentPage -match 'Profiles_Base' -or
         $e.ParentPage -match 'Profiles_Appearance' -or
         $e.ParentPage -match 'Profiles_Terminal' -or
         $e.ParentPage -match 'Profiles_Advanced'))
    {
        $profileEntries += $e
    }
    elseif ($e.SubPage -eq 'BreadcrumbSubPage::ColorSchemes_Edit')
    {
        $schemeEntries += $e
    }
    elseif ($e.SubPage -eq 'BreadcrumbSubPage::NewTabMenu_Folder')
    {
        $ntmEntries += $e
    }
    else
    {
        $buildTimeEntries += $e
    }
}

$headerPath = Join-Path $OutputDir 'GeneratedSettingsIndex.g.h'
$cppPath    = Join-Path $OutputDir 'GeneratedSettingsIndex.g.cpp'

$header = @"
/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
// This file is automatically generated by tools\GenerateSettingsIndex.ps1. Changes to this file may be overwritten.
#pragma once
#include <winrt/Windows.UI.Xaml.Interop.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct IndexEntry
    {
        // Resource name of the SettingContainer's Header (i.e. "Globals_DefaultProfile/Header")
        // NOTE: wrapped in USES_RESOURCE() to take advantage of compile-time resource name validation in ResourceLoader
        wil::zwstring_view ResourceName;
        
        // Navigation argument
        // - the tag used to identify the page to navigate to (i.e. "Launch_Nav")
        // - empty if the NavigationArg is meant to be a view model object at runtime (i.e. profile, ntm folder, etc.)
        std::wstring_view NavigationArgTag;

        // SubPage to navigate to for pages with multiple subpages (i.e. Profiles, New Tab Menu)
        BreadcrumbSubPage SubPage;
        
        // x:Name of the SettingContainer to navigate to on the page (i.e. "DefaultProfile")
        std::wstring_view ElementName;
    };

    const std::array<IndexEntry, $($buildTimeEntries.Count)>& LoadBuildTimeIndex();
    const std::array<IndexEntry, $($profileEntries.Count)>& LoadProfileIndex();
    const std::array<IndexEntry, $($ntmEntries.Count)>& LoadNTMFolderIndex();
    const std::array<IndexEntry, $($schemeEntries.Count)>& LoadColorSchemeIndex();

    const IndexEntry& PartialProfileIndexEntry();
    const IndexEntry& PartialNTMFolderIndexEntry();
    const IndexEntry& PartialColorSchemeIndexEntry();
    const IndexEntry& PartialExtensionIndexEntry();
    const IndexEntry& PartialActionIndexEntry();
}
"@

$cpp = @"
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
// This file is automatically generated by tools\GenerateSettingsIndex.ps1. Changes to this file may be overwritten.

#include "pch.h"
#include <winrt/Microsoft.Terminal.Settings.Editor.h>
#include "GeneratedSettingsIndex.g.h"
#include <LibraryResources.h>

// In Debug builds, USES_RESOURCE() expands to a lambda (for resource validation),
// which prevents constexpr evaluation. In Release it's a no-op identity macro.
#ifdef _DEBUG
#define STATIC_INDEX_QUALIFIER static const
#else
#define STATIC_INDEX_QUALIFIER static constexpr
#endif

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    const std::array<IndexEntry, $($buildTimeEntries.Count)>& LoadBuildTimeIndex()
    {
        STATIC_INDEX_QUALIFIER std::array entries =
        {
$(FormatEntries $buildTimeEntries)
        };
        return entries;
    }

    const std::array<IndexEntry, $($profileEntries.Count)>& LoadProfileIndex()
    {
        STATIC_INDEX_QUALIFIER std::array entries =
        {
$(FormatEntries $profileEntries)
        };
        return entries;
    }

    const std::array<IndexEntry, $($ntmEntries.Count)>& LoadNTMFolderIndex()
    {
        STATIC_INDEX_QUALIFIER std::array entries =
        {
$(FormatEntries $ntmEntries)
        };
        return entries;
    }

    const std::array<IndexEntry, $($schemeEntries.Count)>& LoadColorSchemeIndex()
    {
        STATIC_INDEX_QUALIFIER std::array entries =
        {
$(FormatEntries $schemeEntries)
        };
        return entries;
    }

    const IndexEntry& PartialProfileIndexEntry()
    {
        static constexpr IndexEntry entry{ L"", L"", BreadcrumbSubPage::None, L"" };
        return entry;
    }

    const IndexEntry& PartialNTMFolderIndexEntry()
    {
        static constexpr IndexEntry entry{ L"", L"", BreadcrumbSubPage::NewTabMenu_Folder, L"" };
        return entry;
    }

    const IndexEntry& PartialColorSchemeIndexEntry()
    {
        static constexpr IndexEntry entry{ L"", L"", BreadcrumbSubPage::ColorSchemes_Edit, L"" };
        return entry;
    }

    const IndexEntry& PartialExtensionIndexEntry()
    {
        static constexpr IndexEntry entry{ L"", L"", BreadcrumbSubPage::Extensions_Extension, L"" };
        return entry;
    }

    const IndexEntry& PartialActionIndexEntry()
    {
        static constexpr IndexEntry entry{ L"", L"", BreadcrumbSubPage::Actions_Edit, L"" };
        return entry;
    }
}
"@

Set-Content -LiteralPath $headerPath -Value $header -NoNewline
Set-Content -LiteralPath $cppPath -Value $cpp -NoNewline

Write-Host "Generated:"
Write-Host "  $headerPath"
Write-Host "  $cppPath"