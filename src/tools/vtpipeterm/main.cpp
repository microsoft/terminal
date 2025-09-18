// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <Windows.h>

#ifndef __INSIDE_WINDOWS
#define CONPTY_IMPEXP
#include <conpty-static.h>
#else // Building inside Windows, just use the kernel32 ones.
#define ConptyCreatePseudoConsole CreatePseudoConsole
#define ConptyReleasePseudoConsole ReleasePseudoConsole
#define ConptyResizePseudoConsole ResizePseudoConsole
#endif

#include <wil/win32_helpers.h>

#include <string_view>

#define CONSOLE_READ_NOWAIT 0x0002

BOOL WINAPI ReadConsoleInputExA(
    _In_ HANDLE hConsoleInput,
    _Out_writes_(nLength) PINPUT_RECORD lpBuffer,
    _In_ DWORD nLength,
    _Out_ LPDWORD lpNumberOfEventsRead,
    _In_ USHORT wFlags);

// Forward declare the bits from types/inc/utils.hpp that we need so we don't need to pull in a dozen STL headers.
namespace Microsoft::Console::Utils
{
    struct Pipe
    {
        wil::unique_hfile server;
        wil::unique_hfile client;
    };
    Pipe CreateOverlappedPipe(DWORD openMode, DWORD bufferSize);
}

static Microsoft::Console::Utils::Pipe pipe;

static COORD getViewportSize()
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex{ .cbSize = sizeof(csbiex) };
    THROW_IF_WIN32_BOOL_FALSE(GetConsoleScreenBufferInfoEx(GetStdHandle(STD_OUTPUT_HANDLE), &csbiex));
    const SHORT w = csbiex.dwSize.X;
    const SHORT h = csbiex.srWindow.Bottom - csbiex.srWindow.Top + 1;
    return { w, h };
}

