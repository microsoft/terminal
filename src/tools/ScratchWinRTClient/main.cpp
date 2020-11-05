#include "pch.h"
#include <conio.h>
#include "HostManager.h"
#include <winrt/ScratchWinRTServer.h>
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

void createExistingObjectApp(int /*argc*/, char** argv)
{
    winrt::guid guidFromCmdline{};
    std::string guidString{ argv[1] };
    auto canConvert = guidString.length() == 38 && guidString.front() == '{' && guidString.back() == '}';
    if (canConvert)
    {
        std::wstring wideGuidStr{ til::u8u16(guidString) };
        printf("\x1b[90mCLIENT: Found GUID:%ls\x1b[m\n", wideGuidStr.c_str());
        GUID result{};
        THROW_IF_FAILED(IIDFromString(wideGuidStr.c_str(), &result));
        guidFromCmdline = result;
    }
    if (guidFromCmdline == winrt::guid{})
    {
        printf("client did not recieve GUID, early returning.");
        return; // -1;
    }

    winrt::ScratchWinRTServer::HostClass host{ nullptr };
    {
        HANDLE hProcessToken;
        auto processTokenCleanup = wil::scope_exit([&] { CloseHandle(hProcessToken); });

        // Open a handle to the access token for the
        // calling process that is the currently login access token
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ALL_ACCESS, &hProcessToken))
        {
            printf("OpenProcessToken()-Getting the handle to access token failed, error %u\n", GetLastError());
        }
        else
        {
            printf("OpenProcessToken()-Got the handle to access token!\n");
        }

        {
            TOKEN_ELEVATION tokenElevation{ 0 };
            DWORD neededTokenInformationLength = 0;
            if (GetTokenInformation(hProcessToken,
                                    TOKEN_INFORMATION_CLASS::TokenElevation,
                                    &tokenElevation,
                                    sizeof(tokenElevation),
                                    &neededTokenInformationLength))
            {
                printf("GetTokenInformation(TokenElevation) succeeded\n");
                printf("Token is elevated? - %s\n", (tokenElevation.TokenIsElevated != 0 ? "true" : "false"));
            }
            else
            {
                auto gle = GetLastError();
                printf("GetTokenInformation(TokenElevation) failed: %d\n", gle);
            }
        }

        TOKEN_LINKED_TOKEN tokenLinkedToken = {};
        DWORD neededTokenInformationLength = 0;
        if (!GetTokenInformation(hProcessToken,
                                 TokenLinkedToken,
                                 &tokenLinkedToken,
                                 sizeof(tokenLinkedToken),
                                 &neededTokenInformationLength))
        {
            auto gle = GetLastError();
            printf("GetTokenInformation(TokenLinkedToken) failed: %d\n", gle);
            return;
        }
        else
        {
            printf("Got the linked token for this process\n");
        }
        auto tokenLinkedTokenCleanup = wil::scope_exit([&] { CloseHandle(tokenLinkedToken.LinkedToken); });

        // THIS IS THE DAMNDEST THING
        //
        // IF YOU DO THIS, THE PROCESS WILL JUST STRAIGHT UP DIE ON THE create_instance CALL
        if (SetThreadToken(nullptr, tokenLinkedToken.LinkedToken))
        {
            printf("SetThreadToken() succeeded\n");
        }
        else
        {
            printf("SetThreadToken failed %x\n", GetLastError());
        }

        // // Lets the calling process impersonate the security context of a
        // // logged-on user. UNFORTUNATELY, this did not work for me.
        // // * ImpersonateLoggedOnUser(tokenLinkedToken.LinkedToken) does the same
        // //   thing as SetThreadToken(LinkedToken), it crashes when trying to
        // //   create_instance.
        // // * ImpersonateLoggedOnUser(hProcessToken) does seemingly nothing at
        // //   all - we get an "Error: Class not registered"
        //
        // auto revert = wil::scope_exit([&]() {
        //     // Terminates the impersonation of a client.
        //     if (RevertToSelf())
        //     {
        //         printf("Impersonation was terminated.\n");
        //     }
        //     else
        //     {
        //         auto gle = GetLastError();
        //         printf("RevertToSelf failed, %d\n", gle);
        //     }
        // });
        // // if (ImpersonateLoggedOnUser(hProcessToken))
        // if (ImpersonateLoggedOnUser(tokenLinkedToken.LinkedToken))
        // {
        //     printf("ImpersonateLoggedOnUser() is OK.\n");
        // }
        // else
        // {
        //     printf("ImpersonateLoggedOnUser() failed, error %u.\n", GetLastError());
        //     exit(-1);
        // }

        printf("Calling create_instance...\n");

        host = create_instance<winrt::ScratchWinRTServer::HostClass>(guidFromCmdline,
                                                                     // CLSCTX_LOCAL_SERVER);
                                                                     CLSCTX_LOCAL_SERVER | CLSCTX_ENABLE_CLOAKING);
        printf("Done\n");
    }

    if (!host)
    {
        printf("Could not get the existing HostClass\n");
        return;
    }
    else
    {
        printf("Got the existing HostClass\n");
    }

    // The DoCount could be anything, depending on which of the hosts we're creating.
    printf("DoCount: %d (Expected: ?)\n",
           host.DoCount());
}

