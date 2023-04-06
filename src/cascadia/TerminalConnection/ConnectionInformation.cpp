#include "pch.h"
#include "ConnectionInformation.h"
#include "ConnectionInformation.g.cpp"

namespace winrt::Microsoft::Terminal::TerminalConnection::implementation
{
    ConnectionInformation::ConnectionInformation(const hstring& className,
                                                 const Windows::Foundation::Collections::ValueSet& settings) :
        _ClassName{ className },
        _Settings{ settings }
    {
    }

    // Function Description:
    // - Create an instance of the connection specified in the
    //   ConnectionInformation, and Initialize it.
    // - This static method allows the content process to create a connection
    //   from information that lives in the window process.
    // Arguments:
    // - info: A ConnectionInformation object that possibly lives out-of-proc,
    //   containing the name of the WinRT class we should activate for this
    //   connection, and a bag of setting to use to initialize that object.
    // Return Value:
    // - <none>
    TerminalConnection::ITerminalConnection ConnectionInformation::CreateConnection(TerminalConnection::ConnectionInformation info)
    try
    {
        // pilfered from `winrt_get_activation_factory` in module.g.cpp. Does a
        // reverse string match, so that we check the classname first, skipping
        // the namespace prefixes.
        static auto requal = [](std::wstring_view const& left, std::wstring_view const& right) noexcept -> bool {
            return std::equal(left.rbegin(), left.rend(), right.rbegin(), right.rend());
        };

        TerminalConnection::ITerminalConnection connection{ nullptr };

        // A couple short-circuits, for connections that _we_ implement.
        if (requal(info.ClassName(), winrt::name_of<TerminalConnection::ConptyConnection>()))
        {
            connection = TerminalConnection::ConptyConnection();
        }
        else if (requal(info.ClassName(), winrt::name_of<TerminalConnection::AzureConnection>()))
        {
            connection = TerminalConnection::AzureConnection();
        }
        else if (requal(info.ClassName(), winrt::name_of<TerminalConnection::EchoConnection>()))
        {
            connection = TerminalConnection::EchoConnection();
        }
        // We don't want to instantiate anything else that we weren't expecting.
        //
        // When we get to extensions (GH#4000), we may want to revisit, and try
        // if (LOG_IF_FAILED(RoActivateInstance(name, raw)))

        // Now that thing we made, make sure it's actually a ITerminalConnection
        if (connection)
        {
            // Initialize it, and return it.
            connection.Initialize(info.Settings());
            return connection;
        }
        return nullptr;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return nullptr;
    }
}
