#include "pch.h"
#include "AzureConnectionSettings.h"
#include "AzureConnectionSettings.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    AzureConnectionSettings::AzureConnectionSettings(uint32_t rows, uint32_t columns) :
        _Rows{ rows },
        _Columns{ columns }
    {
    }
}
