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
    [Parameter(Mandatory=$false)][string]$SourceDir,
    [Parameter(Mandatory=$false)][string]$OutputDir
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

# Prohibited XAML files
$ProhibitedXamlFiles = @(
    'CommonResources.xaml',
    'KeyChordListener.xaml',
    'NullableColorPicker.xaml',
    'SettingContainerStyle.xaml',
    'AISettings.xaml'
)

if (-not (Test-Path $SourceDir)) { throw "SourceDir not found: $SourceDir" }
if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }

$resourceKeys = ([xml](Get-Content "$($SourceDir)\Resources\en-US\Resources.resw")).root.data.name
$entries = @()

Get-ChildItem -Path $SourceDir -Recurse -Filter *.xaml | ForEach-Object {
    # Skip whole file if prohibited
    $filename = $_.Name
    if ($ProhibitedXamlFiles -contains $filename)
    {
        return
    }

    $text = Get-Content -Raw -LiteralPath $_.FullName

    # Extract Page x:Class
    $pageClass = $null
    if ($text -match '<Page\b[^>]*\bx:Class="([^"]+)"')
    {
        $pageClass = $matches[1]
    }
    elseif ($filename -eq 'Appearances.xaml')
    {
        # Appearances.xaml is a UserControl that is hosted in Profiles_Appearance.xaml
        $pageClass = 'Microsoft::Terminal::Settings::Editor::Profiles_Appearance'
    }
    else
    {
        return
    }

    # Convert XAML namespace dots to C++ scope operators
    $pageClass = ($pageClass -replace '\.', '::')

    # Deduce BreadcrumbSubPage
    # Special cases:
    #  - NewTabMenu: defer to UID, see NavigationParam section below
    #  - Extensions: defer to UID, see NavigationParam section below
    $subPage = 'BreadcrumbSubPage::'
    if ($pageClass -match 'Editor::Profiles_Appearance')
    {
        $subPage += 'Profile_Appearance'
    }
    elseif ($pageClass -match 'Editor::Profiles_Terminal')
    {
        $subPage += 'Profile_Terminal'
    }
    elseif ($pageClass -match 'Editor::Profiles_Advanced')
    {
        $subPage += 'Profile_Advanced'
    }
    elseif ($pageClass -match 'Editor::EditColorScheme')
    {
        $subPage += 'ColorSchemes_Edit'
    }
    else
    {
        $subPage += 'None'
    }

    # Register top-level pages
    if ($filename -eq 'Launch.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_Launch/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_Launch/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"Launch_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'Interaction.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_Interaction/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_Interaction/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"Interaction_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'GlobalAppearance.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_Appearance/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_Appearance/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"GlobalAppearance_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'ColorSchemes.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_ColorSchemes/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_ColorSchemes/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"ColorSchemes_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }

        # Manually register the "add new" button
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"ColorScheme_AddNewButton/Text`""
            DisplayTextLocalized = 'RS_(L"ColorScheme_AddNewButton/Text")'
            HelpTextUid          = 'std::nullopt'
            HelpTextLocalized    = 'std::nullopt'
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"ColorSchemes_Nav`"})"
            SubPage              = 'BreadcrumbSubPage::None'
            ElementName          = 'L"AddNewButton"'
            File                 = $filename
        }

    }
    elseif ($filename -eq 'Rendering.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_Rendering/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_Rendering/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"Rendering_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'Compatibility.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_Compatibility/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_Compatibility/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"Compatibility_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'Actions.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_Actions/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_Actions/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"Actions_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'NewTabMenu.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_NewTabMenu/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_NewTabMenu/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"NewTabMenu_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'Extensions.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_Extensions/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_Extensions/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"Extensions_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'Profiles_Base.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_ProfileDefaults/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_ProfileDefaults/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"GlobalProfile_Nav`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }
    }
    elseif ($filename -eq 'AddProfile.xaml')
    {
        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"Nav_AddNewProfile/Content`""
            DisplayTextLocalized = "RS_(L`"Nav_AddNewProfile/Content`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"AddProfile`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L""'
            File                 = $filename
        }

        $entries += [pscustomobject]@{
            DisplayTextUid       = "L`"AddProfile_AddNewTextBlock/Text`""
            DisplayTextLocalized = "RS_(L`"AddProfile_AddNewTextBlock/Text`")"
            HelpTextUid          = "std::nullopt"
            HelpTextLocalized    = "std::nullopt"
            ParentPage           = $pageClass
            NavigationParam      = "winrt::box_value(hstring{L`"AddProfile`"})"
            SubPage              = "BreadcrumbSubPage::None"
            ElementName          = 'L"AddNewButton"'
            File                 = $filename
        }
    }

    # Find all local:SettingContainer start tags
    $pattern = '<local:SettingContainer\b([^>/]*)(/?>)'
    $matchesAll = [System.Text.RegularExpressions.Regex]::Matches($text, $pattern, 'IgnoreCase')

    foreach ($m in $matchesAll)
    {
        $attrBlock = $m.Groups[1].Value

        # Extract Uid
        if ($attrBlock -match '\bx:Uid="([^"]+)"')
        {
            $uid = $matches[1]
            
            # Skip entry if UID prohibited
            if ($ProhibitedUids -contains $uid)
            {
                continue
            }
        }
        else
        {
            continue
        }

        # Extract Name
        if ($attrBlock -match '\bx:Name="([^"]+)"')
        {
            $name = $matches[1]
        }
        elseif ($attrBlock -match '\bName="([^"]+)"')
        {
            $name = $matches[1]
        }
        else
        {
            $name = ""
        }

        # Profile.Appearance settings need a special prefix for the ElementName.
        # This allows us to bring the element into view at runtime.
        if ($filename -eq 'Appearances.xaml')
        {
            $name = 'App.' + $name
        }

        # Deduce NavigationParam
        # includeInBuildIndex: include the entry in the build-time index (no special param at runtime)
        # includeInPartialIndex: include the entry in the partial index, where the NavigationParam is the view model at runtime (i.e. profile vs profile defaults)
        $includeInBuildIndex = $true
        $includeInPartialIndex = $false
        $navigationParam = 'nullptr'
        if ($pageClass -match 'Editor::Launch')
        {
            $navigationParam = 'Launch_Nav'
        }
        elseif ($pageClass -match 'Editor::Interaction')
        {
            $navigationParam = 'Interaction_Nav'
        }
        elseif ($pageClass -match 'Editor::Rendering')
        {
            $navigationParam = 'Rendering_Nav'
        }
        elseif ($pageClass -match 'Editor::Compatibility')
        {
            $navigationParam = 'Compatibility_Nav'
        }
        elseif ($pageClass -match 'Editor::Actions')
        {
            $navigationParam = 'Actions_Nav'
        }
        elseif ($pageClass -match 'Editor::NewTabMenu')
        {
            if ($uid -match 'NewTabMenu_CurrentFolder')
            {
                $navigationParam = 'nullptr'
                $subPage = 'BreadcrumbSubPage::NewTabMenu_Folder'
            }
            else
            {
                $navigationParam = 'NewTabMenu_Nav'
                $subPage = 'BreadcrumbSubPage::None'
                $includeInPartialIndex = $true
            }
        }
        elseif ($pageClass -match 'Editor::Extensions')
        {
            $navigationParam = 'Extensions_Nav'
            $subPage = 'BreadcrumbSubPage::None'
        }
        elseif ($pageClass -match 'Editor::Profiles_Base' -or
                $pageClass -match 'Editor::Profiles_Appearance' -or
                $pageClass -match 'Editor::Profiles_Terminal' -or
                $pageClass -match 'Editor::Profiles_Advanced')
        {
            $navigationParam = 'GlobalProfile_Nav'
            $includeInBuildIndex = !($name -eq "Name" -or $name -eq "Commandline")
            $includeInPartialIndex = $true
        }
        elseif ($pageClass -match 'Editor::EditColorScheme')
        {
            # populate with color scheme name at runtime
            $navigationParam = 'nullptr'
        }
        elseif ($pageClass -match 'Editor::GlobalAppearance')
        {
            $navigationParam = 'GlobalAppearance_Nav'
        }
        elseif ($pageClass -match 'Editor::AddProfile')
        {
            $navigationParam = 'AddProfile'
        }

        if ($includeInBuildIndex)
        {
            $entries += [pscustomobject]@{
                DisplayTextUid       = "L`"$($uid)/Header`""
                DisplayTextLocalized = "RS_(L`"$($uid)/Header`")"
                HelpTextUid          = $resourceKeys -contains "$($uid).HelpText" ? "std::optional<hstring>{ L`"$($uid)/HelpText`" }" : "std::nullopt"
                HelpTextLocalized    = $resourceKeys -contains "$($uid).HelpText" ? "std::optional<hstring>{ RS_(L`"$($uid)/HelpText`") }" : "std::nullopt"
                ParentPage           = $pageClass
                NavigationParam      = $navigationParam -eq "nullptr" ? $navigationParam : "winrt::box_value(hstring{L`"$($navigationParam)`"})"
                SubPage              = $subPage
                ElementName          = "L`"$($name)`""
                File                 = $filename
            }
        }

        if ($includeInPartialIndex)
        {
            $entries += [pscustomobject]@{
                DisplayTextUid       = "L`"$($uid)/Header`""
                DisplayTextLocalized = "RS_(L`"$($uid)/Header`")"
                HelpTextUid          = $resourceKeys -contains "$($uid).HelpText" ? "std::optional<hstring>{ L`"$($uid)/HelpText`" }" : "std::nullopt"
                HelpTextLocalized    = $resourceKeys -contains "$($uid).HelpText" ? "std::optional<hstring>{ RS_(L`"$($uid)/HelpText`") }" : "std::nullopt"
                ParentPage           = $pageClass
                NavigationParam      = 'nullptr'  # VM param at runtime
                SubPage              = $navigationParam -eq 'NewTabMenu_Nav' ? 'BreadcrumbSubPage::NewTabMenu_Folder' : $subPage
                ElementName          = "L`"$($name)`""
                File                 = $filename
            }
        }
    }
}

