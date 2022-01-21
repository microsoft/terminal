/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once

#include <DefaultSettings.h>
#include <conattrs.hpp>
#include "../../inc/ControlProperties.h"

using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t>;
using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

namespace ControlUnitTests
{
    class MockControlSettings : public winrt::implements<MockControlSettings, winrt::Microsoft::Terminal::Core::ICoreSettings, winrt::Microsoft::Terminal::Control::IControlSettings, winrt::Microsoft::Terminal::Core::ICoreAppearance, winrt::Microsoft::Terminal::Control::IControlAppearance>
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

        winrt::Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept
        {
            return _ColorTable.at(index);
        }
        void SetColorTableEntry(int32_t index,
                                winrt::Microsoft::Terminal::Core::Color color) noexcept
        {
            _ColorTable.at(index) = color;
        }
    };
}
