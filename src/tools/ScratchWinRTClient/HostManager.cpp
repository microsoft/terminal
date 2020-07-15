#include "pch.h"
#include "HostManager.h"

#include "HostManager.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::ScratchWinRTClient::implementation
{
    HostManager::HostManager()
    {
        _hosts = winrt::single_threaded_observable_vector<ScratchWinRTServer::HostClass>();
    }

    Collections::IObservableVector<ScratchWinRTServer::HostClass> HostManager::Hosts()
    {
        return _hosts;
    }

    static void _createHostClassProcess(const winrt::guid& g)
    {
        auto guidStr{ Utils::GuidToString(g) };
        std::wstring commandline{ fmt::format(L"ScratchWinRTServer.exe {}", guidStr) };
        STARTUPINFO siOne{ 0 };
        siOne.cb = sizeof(STARTUPINFOW);
        wil::unique_process_information piOne;
        auto succeeded = CreateProcessW(
            nullptr,
            commandline.data(),
            nullptr, // lpProcessAttributes
            nullptr, // lpThreadAttributes
            false, // bInheritHandles
            CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
            nullptr, // lpEnvironment
            nullptr, // startingDirectory
            &siOne, // lpStartupInfo
            &piOne // lpProcessInformation
        );
        if (!succeeded)
        {
            printf("Failed to create host process\n");
            return;
        }

        // Ooof this is dumb, but we need a sleep here to make the server starts.
        // That's _sub par_. Maybe we could use the host's stdout to have them emit
        // a byte when they're set up?
        Sleep(100);
    }

    ScratchWinRTServer::HostClass HostManager::CreateHost()
    {
        // 1. Generate a GUID.
        winrt::guid g{ Utils::CreateGuid() };

        // 2. Spawn a Server.exe, with the guid on the commandline
        _createHostClassProcess(g);

        auto host = create_instance<winrt::ScratchWinRTServer::HostClass>(g, CLSCTX_LOCAL_SERVER);
        THROW_IF_NULL_ALLOC(host);

        _hosts.Append(host);

        return host;
    }
}
