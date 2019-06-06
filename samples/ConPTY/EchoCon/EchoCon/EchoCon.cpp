// EchoCon.cpp : Entry point for the EchoCon Pseudo-Console sample application.
// Copyright © 2018, Microsoft

#include "stdafx.h"
#include <Windows.h>
#include <process.h>

// Forward declarations
HRESULT CreatePseudoConsoleAndPipes(HPCON*, HANDLE*, HANDLE*);
HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEX*, HPCON);
void __cdecl PipeListener(LPVOID);

int main()
{
    wchar_t szCommand[]{ L"ping localhost" };
    HRESULT hr{ E_UNEXPECTED };
    HANDLE hConsole = { GetStdHandle(STD_OUTPUT_HANDLE) };

    // Enable Console VT Processing
    DWORD consoleMode{};
    GetConsoleMode(hConsole, &consoleMode);
    hr = SetConsoleMode(hConsole, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        ? S_OK
        : GetLastError();
    if (S_OK == hr)
    {
        HPCON hPC{ INVALID_HANDLE_VALUE };

        //  Create the Pseudo Console and pipes to it
        HANDLE hPipeIn{ INVALID_HANDLE_VALUE };
        HANDLE hPipeOut{ INVALID_HANDLE_VALUE };
        hr = CreatePseudoConsoleAndPipes(&hPC, &hPipeIn, &hPipeOut);
        if (S_OK == hr)
        {
            // Create & start thread to listen to the incoming pipe
            // Note: Using CRT-safe _beginthread() rather than CreateThread()
            HANDLE hPipeListenerThread{ reinterpret_cast<HANDLE>(_beginthread(PipeListener, 0, hPipeIn)) };

            // Initialize the necessary startup info struct        
            STARTUPINFOEX startupInfo{};
            if (S_OK == InitializeStartupInfoAttachedToPseudoConsole(&startupInfo, hPC))
            {
                // Launch ping to emit some text back via the pipe
                PROCESS_INFORMATION piClient{};
                hr = CreateProcess(
                    NULL,                           // No module name - use Command Line
                    szCommand,                      // Command Line
                    NULL,                           // Process handle not inheritable
                    NULL,                           // Thread handle not inheritable
                    FALSE,                          // Inherit handles
                    EXTENDED_STARTUPINFO_PRESENT,   // Creation flags
                    NULL,                           // Use parent's environment block
                    NULL,                           // Use parent's starting directory 
                    &startupInfo.StartupInfo,       // Pointer to STARTUPINFO
                    &piClient)                      // Pointer to PROCESS_INFORMATION
                    ? S_OK
                    : GetLastError();

                if (S_OK == hr)
                {
                    // Wait up to 10s for ping process to complete
                    WaitForSingleObject(piClient.hThread, 10 * 1000);

                    // Allow listening thread to catch-up with final output!
                    Sleep(500);
                }

                // --- CLOSEDOWN ---

                // Now safe to clean-up client app's process-info & thread
                CloseHandle(piClient.hThread);
                CloseHandle(piClient.hProcess);

                // Cleanup attribute list
                DeleteProcThreadAttributeList(startupInfo.lpAttributeList);
                free(startupInfo.lpAttributeList);
            }

            // Close ConPTY - this will terminate client process if running
            ClosePseudoConsole(hPC);

            // Clean-up the pipes
            if (INVALID_HANDLE_VALUE != hPipeOut) CloseHandle(hPipeOut);
            if (INVALID_HANDLE_VALUE != hPipeIn) CloseHandle(hPipeIn);
        }
    }

    return S_OK == hr ? EXIT_SUCCESS : EXIT_FAILURE;
}

HRESULT CreatePseudoConsoleAndPipes(HPCON* phPC, HANDLE* phPipeIn, HANDLE* phPipeOut)
{
    HRESULT hr{ E_UNEXPECTED };
    HANDLE hPipePTYIn{ INVALID_HANDLE_VALUE };
    HANDLE hPipePTYOut{ INVALID_HANDLE_VALUE };

    // Create the pipes to which the ConPTY will connect
    if (CreatePipe(&hPipePTYIn, phPipeOut, NULL, 0) &&
        CreatePipe(phPipeIn, &hPipePTYOut, NULL, 0))
    {
        // Determine required size of Pseudo Console
        COORD consoleSize{};
        CONSOLE_SCREEN_BUFFER_INFO csbi{};
        HANDLE hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) };
        if (GetConsoleScreenBufferInfo(hConsole, &csbi))
        {
            consoleSize.X = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            consoleSize.Y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
        }

        // Create the Pseudo Console of the required size, attached to the PTY-end of the pipes
        hr = CreatePseudoConsole(consoleSize, hPipePTYIn, hPipePTYOut, 0, phPC);

        // Note: We can close the handles to the PTY-end of the pipes here
        // because the handles are dup'ed into the ConHost and will be released
        // when the ConPTY is destroyed.
        if (INVALID_HANDLE_VALUE != hPipePTYOut) CloseHandle(hPipePTYOut);
        if (INVALID_HANDLE_VALUE != hPipePTYIn) CloseHandle(hPipePTYIn);
    }

    return hr;
}

// Initializes the specified startup info struct with the required properties and
// updates its thread attribute list with the specified ConPTY handle
HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEX* pStartupInfo, HPCON hPC)
{
    HRESULT hr{ E_UNEXPECTED };

    if (pStartupInfo)
    {
        size_t attrListSize{};

        pStartupInfo->StartupInfo.cb = sizeof(STARTUPINFOEX);

        // Get the size of the thread attribute list.
        InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);

        // Allocate a thread attribute list of the correct size
        pStartupInfo->lpAttributeList =
            reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));

        // Initialize thread attribute list
        if (pStartupInfo->lpAttributeList
            && InitializeProcThreadAttributeList(pStartupInfo->lpAttributeList, 1, 0, &attrListSize))
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

void __cdecl PipeListener(LPVOID pipe)
{
    HANDLE hPipe{ pipe };
    HANDLE hConsole{ GetStdHandle(STD_OUTPUT_HANDLE) };

    const DWORD BUFF_SIZE{ 512 };
    char szBuffer[BUFF_SIZE]{};

    DWORD dwBytesWritten{};
    DWORD dwBytesRead{};
    BOOL fRead{ FALSE };
    do
    {
        // Read from the pipe
        fRead = ReadFile(hPipe, szBuffer, BUFF_SIZE, &dwBytesRead, NULL);

        // Write received text to the Console
        // Note: Write to the Console using WriteFile(hConsole...), not printf()/puts() to
        // prevent partially-read VT sequences from corrupting output
        WriteFile(hConsole, szBuffer, dwBytesRead, &dwBytesWritten, NULL);

    } while (fRead && dwBytesRead >= 0);
}
