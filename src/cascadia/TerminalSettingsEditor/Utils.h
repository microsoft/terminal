// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Settings
{
    winrt::hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable);
    winrt::hstring KeyToString(winrt::Windows::System::VirtualKey key);
}
