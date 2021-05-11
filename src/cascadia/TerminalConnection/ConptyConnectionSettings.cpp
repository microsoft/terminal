#include "pch.h"
#include "ConptyConnectionSettings.h"
#include "ConptyConnectionSettings.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    ConptyConnectionSettings::ConptyConnectionSettings(hstring const& cmdline,
                                                       hstring const& startingDirectory,
                                                       hstring const& startingTitle,
                                                       Windows::Foundation::Collections::IMapView<hstring, hstring> const& environment,
                                                       uint32_t rows,
                                                       uint32_t columns,
                                                       winrt::guid const& guid) :
        _Cmdline{ cmdline },
        _StartingDirectory{ startingDirectory },
        _StartingTitle{ startingTitle },
        _Environment{ environment },
        _Rows{ rows },
        _Columns{ columns },
        _Guid{ guid }
    {
    }
};
