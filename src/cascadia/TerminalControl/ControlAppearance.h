/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.
--*/
#pragma once
#include "../../inc/cppwinrt_utils.h"
#include "../../inc/ControlProperties.h"

#include <DefaultSettings.h>
#include <conattrs.hpp>

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlAppearance : public winrt::implements<ControlAppearance, MTCore::ICoreAppearance, MTControl::IControlAppearance>
    {
#define SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CORE_APPEARANCE_SETTINGS(SETTINGS_GEN)
        CONTROL_APPEARANCE_SETTINGS(SETTINGS_GEN)
#undef SETTINGS_GEN

    private:
        // Color Table is special because it's an array
        std::array<MTCore::Color, COLOR_TABLE_SIZE> _ColorTable;

    public:
        MTCore::Color GetColorTableEntry(int32_t index) noexcept
        {
            return _ColorTable.at(index);
        }
        void SetColorTableEntry(int32_t index,
                                MTCore::Color color) noexcept
        {
            _ColorTable.at(index) = color;
        }

        ControlAppearance(MTControl::IControlAppearance appearance)
        {
#define COPY_SETTING(type, name, ...) _##name = appearance.name();
            CORE_APPEARANCE_SETTINGS(COPY_SETTING)
            CONTROL_APPEARANCE_SETTINGS(COPY_SETTING)
#undef COPY_SETTING

            for (size_t i = 0; i < _ColorTable.size(); i++)
            {
                _ColorTable[i] = appearance.GetColorTableEntry(static_cast<int32_t>(i));
            }
        }
    };
}