static int run(int argc, const wchar_t* argv[])
{
    const auto inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    const auto outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    wil::unique_hfile debugOutput;

    for (int i = 1; i < argc; ++i)
    {
        if (wcscmp(argv[i], L"--out") == 0 && (i + 1) < argc)
        {
            debugOutput.reset(CreateFileW(argv[++i], GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
            THROW_LAST_ERROR_IF(!debugOutput);
        }
        else
        {
            static constexpr auto help =
                "USAGE:\r\n"
                "  VtPipeTerm [OPTIONS]\r\n"
                "\r\n"
                "OPTIONS:\r\n"
                "  -h, --help\r\n"
                "            Display this help message\r\n"
                "      --out <PATH>\r\n"
                "            Dump output to PATH\r\n";

            WriteFile(outputHandle, help, static_cast<DWORD>(strlen(help)), nullptr, nullptr);
            return wcscmp(argv[i], L"-h") == 0 || wcscmp(argv[i], L"--help") == 0 ? 0 : 1;
        }
    }

    static const auto pReadConsoleInputExA = GetProcAddressByFunctionDeclaration(GetModuleHandleW(L"kernel32.dll"), ReadConsoleInputExA);
    THROW_LAST_ERROR_IF_NULL(pReadConsoleInputExA);

    pipe = Microsoft::Console::Utils::CreateOverlappedPipe(PIPE_ACCESS_DUPLEX, 128 * 1024);

    auto viewportSize = getViewportSize();

    HPCON hPC = nullptr;
    THROW_IF_FAILED(ConptyCreatePseudoConsole(viewportSize, pipe.client.get(), pipe.client.get(), PSEUDOCONSOLE_INHERIT_CURSOR, &hPC));
    pipe.client.reset();

    PROCESS_INFORMATION pi;
    {
        wchar_t commandLine[MAX_PATH] = LR"(C:\Windows\System32\cmd.exe)";

        STARTUPINFOEX siEx{};
        siEx.StartupInfo.cb = sizeof(STARTUPINFOEX);
        siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;

        char attrList[128];
        SIZE_T size = sizeof(attrList);
        siEx.lpAttributeList = reinterpret_cast<PPROC_THREAD_ATTRIBUTE_LIST>(&attrList[0]);
        THROW_IF_WIN32_BOOL_FALSE(InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0, &size));
        THROW_IF_WIN32_BOOL_FALSE(UpdateProcThreadAttribute(siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, hPC, sizeof(HPCON), nullptr, nullptr));

        THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(
            /* lpApplicationName    */ nullptr,
            /* lpCommandLine        */ commandLine,
            /* lpProcessAttributes  */ nullptr,
            /* lpThreadAttributes   */ nullptr,
            /* bInheritHandles      */ false,
            /* dwCreationFlags      */ EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT,
            /* lpEnvironment        */ nullptr,
            /* lpCurrentDirectory   */ nullptr,
            /* lpStartupInfo        */ &siEx.StartupInfo,
            /* lpProcessInformation */ &pi));
    }

    THROW_IF_FAILED(ConptyReleasePseudoConsole(hPC));

    // Forward Ctrl-C to the PTY.
    SetConsoleCtrlHandler(
        [](DWORD type) -> BOOL {
            switch (type)
            {
            case CTRL_C_EVENT:
            case CTRL_BREAK_EVENT:
                WriteFile(pipe.server.get(), "\x03", 1, nullptr, nullptr);
                return true;
            default:
                return false;
            }
        },
        TRUE);

    OVERLAPPED outputConptyOverlapped{ .hEvent = CreateEventW(nullptr, TRUE, TRUE, nullptr) };
    HANDLE handles[] = { inputHandle, outputConptyOverlapped.hEvent };
    INPUT_RECORD inputRecords[4096];
    char inputConptyBuffer[ARRAYSIZE(inputRecords)];
    char outputConptyBuffer[256 * 1024];

    // Kickstart the overlapped read of the pipe and cause the
    // outputConptyOverlapped members to be filled up appropriately.
    ReadFile(pipe.server.get(), &outputConptyBuffer[0], sizeof(outputConptyBuffer), nullptr, &outputConptyOverlapped);

    for (;;)
    {
        switch (WaitForMultipleObjectsEx(ARRAYSIZE(handles), &handles[0], FALSE, INFINITE, FALSE))
        {
        case WAIT_OBJECT_0 + 0:
        {
            DWORD read;
            if (!pReadConsoleInputExA(inputHandle, &inputRecords[0], ARRAYSIZE(inputRecords), &read, CONSOLE_READ_NOWAIT) || read == 0)
            {
                return 0;
            }

            DWORD write = 0;
            bool resize = false;

            for (DWORD i = 0; i < read; ++i)
            {
                switch (inputRecords[i].EventType)
                {
                case KEY_EVENT:
                    if (inputRecords[i].Event.KeyEvent.bKeyDown)
                    {
                        inputConptyBuffer[write++] = inputRecords[i].Event.KeyEvent.uChar.AsciiChar;
                    }
                    break;
                case WINDOW_BUFFER_SIZE_EVENT:
                    resize = true;
                    break;
                default:
                    break;
                }
            }

            if (resize)
            {
                const auto size = getViewportSize();
                if (memcmp(&viewportSize, &size, sizeof(COORD)) != 0)
                {
                    viewportSize = size;
                    THROW_IF_FAILED(ConptyResizePseudoConsole(hPC, viewportSize));
                }
            }

            if (write != 0)
            {
                DWORD written;
                if (!WriteFile(pipe.server.get(), &inputConptyBuffer[0], write, &written, nullptr) || written != write)
                {
                    return 0;
                }
            }
            break;
        }
        case WAIT_OBJECT_0 + 1:
        {
            DWORD read;
            if (!GetOverlappedResult(pipe.server.get(), &outputConptyOverlapped, &read, FALSE))
            {
                return 0;
            }

            do
            {
                DWORD written;
                if (debugOutput)
                {
                    WriteFile(debugOutput.get(), &outputConptyBuffer[0], read, &written, nullptr);
                }
                if (!WriteFile(outputHandle, &outputConptyBuffer[0], read, &written, nullptr) || written != read)
                {
                    return 0;
                }
            } while (ReadFile(pipe.server.get(), &outputConptyBuffer[0], sizeof(outputConptyBuffer), &read, &outputConptyOverlapped));

            if (GetLastError() != ERROR_IO_PENDING)
            {
                return 0;
            }
            break;
        }
        default:
            return 0;
        }
    }
}

int wmain(int argc, const wchar_t* argv[])
{
    const auto inputHandle = GetStdHandle(STD_INPUT_HANDLE);
    const auto outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    const auto previousInputCP = GetConsoleCP();
    const auto previousOutputCP = GetConsoleOutputCP();
    DWORD previousInputMode = 0;
    DWORD previousOutputMode = 0;
    GetConsoleMode(inputHandle, &previousInputMode);
    GetConsoleMode(outputHandle, &previousOutputMode);

    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleMode(inputHandle, ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT | ENABLE_QUICK_EDIT_MODE | ENABLE_EXTENDED_FLAGS | ENABLE_VIRTUAL_TERMINAL_INPUT);
    SetConsoleMode(outputHandle, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);

    int exitCode = 0;

    try
    {
        exitCode = run(argc, argv);
    }
    catch (const wil::ResultException& e)
    {
        printf("Error: %s\n", e.what());
        exitCode = e.GetErrorCode();
    }

    SetConsoleMode(outputHandle, previousOutputMode);
    SetConsoleCP(previousInputCP);
    SetConsoleOutputCP(previousOutputCP);

    return exitCode;
}
