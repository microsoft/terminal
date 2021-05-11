#include "pch.h"
#include "ConnectionInformation.h"
#include "ConnectionInformation.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    ConnectionInformation::ConnectionInformation(hstring const& className,
                                                 TerminalConnection::IConnectionSettings settings) :
        _ClassName{ className },
        _Settings{ settings }
    {
    }

    TerminalConnection::ITerminalConnection ConnectionInformation::CreateConnection(
        TerminalConnection::ConnectionInformation info)
    {
        Windows::Foundation::IInspectable coolInspectable{};

        auto name = static_cast<HSTRING>(winrt::get_abi(info.ClassName()));
        auto foo = winrt::put_abi(coolInspectable);
        ::IInspectable** bar = reinterpret_cast<::IInspectable**>(foo);

        if (LOG_IF_FAILED(RoActivateInstance(name, bar)))
        {
            return nullptr;
        }

        if (auto conn2{ coolInspectable.try_as<TerminalConnection::ITerminalConnection>() })
        {
            conn2.Initialize(info.Settings());
            return conn2;
        }
        return nullptr;
    }
}
