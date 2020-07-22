#include "pch.h"
#include "HostManager.h"

#include "HostManager.g.cpp"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

namespace winrt::ScratchIsland::implementation
{
    HostManager::HostManager()
    {
        _hosts = winrt::single_threaded_observable_vector<winrt::ScratchIsland::HostAndProcess>();
    }

    Collections::IObservableVector<winrt::ScratchIsland::HostAndProcess> HostManager::Hosts()
    {
        return _hosts;
    }

    static wil::unique_process_information _createHostClassProcess(const winrt::guid& g)
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
        THROW_IF_WIN32_BOOL_FALSE(succeeded);
        // if (!succeeded)
        // {
        //     printf("Failed to create host process\n");
        //     return;
        // }

        // Ooof this is dumb, but we need a sleep here to make the server starts.
        // That's _sub par_. Maybe we could use the host's stdout to have them emit
        // a byte when they're set up?
        Sleep(2500);

        // TODO MONDAY - It seems like it takes conhost too long to start up to
        // host the ScratchWinRTServer that even a sleep 100 is too short. However,
        // any longer, and XAML will just crash, because some frame took too long.
        // So we _need_ to do the "have the server explicitly tell us it's ready"
        // thing, and maybe also do it on a bg thread (and signal to the UI thread
        // that it can attach now)

        return std::move(piOne);
    }

    winrt::ScratchIsland::HostAndProcess HostManager::CreateHost()
    {
        // 1. Generate a GUID.
        winrt::guid g{ Utils::CreateGuid() };

        // 2. Spawn a Server.exe, with the guid on the commandline
        auto piContent{ std::move(_createHostClassProcess(g)) };

        auto host = create_instance<winrt::ScratchWinRTServer::HostClass>(g, CLSCTX_LOCAL_SERVER);
        THROW_IF_NULL_ALLOC(host);

        auto hostAndProcess = winrt::make_self<HostAndProcess>(host, std::move(piContent));

        _hosts.Append(*hostAndProcess);

        return *hostAndProcess;
    }
}