void createHostClassProcess(const winrt::guid& g)
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
        printf("Failed to create first host process\n");
        return;
    }

    // Ooof this is dumb, but we need a sleep here to make the server starts.
    // That's _sub par_. Maybe we could use the host's stdout to have them emit
    // a byte when they're set up?
    Sleep(1000);
}

void scratchApp()
{
    printf("scratchApp\n");

    // 1. Generate a GUID.
    // 2. Spawn a Server.exe, with the guid on the commandline
    // 3. Make an instance of that GUID, as a HostClass
    // 4. Call HostClass::DoTheThing, and get the count with HostClass::DoCount [1]
    // 5. Make another instance of HostClass, and get the count with HostClass::DoCount. It should be the same. [1, 1]
    // 6. On the second HostClass, call DoTheThing. Verify that both instances have the same DoCount. [2. 2]
    // 7. Create a second Server.exe, and create a Third HostClass, using that GUID.
    // 8. Call DoTheThing on the third, and verify the counts of all three instances. [2, 2, 1]
    // 9. QUESTION: Does releasing the first instance leave the first object alive, since the second instance still points at it?

    // 1. Generate a GUID.
    winrt::guid firstGuid{ Utils::CreateGuid() };

    // 2. Spawn a Server.exe, with the guid on the commandline
    createHostClassProcess(firstGuid);

    // 3. Make an instance of that GUID, as a HostClass
    printf("Trying to directly create a HostClass...\n");
    auto firstHost = create_instance<winrt::ScratchWinRTServer::HostClass>(firstGuid, CLSCTX_LOCAL_SERVER);

    if (!firstHost)
    {
        printf("Could not get the first HostClass\n");
        return;
    }

    printf("DoCount: %d (Expected: 0)\n",
           firstHost.DoCount());
    // 4. Call HostClass::DoTheThing, and get the count with HostClass::DoCount [1]
    firstHost.DoTheThing();
    printf("DoCount: %d (Expected: 1)\n",
           firstHost.DoCount());

    // 5. Make another instance of HostClass, and get the count with HostClass::DoCount. It should be the same. [1, 1]
    auto secondHost = create_instance<winrt::ScratchWinRTServer::HostClass>(firstGuid, CLSCTX_LOCAL_SERVER);
    if (!secondHost)
    {
        printf("Could not get the second HostClass\n");
        return;
    }
    printf("DoCount: [%d, %d] (Expected: [1, 1])\n",
           firstHost.DoCount(),
           secondHost.DoCount());

    // 6. On the second HostClass, call DoTheThing. Verify that both instances have the same DoCount. [2. 2]
    secondHost.DoTheThing();
    printf("DoCount: [%d, %d] (Expected: [2, 2])\n",
           firstHost.DoCount(),
           secondHost.DoCount());

    // 7. Create a second Server.exe, and create a Third HostClass, using that GUID.
    winrt::guid secondGuid{ Utils::CreateGuid() };
    createHostClassProcess(secondGuid);
    auto thirdHost = create_instance<winrt::ScratchWinRTServer::HostClass>(secondGuid, CLSCTX_LOCAL_SERVER);
    if (!thirdHost)
    {
        printf("Could not get the third HostClass\n");
        return;
    }
    printf("DoCount: [%d, %d, %d] (Expected: [2, 2, 0])\n",
           firstHost.DoCount(),
           secondHost.DoCount(),
           thirdHost.DoCount());
    // 8. Call DoTheThing on the third, and verify the counts of all three instances. [2, 2, 1]
    thirdHost.DoTheThing();
    printf("DoCount: [%d, %d, %d] (Expected: [2, 2, 1])\n",
           firstHost.DoCount(),
           secondHost.DoCount(),
           thirdHost.DoCount());
}

