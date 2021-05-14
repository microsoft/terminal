#pragma once
#include "ConptyConnectionSettings.g.h"
#include "../inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConptyConnectionSettings : ConptyConnectionSettingsT<ConptyConnectionSettings>
    {
        // WINRT_PROPERTY will be VERY unhappy if you use this templated type
        // directly in the macro, so just typedef a helper here.
        using EnvironmentMap = Windows::Foundation::Collections::IMapView<hstring, hstring>;
        ConptyConnectionSettings(hstring const& cmdline,
                                 hstring const& startingDirectory,
                                 hstring const& startingTitle,
                                 Windows::Foundation::Collections::IMapView<hstring, hstring> const& environment,
                                 uint32_t rows,
                                 uint32_t columns,
                                 winrt::guid const& guid);

        WINRT_PROPERTY(hstring, Cmdline);
        WINRT_PROPERTY(hstring, StartingDirectory);
        WINRT_PROPERTY(hstring, StartingTitle);
        WINRT_PROPERTY(EnvironmentMap, Environment, nullptr);
        WINRT_PROPERTY(uint32_t, Rows);
        WINRT_PROPERTY(uint32_t, Columns);
        WINRT_PROPERTY(winrt::guid, Guid);
    };
}
namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(ConptyConnectionSettings);
}