# Ensure there aren't any duplicate entries
$entries = $entries | Sort-Object DisplayTextLocalized, HelpTextLocalized, ParentPage, NavigationParam, SubPage, ElementName, File -Unique

$buildTimeEntries = @()
$profileEntries = @()
$schemeEntries = @()
$ntmEntries = @()
foreach ($e in $entries)
{
    $formattedEntry = "            IndexEntry{ $($e.DisplayTextUid), $($e.DisplayTextLocalized), $($e.HelpTextUid), $($e.HelpTextLocalized), $($e.NavigationParam), $($e.SubPage), $($e.ElementName) }, // $($e.File)"
    
    if ($e.NavigationParam -eq 'nullptr' -and
        ($e.ParentPage -match 'Profiles_Base' -or
         $e.ParentPage -match 'Profiles_Appearance' -or
         $e.ParentPage -match 'Profiles_Terminal' -or
         $e.ParentPage -match 'Profiles_Advanced'))
    {
        $profileEntries += $formattedEntry
    }
    elseif ($e.SubPage -eq 'BreadcrumbSubPage::ColorSchemes_Edit')
    {
        $schemeEntries += $formattedEntry
    }
    elseif ($e.SubPage -eq 'BreadcrumbSubPage::NewTabMenu_Folder')
    {
        $ntmEntries += $formattedEntry
    }
    else
    {
        $buildTimeEntries += $formattedEntry
    }
}

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
        // x:Uid of the SettingContainer's Header (i.e. "Globals_DefaultProfile/Header")
        hstring DisplayTextUid;

        // Localized display text shown in the SettingContainer (i.e. RS_(L"Globals_DefaultProfile/Header"))
        hstring DisplayTextLocalized;

        // x:Uid of the SettingContainer's HelpText (i.e. "Globals_DefaultProfile/HelpText")
        // May not exist for all entries
        std::optional<hstring> HelpTextUid;

        // Localized help text shown in the SettingContainer (i.e. RS_(L"Globals_DefaultProfile/HelpText"))
        // May not exist for all entries
        std::optional<hstring> HelpTextLocalized;

        // Navigation argument (i.e. winrt::box_value(hstring) or nullptr)
        // Use nullptr as placeholder for runtime navigation with a view model object
        winrt::Windows::Foundation::IInspectable NavigationArg;

        BreadcrumbSubPage SubPage;
        
        // x:Name of the SettingContainer to navigate to on the page (i.e. "DefaultProfile")
        hstring ElementName;
    };

    const std::array<IndexEntry, $($buildTimeEntries.Count)>& LoadBuildTimeIndex();
    const std::array<IndexEntry, $($profileEntries.Count)>& LoadProfileIndex();
    const std::array<IndexEntry, $($ntmEntries.Count)>& LoadNTMFolderIndex();
    const std::array<IndexEntry, $($schemeEntries.Count)>& LoadColorSchemeIndex();

    const IndexEntry& PartialProfileIndexEntry();
    const IndexEntry& PartialNTMFolderIndexEntry();
    const IndexEntry& PartialColorSchemeIndexEntry();
    const IndexEntry& PartialExtensionIndexEntry();
}
"@

