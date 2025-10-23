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
    # TODO CARLOS: set mandatory to true and remove default params
    [Parameter(Mandatory=$false)][string]$SourceDir='D:\projects\terminal\src\cascadia\TerminalSettingsEditor\',
    [Parameter(Mandatory=$false)][string]$OutputDir='D:\projects\terminal\src\cascadia\TerminalSettingsEditor\Generated Files\'
)

# Prohibited UIDs (exact match, case-insensitive by default)
$ProhibitedUids = @(
    # TODO CARLOS: AddRemainingProfiles should probably be allowed
    'NewTabMenu_AddRemainingProfiles',
    'Extensions_Scope'
)

# Prohibited XAML files
$ProhibitedXamlFiles = @(
    'CommonResources.xaml',
    'KeyChordListener.xaml',
    'NullableColorPicker.xaml',
    'SettingContainerStyle.xaml'
)

if (-not (Test-Path $SourceDir)) { throw "SourceDir not found: $SourceDir" }
if (-not (Test-Path $OutputDir)) { New-Item -ItemType Directory -Path $OutputDir | Out-Null }

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
        # Apperances.xaml is a UserControl that is hosted in Profiles_Appearance.xaml
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

    if ($filename -eq 'ColorSchemes.xaml')
    {
        # ColorSchemes.xaml doesn't have any SettingContainers!
        
        # Register the page itself
        $entries += [pscustomobject]@{
            DisplayText     = 'vm'
            ParentPage      = "winrt::xaml_typename<Microsoft::Terminal::Settings::Editor::ColorSchemes>()"
            NavigationParam = "winrt::box_value(hstring{L`"ColorSchemes_Nav`"})"
            SubPage         = 'BreadcrumbSubPage::ColorSchemes_Edit'
            ElementName    = 'L""'
            File            = $filename
        }

        # Manually register the "add new" button
        $entries += [pscustomobject]@{
            DisplayText     = 'RS_(L"ColorScheme_AddNewButton/Text")'
            ParentPage      = "winrt::xaml_typename<Microsoft::Terminal::Settings::Editor::ColorSchemes>()"
            NavigationParam = "winrt::box_value(hstring{L`"ColorSchemes_Nav`"})"
            SubPage         = 'BreadcrumbSubPage::None'
            ElementName    = 'L"AddNewButton"'
            File            = $filename
        }
        return
    }
    elseif ($filename -eq 'Extensions.xaml')
    {
        # Register the main extension page
        $entries += [pscustomobject]@{
            DisplayText     = 'RS_(L"Nav_Extensions/Content")'
            ParentPage      = "winrt::xaml_typename<Microsoft::Terminal::Settings::Editor::Extensions>()"
            NavigationParam = "nullptr"
            SubPage         = 'BreadcrumbSubPage::None'
            ElementName    = 'L""'
            File            = $filename
        }

        # Register the extension view
        $entries += [pscustomobject]@{
            DisplayText     = 'vm.Package().DisplayName()'
            ParentPage      = "winrt::xaml_typename<Microsoft::Terminal::Settings::Editor::Extensions>()"
            NavigationParam = "vm"
            SubPage         = 'BreadcrumbSubPage::Extensions_Extension'
            ElementName    = 'L""'
            File            = $filename
        }
        return
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

        # Deduce NavigationParam
        # duplicateForVM is used to duplicate the entry if we need a VM param at runtime (i.e. profile vs global profile)
        $duplicateForVM = $false
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
                $navigationParam = 'vm'
                $subPage = 'BreadcrumbSubPage::NewTabMenu_Folder'
            }
            else
            {
                $navigationParam = 'NewTabMenu_Nav'
                $subPage = 'BreadcrumbSubPage::None'
                $duplicateForVM = $true
            }
        }
        elseif ($pageClass -match 'Editor::Extensions')
        {
            # TODO CARLOS: There's actually no UIDs for extension view! But I want the page to still exist in the index at runtime for each extension.
            #if ($uid -match 'NewTabMenu_CurrentFolder')
            #{
            #    $navigationParam = 'vm'
            #    $subPage = 'BreadcrumbSubPage::Extensions_Extension'
            #}
            #else
            #{
            $navigationParam = 'Extensions_Nav'
            $subPage = 'BreadcrumbSubPage::None'
            #}
        }
        elseif ($pageClass -match 'Editor::Profiles_Base' -or
                $pageClass -match 'Editor::Profiles_Appearance' -or
                $pageClass -match 'Editor::Profiles_Terminal' -or
                $pageClass -match 'Editor::Profiles_Advanced')
        {
            $navigationParam = 'GlobalProfile_Nav'
            $duplicateForVM = $true
        }
        elseif ($pageClass -match 'Editor::EditColorScheme')
        {
            # EditColorScheme.xaml always uses a boxed vm param
            $navigationParam = 'winrt::box_value(vm)'
        }
        elseif ($pageClass -match 'Editor::GlobalAppearance')
        {
            $navigationParam = 'GlobalAppearance_Nav'
        }
        elseif ($pageClass -match 'Editor::AddProfile')
        {
            $navigationParam = 'AddProfile_Nav'
        }

        $entries += [pscustomobject]@{
            DisplayText     = "RS_(L`"$($uid)/Header`")"
            ParentPage      = "winrt::xaml_typename<$($pageClass)>()"
            NavigationParam = $navigationParam -eq "vm" -or $navigationParam -eq "winrt::box_value(vm)" -or $navigationParam -eq "nullptr" ? $navigationParam : "winrt::box_value(hstring{L`"$($navigationParam)`"})"
            SubPage         = $subPage
            ElementName    = "L`"$($name)`""
            File            = $filename
        }

        # Duplicate entry for VM param if needed
        if ($duplicateForVM)
        {
            $entries += [pscustomobject]@{
                DisplayText     = "RS_(L`"$($uid)/Header`")"
                ParentPage      = "winrt::xaml_typename<$($pageClass)>()"
                NavigationParam = 'vm'
                SubPage         = $navigationParam -eq 'NewTabMenu_Nav' ? 'BreadcrumbSubPage::NewTabMenu_Folder' : $subPage
                ElementName    = "L`"$($name)`""
                File            = $filename
            }
        }
    }
}

# Ensure there aren't any duplicate entries
$entries = $entries | Sort-Object DisplayText, ParentPage, NavigationParam, SubPage, ElementName, File -Unique

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
        hstring DisplayText; // RS_(L"<x:uid>/Header")
        winrt::Windows::UI::Xaml::Interop::TypeName ParentPage;
        winrt::Windows::Foundation::IInspectable NavigationParam; // VM or hstring tag
        BreadcrumbSubPage SubPage;
        hstring ElementName;
    };

    std::vector<IndexEntry> LoadBuildTimeIndex();
    std::vector<IndexEntry> LoadProfileIndex(const Editor::ProfileViewModel& vm);
    std::vector<IndexEntry> LoadNTMFolderIndex(const Editor::FolderEntryViewModel& vm);
    std::vector<IndexEntry> LoadExtensionIndex(const Editor::ExtensionPackageViewModel& vm);
    std::vector<IndexEntry> LoadColorSchemeIndex(const hstring colorSchemeName);
}
"@


$buildTimeEntries = @()
$profileEntries = @()
$schemeEntries = @()
$ntmEntries = @()
$extensionEntries = @()
foreach ($e in $entries)
{
    $formattedEntry = "            { $($e.DisplayText), $($e.ParentPage), $($e.NavigationParam), $($e.SubPage), $($e.ElementName) }, // $($e.File)"
    
    if ($e.NavigationParam -eq 'vm' -and
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
    elseif ($e.SubPage -eq 'BreadcrumbSubPage::Extensions_Extension')
    {
        $extensionEntries += $formattedEntry
    }
    else
    {
        $buildTimeEntries += $formattedEntry
    }
}

$cpp = @"
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include <winrt/Microsoft.Terminal.Settings.Editor.h>
#include "GeneratedSettingsIndex.g.h"
#include <LibraryResources.h>

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    std::vector<IndexEntry> LoadBuildTimeIndex()
    {
        static const IndexEntry entries[] =
        {
$( ($buildTimeEntries -join "`r`n") )
        };
        std::vector<IndexEntry> index;
        index.assign(std::begin(entries), std::end(entries));
        return index;
    }

    std::vector<IndexEntry> LoadProfileIndex(const Editor::ProfileViewModel& vm)
    {
        const IndexEntry entries[] =
        {
$( ($profileEntries -join "`r`n") )
        };
        std::vector<IndexEntry> index;
        index.assign(std::begin(entries), std::end(entries));
        return index;
    }

    std::vector<IndexEntry> LoadNTMFolderIndex(const Editor::FolderEntryViewModel& vm)
    {
        const IndexEntry entries[] =
        {
$( ($ntmEntries -join "`r`n") )
        };
        std::vector<IndexEntry> index;
        index.assign(std::begin(entries), std::end(entries));
        return index;
    }

    std::vector<IndexEntry> LoadExtensionIndex(const Editor::ExtensionPackageViewModel& vm)
    {
        const IndexEntry entries[] =
        {
$( ($extensionEntries -join "`r`n") )
        };
        std::vector<IndexEntry> index;
        index.assign(std::begin(entries), std::end(entries));
        return index;
    }
    
    std::vector<IndexEntry> LoadColorSchemeIndex(const hstring vm)
    {
        const IndexEntry entries[] =
        {
$( ($schemeEntries -join "`r`n") )
        };
        std::vector<IndexEntry> index;
        index.assign(std::begin(entries), std::end(entries));
        return index;
    }
}
"@

Set-Content -LiteralPath $headerPath -Value $header -NoNewline
Set-Content -LiteralPath $cppPath -Value $cpp -NoNewline

Write-Host "Generated:"
Write-Host "  $headerPath"
Write-Host "  $cppPath"