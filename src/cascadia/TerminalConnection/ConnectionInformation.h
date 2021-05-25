#pragma once
#include "../inc/cppwinrt_utils.h"
#include "ConnectionInformation.g.h"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    struct ConnectionInformation : ConnectionInformationT<ConnectionInformation>
    {
        ConnectionInformation(hstring const& className,
                              const Windows::Foundation::Collections::ValueSet& settings);

        static TerminalConnection::ITerminalConnection CreateConnection(TerminalConnection::ConnectionInformation info);

        WINRT_PROPERTY(hstring, ClassName);
        WINRT_PROPERTY(Windows::Foundation::Collections::ValueSet, Settings);
    };
}
namespace winrt::Microsoft::Terminal::TerminalConnection::factory_implementation
{
    BASIC_FACTORY(ConnectionInformation);
}
