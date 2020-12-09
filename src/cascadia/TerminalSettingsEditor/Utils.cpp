// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "Utils.h"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Settings.Editor/Resources");

namespace winrt::Microsoft::Terminal::Settings
{
    hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable)
    {
        Controls::ComboBox comboBox = comboBoxAsInspectable.as<Controls::ComboBox>();
        Controls::ComboBoxItem selectedOption = comboBox.SelectedItem().as<Controls::ComboBoxItem>();

        return unbox_value<hstring>(selectedOption.Tag());
    }

    hstring LocalizedNameForEnumName(const std::wstring_view sectionAndEnumType, const std::wstring_view enumValue, const std::wstring_view propertyType)
    {
        // Uppercase the first letter to conform to our current Resource keys
        auto fmtKey = fmt::format(L"{}{}{}/{}", sectionAndEnumType, char(std::towupper(enumValue[0])), enumValue.substr(1), propertyType);
        return GetLibraryResourceString(fmtKey);
    }
}
