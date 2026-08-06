/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once

#include <DefaultSettings.h>
#include <conattrs.hpp>
#include "../../inc/ControlProperties.h"

using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;
using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

namespace ControlUnitTests
{
    class MockControlSettings : public winrt::implements<MockControlSettings, winrt::Microsoft::Terminal::Core::ICoreSettings, winrt::Microsoft::Terminal::Control::IControlSettings, winrt::Microsoft::Terminal::Core::ICoreAppearance, winrt::Microsoft::Terminal::Control::IControlAppearance, winrt::Microsoft::Terminal::Core::ICoreScheme>
    {
        // Color Table is special because it's an array
        std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> _ColorTable;

#define SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CORE_SETTINGS(SETTINGS_GEN)
        CORE_APPEARANCE_SETTINGS(SETTINGS_GEN)
        CONTROL_SETTINGS(SETTINGS_GEN)
        CONTROL_APPEARANCE_SETTINGS(SETTINGS_GEN)
#undef SETTINGS_GEN

    public:
        MockControlSettings() = default;

        void SetColorTable(const std::array<winrt::Microsoft::Terminal::Core::Color, 16>& colors)
        {
            _ColorTable = colors;
        }

        void GetColorTable(winrt::com_array<winrt::Microsoft::Terminal::Core::Color>& table) noexcept
        {
            table = winrt::com_array(_ColorTable.begin(), _ColorTable.end());
        }
    };
}
