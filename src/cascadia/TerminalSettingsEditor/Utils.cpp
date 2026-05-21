// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "Utils.h"
#include <winrt/Microsoft.Terminal.Settings.Editor.h>

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings
{
    hstring GetSelectedItemTag(const winrt::Windows::Foundation::IInspectable& comboBoxAsInspectable)
    {
        auto comboBox = comboBoxAsInspectable.as<Controls::ComboBox>();
        auto selectedOption = comboBox.SelectedItem().as<Controls::ComboBoxItem>();

        return unbox_value<hstring>(selectedOption.Tag());
    }

    hstring LocalizedNameForEnumName(const std::wstring_view sectionAndEnumType, const std::wstring_view enumValue, const std::wstring_view propertyType)
    {
        // Uppercase the first letter to conform to our current Resource keys
        auto fmtKey = fmt::format(FMT_COMPILE(L"{}{}{}/{}"), sectionAndEnumType, static_cast<wchar_t>(std::towupper(enumValue[0])), enumValue.substr(1), propertyType);
        return GetLibraryResourceString(fmtKey);
    }

    void ExpandAncestorExpanders(const winrt::Windows::UI::Xaml::DependencyObject& controlToFocus)
    {
        winrt::Windows::UI::Xaml::DependencyObject ancestor{ controlToFocus };
        while (ancestor)
        {
            if (const auto& expander{ ancestor.try_as<winrt::Microsoft::UI::Xaml::Controls::Expander>() })
            {
                expander.IsExpanded(true);
            }
            else if (const auto& settingsExpander{ ancestor.try_as<winrt::Microsoft::Terminal::Settings::Editor::SettingsExpander>() })
            {
                settingsExpander.IsExpanded(true);
            }
            ancestor = winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetParent(ancestor);
        }
    }
}
