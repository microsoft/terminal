#pragma once

#include "pch.h"
#include <WexTestClass.h>

#include "DefaultSettings.h"
#include "../../inc/ControlProperties.h"

#include <winrt/Microsoft.Terminal.Core.h>

using namespace MTCore;

namespace TerminalCoreUnitTests
{
    class MockTermSettings : public winrt::implements<MockTermSettings, ICoreSettings, ICoreAppearance>
    {
        // Color Table is special because it's an array
        std::array<MTCore::Color, COLOR_TABLE_SIZE> _ColorTable;

#define SETTINGS_GEN(type, name, ...) WINRT_PROPERTY(type, name, __VA_ARGS__);
        CORE_SETTINGS(SETTINGS_GEN)
        CORE_APPEARANCE_SETTINGS(SETTINGS_GEN)
#undef SETTINGS_GEN

    public:
        MockTermSettings(int32_t historySize, int32_t initialRows, int32_t initialCols) :
            _HistorySize(historySize),
            _InitialRows(initialRows),
            _InitialCols(initialCols)
        {
        }

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
