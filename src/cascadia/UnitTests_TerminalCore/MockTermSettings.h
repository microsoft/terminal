#pragma once

#include "pch.h"
#include <WexTestClass.h>

#include "DefaultSettings.h"
#include "../../inc/ControlProperties.h"

#include <winrt/Microsoft.Terminal.Core.h>
#include <til/winrt.h>

using namespace winrt::Microsoft::Terminal::Core;

namespace TerminalCoreUnitTests
{
    class MockTermSettings : public winrt::implements<MockTermSettings, ICoreSettings, ICoreAppearance, ICoreScheme>
    {
        // Color Table is special because it's an array
        std::array<winrt::Microsoft::Terminal::Core::Color, COLOR_TABLE_SIZE> _ColorTable;

    public:
#define SETTINGS_GEN(type, name, ...) til::property<type> name{ __VA_ARGS__ };
        CORE_SETTINGS(SETTINGS_GEN)
        CORE_APPEARANCE_SETTINGS(SETTINGS_GEN)
#undef SETTINGS_GEN

    public:
        MockTermSettings(int32_t historySize, int32_t initialRows, int32_t initialCols) :
            HistorySize(historySize),
            InitialRows(initialRows),
            InitialCols(initialCols)
        {
        }

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
