// EchoCon.cpp : Entry point for the EchoCon sample application.
//

#include "stdafx.h"
#include <Windows.h>
#include <conio.h>
#include <process.h>
#include <wchar.h>

// Forward declarations
void __cdecl PipeListener(LPVOID);
HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEX*, HPCON);
COORD GetConsoleSize(HANDLE);

int main()
{
    wchar_t szCommand[] = L"ping localhost";
    HRESULT hr{ S_OK };
    HANDLE hPipeIn{ INVALID_HANDLE_VALUE };
    HANDLE hPipeOut{ INVALID_HANDLE_VALUE };
    HANDLE hPipePTYIn{ INVALID_HANDLE_VALUE };
    HANDLE hPipePTYOut{ INVALID_HANDLE_VALUE };
    HPCON hPC{ INVALID_HANDLE_VALUE };

    PROCESS_INFORMATION piClient{};
    STARTUPINFOEX startupInfo{};

    HANDLE hConsole = { GetStdHandle(STD_OUTPUT_HANDLE) };
    DWORD consoleMode{};

    // Enable VT Processing
    GetConsoleMode(hConsole, &consoleMode);
    hr = SetConsoleMode(hConsole, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        ? S_OK
        : GetLastError();
    if (S_OK == hr)
    {
        // Create the pipes to which the ConPTY will connect
        if (CreatePipe(&hPipePTYIn, &hPipeOut, NULL, 0) &&
            CreatePipe(&hPipeIn, &hPipePTYOut, NULL, 0))
        {
            // Create & start thread to listen to the incoming pipe
            // NB: Using CRT-safe _beginthread() rather than CreateThread()
            DWORD pipeListenerThreadId{};
            HANDLE hPipeListenerThread{ 
                reinterpret_cast<HANDLE>(_beginthread(PipeListener, 0, hPipeIn)) };

            // Create appropriately sized ConPTY
            COORD consoleSize{ GetConsoleSize(hConsole) };
            hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut, 0, &hPC);

            // Initialize the necessary startup info struct        
            if (S_OK == InitializeStartupInfoAttachedToPseudoConsole(&startupInfo, hPC))
            {
                // Launch ping to echo some text back via the pipe
                hr = CreateProcess(
                    NULL,                           // No module name - use Command Line
                    szCommand,                      // Command Line
                    NULL,                           // Process handle not inheritable
                    NULL,                           // Thread handle not inheritable
                    TRUE,                           // Inherit handles
                    EXTENDED_STARTUPINFO_PRESENT,   // Creation flags
                    NULL,                           // Use parent's environment block
                    NULL,                           // Use parent's starting directory 
                    &startupInfo.StartupInfo,       // Pointer to STARTUPINFO
                    &piClient)                      // Pointer to PROCESS_INFORMATION
                    ? S_OK
                    : GetLastError();

                if (S_OK == hr)
                {
                    // Wait until ping completes
                    WaitForSingleObject(piClient.hThread, 10 * 1000);

                    // Allow listening thread to catch-up with final output!
                    Sleep(500);
                }

                // --- CLOSEDOWN ---
                // Close ConPTY - this will terminate client process if running
                ClosePseudoConsole(hPC);

                // Now safe to clean-up client process info process & thread
                CloseHandle(piClient.hThread);
                CloseHandle(piClient.hProcess);

                // Cleanup attribute list
                if (startupInfo.lpAttributeList)
                {
                    DeleteProcThreadAttributeList(startupInfo.lpAttributeList);
                    free(startupInfo.lpAttributeList);
                }
            }
            else
            {
                wprintf_s(L"Error: Failed to initialize StartupInfo attached to ConPTY [0x%x]",
                    GetLastError());
            }
        }
        else
        {
            wprintf_s(L"Error: Failed to create pipes");
        }

        // Clean-up the pipes
        if (hPipePTYOut != INVALID_HANDLE_VALUE) CloseHandle(hPipePTYOut);
        if (hPipePTYIn != INVALID_HANDLE_VALUE) CloseHandle(hPipePTYIn);
        if (hPipeOut != INVALID_HANDLE_VALUE) CloseHandle(hPipeOut);
        if (hPipeIn != INVALID_HANDLE_VALUE) CloseHandle(hPipeIn);

        // Restore console to its original mode.
        hr = SetConsoleMode(hConsole, consoleMode)
            ? S_OK
            : GetLastError();
    }

    return hr == S_OK;
}

void __cdecl PipeListener(LPVOID pipe)
{
    HANDLE hPipe{ pipe };
    HANDLE hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) };

    const DWORD BUFF_SIZE{ 512 };
    char pszBuffer[BUFF_SIZE]{};

    DWORD dwBytesWritten{};
    DWORD dwBytesRead{};
    BOOL fRead{ FALSE };
    do
    {
        // Read from the pipe
        fRead = ReadFile(hPipe, pszBuffer, BUFF_SIZE, &dwBytesRead, NULL);

        // Write received text to the Console
        WriteFile(hConsole, pszBuffer, dwBytesRead, &dwBytesWritten, NULL);

    } while (fRead && dwBytesRead >= 0);
}

// Initializes the specified startup info struct with the required properties and
// updates its thread attribute list with the specified ConPTY handle
HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEX* pStartupInfo, HPCON hPC)
{
    HRESULT hr{ E_UNEXPECTED };

    if (pStartupInfo)
    {
        size_t size{};

        pStartupInfo->StartupInfo.cb = sizeof(STARTUPINFOEX);

        // Get the size of the thread attribute list.
        InitializeProcThreadAttributeList(NULL, 1, 0, &size);

        // Allocate a thread attribute list of the correct size
        pStartupInfo->lpAttributeList = 
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(size));

        // Initialize thread attribute list
        if (pStartupInfo->lpAttributeList
            && InitializeProcThreadAttributeList(pStartupInfo->lpAttributeList, 1, 0, &size))
        {
            // Set Pseudo Console attribute
            hr = UpdateProcThreadAttribute(
                pStartupInfo->lpAttributeList,
                0,
                PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                hPC,
                sizeof(HPCON),
                NULL,
                NULL)
                ? S_OK
                : HRESULT_FROM_WIN32(GetLastError());
        }
        else
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }
    }
    return hr;
}

COORD GetConsoleSize(HANDLE hStdOut)
{
    COORD size = { 0, 0 };

    CONSOLE_SCREEN_BUFFER_INFO csbi{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    {
        size.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        size.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }

    return size;
}
