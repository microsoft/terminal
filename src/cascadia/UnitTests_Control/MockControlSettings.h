/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once

#include <DefaultSettings.h>
#include <conattrs.hpp>
#include "../../inc/ControlProperties.h"

using IFontFeatureMap = WFC::IMap<winrt::hstring, uint32_t>;
using IFontAxesMap = WFC::IMap<winrt::hstring, float>;

namespace ControlUnitTests
{
    class MockControlSettings : public winrt::implements<MockControlSettings, MTCore::ICoreSettings, MTControl::IControlSettings, MTCore::ICoreAppearance, MTControl::IControlAppearance>
    {
        // Color Table is special because it's an array
        std::array<MTCore::Color, COLOR_TABLE_SIZE> _ColorTable;

#define SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CORE_SETTINGS(SETTINGS_GEN)
        CORE_APPEARANCE_SETTINGS(SETTINGS_GEN)
        CONTROL_SETTINGS(SETTINGS_GEN)
        CONTROL_APPEARANCE_SETTINGS(SETTINGS_GEN)
#undef SETTINGS_GEN

    public:
        MockControlSettings() = default;

        MTCore::Color GetColorTableEntry(int32_t index) noexcept
        {
            return _ColorTable.at(index);
        }
        void SetColorTableEntry(int32_t index,
                                MTCore::Color color) noexcept
        {
            _ColorTable.at(index) = color;
        }
    };
}
