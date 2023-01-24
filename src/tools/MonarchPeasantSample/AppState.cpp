#include "pch.h"
#include "AppState.h"
#include "../../types/inc/utils.hpp"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace ::Microsoft::Console;

void AppState::_setupConsole()
{
    hOutput = GetStdHandle(STD_OUTPUT_HANDLE);
    hInput = GetStdHandle(STD_INPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOutput, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOutput, dwMode);
}

void AppState::initializeState()
{
    // Initialize the console handles
    _setupConsole();

    // Set up WinRT
    init_apartment();
}

bool AppState::areWeTheKing(const bool logPIDs)
{
    auto kingPID = monarch.GetPID();
    auto ourPID = GetCurrentProcessId();
    if (logPIDs)
    {
        if (ourPID == kingPID)
        {
            printf(fmt::format("We're the\x1b[33m king\x1b[m - our PID is {}\n", ourPID).c_str());
        }
        else
        {
            printf(fmt::format("We're a lowly peasant - the king is {}\n", kingPID).c_str());
        }
    }
    return (ourPID == kingPID);
}

void AppState::remindKingWhoTheyAre(const winrt::MonarchPeasantSample::IPeasant& iPeasant)
{
    winrt::com_ptr<MonarchPeasantSample::implementation::Monarch> monarchImpl;
    monarchImpl.copy_from(winrt::get_self<MonarchPeasantSample::implementation::Monarch>(monarch));
    if (monarchImpl)
    {
        auto ourID = iPeasant.GetID();
        monarchImpl->SetSelfID(ourID);
        monarchImpl->AddPeasant(iPeasant);
        printf("The king is peasant #%lld\n", ourID);
    }
    else
    {
        printf("Shoot, we wanted to be able to get the monarchImpl here but couldn't\n");
    }
}

winrt::MonarchPeasantSample::Monarch AppState::instantiateMonarch()
{
    // Heads up! This only works because we're using
    // "metadata-based-marshalling" for our WinRT types. THat means the OS is
    // using the .winmd file we generate to figure out the proxy/stub
    // definitions for our types automatically. This only works in the following
    // cases:
    //
    // * If we're running unpackaged: the .winmd but be a sibling of the .exe
    // * If we're running packaged: the .winmd must be in the package root
    auto monarch = create_instance<winrt::MonarchPeasantSample::Monarch>(Monarch_clsid,
                                                                         CLSCTX_LOCAL_SERVER);
    return monarch;
}

MonarchPeasantSample::IPeasant AppState::_createOurPeasant()
{
    auto peasant = winrt::make_self<MonarchPeasantSample::implementation::Peasant>();
    auto ourID = monarch.AddPeasant(*peasant);
    printf("The monarch assigned us the ID %llu\n", ourID);

    if (areWeTheKing())
    {
        remindKingWhoTheyAre(*peasant);
    }

    return *peasant;
}

void AppState::createMonarch()
{
    monarch = AppState::instantiateMonarch();
}

// return true to exit early, false if we should continue into the main loop
bool AppState::processCommandline()
{
    const bool isKing = areWeTheKing(false);
    // If we're the king, we _definitely_ want to process the arguments, we were
    // launched with them!
    //
    // Otherwise, the King will tell us if we should make a new window
    const bool createNewWindow = isKing || monarch.ProposeCommandline({ args }, { L"placeholder CWD" });

    if (createNewWindow)
    {
        peasant = _createOurPeasant();
        peasant.ExecuteCommandline({ args }, { L"placeholder CWD" });
        return false;
    }
    else
    {
        printf("The Monarch instructed us to not create a new window. We'll be exiting now.\n");
    }

    return true;
}
