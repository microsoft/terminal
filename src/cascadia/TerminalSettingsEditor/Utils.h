// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Settings
{
    winrt::hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable);
    winrt::hstring KeyToString(winrt::Windows::System::VirtualKey key);
    winrt::hstring LocalizedNameForEnumName(const std::wstring_view sectionAndType, const std::wstring_view enumValue, const std::wstring_view propertyType);
}
