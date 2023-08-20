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
        Windows::Foundation::IInspectable inspectable{};

        const auto name = static_cast<HSTRING>(winrt::get_abi(info.ClassName()));
        const auto pointer = winrt::put_abi(inspectable);

#pragma warning(push)
#pragma warning(disable : 26490)
        // C++/WinRT just loves its void**, nothing we can do here _except_ reinterpret_cast
        auto raw = reinterpret_cast<::IInspectable**>(pointer);
#pragma warning(pop)

        // RoActivateInstance() will try to create an instance of the object,
        // who's fully qualified name is the string in Name().
        //
        // The class has to be activatable. For the Terminal, this is easy
        // enough - we're not hosting anything that's not already in our
        // manifest, or living as a .dll & .winmd SxS.
        //
        // When we get to extensions (GH#4000), we may want to revisit.
        if (LOG_IF_FAILED(RoActivateInstance(name, raw)))
        {
            return nullptr;
        }

        // Now that thing we made, make sure it's actually a ITerminalConnection
        if (const auto connection{ inspectable.try_as<TerminalConnection::ITerminalConnection>() })
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
