#pragma once
#include "AzureConnectionSettings.g.h"
#include "../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct AzureConnectionSettings : AzureConnectionSettingsT<AzureConnectionSettings>
    {
        AzureConnectionSettings(uint32_t rows, uint32_t columns);
        WINRT_PROPERTY(uint32_t, Rows);
        WINRT_PROPERTY(uint32_t, Columns);
    };
}
namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(AzureConnectionSettings);
}
