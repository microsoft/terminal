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
    struct ControlAppearance : public winrt::implements<ControlAppearance, Microsoft::Terminal::Core::ICoreAppearance, Microsoft::Terminal::Control::IControlAppearance>
    {
#define SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CORE_APPEARANCE_SETTINGS(SETTINGS_GEN)
        CONTROL_APPEARANCE_SETTINGS(SETTINGS_GEN)
#undef SETTINGS_GEN

    private:
        // Color Table is special because it's an array
        std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> _ColorTable;

        // Color table is _extra_ special because each individual color is
        // overridable, not the whole array.
        std::array<std::optional<winrt::Microsoft::Terminal::Core::Color>, COLOR_TABLE_SIZE> _runtimeColorTable;

    public:
        winrt::Microsoft::Terminal::Core::Color GetColorTableEntry(int32_t index) noexcept
        {
            return til::coalesce_value(_runtimeColorTable.at(index), _ColorTable.at(index));
        }
        void SetColorTableEntry(int32_t index,
                                winrt::Microsoft::Terminal::Core::Color color) noexcept
        {
            _runtimeColorTable.at(index) = color;
        }

        std::array<winrt::Microsoft::Terminal::Core::Color, 16> ColorTable() { return _ColorTable; }
        void ColorTable(std::array<winrt::Microsoft::Terminal::Core::Color, 16> /*colors*/) {}

        ControlAppearance(Control::IControlAppearance appearance)
        {
#define COPY_SETTING(type, name, ...) _##name = appearance.name();
            CORE_APPEARANCE_SETTINGS(COPY_SETTING)
            CONTROL_APPEARANCE_SETTINGS(COPY_SETTING)
#undef COPY_SETTING

            for (int32_t i = 0; i < _ColorTable.size(); i++)
            {
                _ColorTable[i] = appearance.GetColorTableEntry(i);
            }
        }
    };
}