$cpp = @"
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <winrt/Microsoft.Terminal.Settings.Editor.h>
#include "GeneratedSettingsIndex.g.h"
#include <LibraryResources.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    const std::array<IndexEntry, $($buildTimeEntries.Count)>& LoadBuildTimeIndex()
    {
        static std::array<IndexEntry, $($buildTimeEntries.Count)> entries =
        {
$( ($buildTimeEntries -join "`r`n") )
        };
        return entries;
    }

    const std::array<IndexEntry, $($profileEntries.Count)>& LoadProfileIndex()
    {
        static std::array<IndexEntry, $($profileEntries.Count)> entries =
        {
$( ($profileEntries -join "`r`n") )
        };
        return entries;
    }

    const std::array<IndexEntry, $($ntmEntries.Count)>& LoadNTMFolderIndex()
    {
        static std::array<IndexEntry, $($ntmEntries.Count)> entries =
        {
$( ($ntmEntries -join "`r`n") )
        };
        return entries;
    }

    const std::array<IndexEntry, $($schemeEntries.Count)>& LoadColorSchemeIndex()
    {
        static std::array<IndexEntry, $($schemeEntries.Count)> entries =
        {
$( ($schemeEntries -join "`r`n") )
        };
        return entries;
    }

    const IndexEntry& PartialProfileIndexEntry()
    {
        static IndexEntry entry{ L"", L"", std::nullopt, std::nullopt, nullptr, BreadcrumbSubPage::None, L"" };
        return entry;
    }

    const IndexEntry& PartialNTMFolderIndexEntry()
    {
        static IndexEntry entry{ L"", L"", std::nullopt, std::nullopt, nullptr, BreadcrumbSubPage::NewTabMenu_Folder, L"" };
        return entry;
    }

    const IndexEntry& PartialColorSchemeIndexEntry()
    {
        static IndexEntry entry{ L"", L"", std::nullopt, std::nullopt, nullptr, BreadcrumbSubPage::ColorSchemes_Edit, L"" };
        return entry;
    }

    const IndexEntry& PartialExtensionIndexEntry()
    {
        static IndexEntry entry{ L"", L"", std::nullopt, std::nullopt, nullptr, BreadcrumbSubPage::Extensions_Extension, L"" };
        return entry;
    }
}
"@

Set-Content -LiteralPath $headerPath -Value $header -NoNewline
Set-Content -LiteralPath $cppPath -Value $cpp -NoNewline

Write-Host "Generated:"
Write-Host "  $headerPath"
Write-Host "  $cppPath"