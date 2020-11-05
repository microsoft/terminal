#include "pch.h"
#include <conio.h>
#include "Monarch.h"
#include "Peasant.h"
#include "AppState.h"
#include "../../types/inc/utils.hpp"

bool peasantReadInput(AppState& state)
{
    DWORD cNumRead, i;
    INPUT_RECORD irInBuf[128];

    if (!ReadConsoleInput(state.hInput, // input buffer handle
                          irInBuf, // buffer to read into
                          128, // size of read buffer
                          &cNumRead)) // number of records read
    {
        printf("\x1b[31mReadConsoleInput failed\x1b[m\n");
        ExitProcess(0);
    }
    for (i = 0; i < cNumRead; i++)
    {
        switch (irInBuf[i].EventType)
        {
        case KEY_EVENT: // keyboard input
        {
            auto key = irInBuf[i].Event.KeyEvent;
            if (key.bKeyDown == false &&
                key.uChar.UnicodeChar == L'q')
            {
                return true;
            }
            else
            {
                printf("This window was activated\n");
                winrt::com_ptr<winrt::MonarchPeasantSample::implementation::Peasant> peasantImpl;
                peasantImpl.copy_from(winrt::get_self<winrt::MonarchPeasantSample::implementation::Peasant>(state.peasant));
                if (peasantImpl)
                {
                    peasantImpl->raiseActivatedEvent();
                }
            }
            break;
        }

        case MOUSE_EVENT:
        case WINDOW_BUFFER_SIZE_EVENT:
        case FOCUS_EVENT:
        case MENU_EVENT:
            break;

        default:
            printf("\x1b[33mUnknown event from ReadConsoleInput - this is probably impossible\x1b[m\n");
            ExitProcess(0);
            break;
        }
    }

    return false;
}

bool peasantAppLoop(AppState& state)
{
    wil::unique_handle hMonarch{ OpenProcess(PROCESS_ALL_ACCESS, FALSE, static_cast<DWORD>(state.monarch.GetPID())) };
    // printf("handle for the monarch process is %d\n", hMonarch.get());

    HANDLE handlesToWaitOn[2]{ hMonarch.get(), state.hInput };

    bool exitRequested = false;
    printf("Press `q` to quit\n");

    while (!exitRequested)
    {
        // printf("Beginning wait...\n");
        auto waitResult = WaitForMultipleObjects(2, handlesToWaitOn, false, INFINITE);

        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0: // handlesToWaitOn[0] was signaled
            printf("THE KING IS \x1b[31mDEAD\x1b[m\n");
            // Return false here - this will trigger us to find the new monarch
            return false;

        case WAIT_OBJECT_0 + 1: // handlesToWaitOn[1] was signaled
            exitRequested = peasantReadInput(state);
            break;

        case WAIT_TIMEOUT:
            printf("Wait timed out. This should be impossible.\n");
            break;

        // Return value is invalid.
        default:
        {
            auto gle = GetLastError();
            printf("WaitForMultipleObjects returned: %d\n", waitResult);
            printf("Wait error: %d\n", gle);
            ExitProcess(0);
        }
        }
    }

    printf("Bottom of peasantAppLoop\n");
    return true;
}
