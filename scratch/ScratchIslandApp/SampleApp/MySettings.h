/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once
#include "../../inc/cppwinrt_utils.h"
#include "../types/inc/colorTable.hpp"
#include "../../inc/ControlProperties.h"

#include <DefaultSettings.h>
#include <conattrs.hpp>
#include "MySettings.g.h"

using IFontFeatureMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, uint32_t>;
using IFontAxesMap = winrt::Windows::Foundation::Collections::IMap<winrt::hstring, float>;

namespace winrt::SampleApp::implementation
{
    struct MySettings : MySettingsT<MySettings>
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
        winrt::Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept { return _ColorTable.at(index); }
        std::array<winrt::Microsoft::Terminal::Core::Color, 16> ColorTable() { return _ColorTable; }
        void ColorTable(std::array<winrt::Microsoft::Terminal::Core::Color, 16> /*colors*/) {}

        MySettings()
        {
            const auto campbellSpan = ::Microsoft::Console::Utils::CampbellColorTable();
            std::transform(campbellSpan.begin(), campbellSpan.end(), _ColorTable.begin(), [](auto&& color) {
                return static_cast<winrt::Microsoft::Terminal::Core::Color>(til::color{ color });
            });
        }
    };
}

namespace winrt::SampleApp::factory_implementation
{
    BASIC_FACTORY(MySettings);
}