static void printHosts(const ScratchWinRTClient::HostManager& manager)
{
    int index = 0;
    for (const auto& h : manager.Hosts())
    {
        auto guidStr{ Utils::GuidToString(h.Id()) };
        printf("Host[%d]: DoCount=%d %ls\n", index, h.DoCount(), guidStr.c_str());
        index++;
    }
    if (index == 0)
    {
        printf("<No hosts>\n");
    }
}

void managerApp()
{
    ScratchWinRTClient::HostManager manager;
    printHosts(manager);

    printf("Create host 0:\n");
    auto host0 = manager.CreateHost();
    printHosts(manager);

    printf("Create host 1:\n");
    auto host1 = manager.CreateHost();
    host1.DoTheThing();
    printHosts(manager);

    printf("Create host 2:\n");
    auto host2 = manager.CreateHost();
    host2.DoTheThing();
    host2.DoTheThing();
    printHosts(manager);

    printf("Create host 3:\n");
    auto host3 = manager.CreateHost();
    host3.DoTheThing();
    host3.DoTheThing();
    host3.DoTheThing();
    printHosts(manager);

    printf("increment host 0:\n");

    host0.DoTheThing();
    host0.DoTheThing();
    host0.DoTheThing();
    host0.DoTheThing();
    printHosts(manager);

    bool exitRequested = false;
    while (!exitRequested)
    {
        printf("-----------------------------\n");
        printf("input a command (l, i, c, q): ");
        const auto ch = _getch();
        printf("\n");
        if (ch == 'l')
        {
            printHosts(manager);
        }
        else if (ch == 'i')
        {
            printf("input a host to increment: ");
            const auto ch2 = _getch();
            if (ch2 >= '0' && ch2 <= '9')
            {
                uint32_t index = ((int)(ch2)) - ((int)('0'));
                if (index < manager.Hosts().Size())
                {
                    manager.Hosts().GetAt(index).DoTheThing();
                    printHosts(manager);
                }
            }
        }
        else if (ch == 'c')
        {
            printf("Creating a new host\n");
            manager.CreateHost();
            printHosts(manager);
        }
        else if (ch == 'q')
        {
            exitRequested = true;
        }
    }
}

int main(int argc, char** argv)
{
    auto hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);

    init_apartment();

    try
    {
        // If a GUID was passed on the commandline, then try to instead make an instance of that class.
        if (argc > 1)
        {
            createExistingObjectApp(argc, argv);
        }
        else
        {
            managerApp();
        }
    }
    catch (hresult_error const& e)
    {
        printf("Error: %ls\n", e.message().c_str());
    }
    catch (...)
    {
        printf("Some other exception happened\n");
        LOG_CAUGHT_EXCEPTION();
    }

    printf("Exiting client");
}
