// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <windows.h>
#include <wil\Common.h>
#include <wil\result.h>
#include <wil\resource.h>
#include <wil\wistd_functional.h>
#include <wil\wistd_memory.h>
#include <stdlib.h> /* srand, rand */
#include <time.h> /* time */

#include <deque>
#include <memory>
#include <vector>
#include <string>
#include <sstream>
#include <assert.h>

#include "VtConsole.hpp"

using namespace std;
////////////////////////////////////////////////////////////////////////////////
// "Do Unicode" strings - C-b, u, then one of these characters to emit a string
//      of characters that aren't really possible to read from the console currently.
const int TEST_LANG_NONE = 0; // 0
const int TEST_LANG_CYRILLIC = 1; // 1
const int TEST_LANG_CHINESE = 2; // 2
const int TEST_LANG_JAPANESE = 3; // 3
const int TEST_LANG_KOREAN = 4; // 4
const int TEST_LANG_GOOD_POUND = 5; // #
const int TEST_LANG_BAD_POUND = 6; // $

////////////////////////////////////////////////////////////////////////////////
// State
HANDLE hOut;
HANDLE hIn;
short lastTerminalWidth;
short lastTerminalHeight;

std::deque<VtConsole*> consoles;
// A console for printing debug output to
VtConsole* debug;

bool prefixPressed = false;
bool doUnicode = false;
int lang = TEST_LANG_NONE;

bool g_headless = false;
bool g_useConpty = false;
bool g_useOutfile = false;
std::wstring outfile = L"vtpt.out";
HANDLE hOutFile = INVALID_HANDLE_VALUE;
////////////////////////////////////////////////////////////////////////////////
// Forward decls
std::string toPrintableString(std::string& inString);
void toPrintableBuffer(char c, char* printBuffer, int* printCch);
std::string csi(string seq);
void PrintInputToDebug(std::string& rawInput);
void PrintOutputToDebug(std::string& rawOutput);
////////////////////////////////////////////////////////////////////////////////

void ReadCallback(BYTE* buffer, DWORD dwRead)
{
    // We already set the console to UTF-8 CP, so we can just write straight to it
    bool fSuccess = !!WriteFile(hOut, buffer, dwRead, nullptr, nullptr);
    if (fSuccess && g_useOutfile)
    {
        fSuccess = !!WriteFile(hOutFile, buffer, dwRead, nullptr, nullptr);
    }
    if (fSuccess)
    {
        std::string renderData = std::string((char*)buffer, dwRead);
        PrintOutputToDebug(renderData);
    }
    else
    {
        HRESULT hr = GetLastError();
        exit(hr);
    }
}

void DebugReadCallback(BYTE* /*buffer*/, DWORD /*dwRead*/)
{
    // do nothing.
}

VtConsole* getConsole()
{
    return consoles[0];
}

void nextConsole()
{
    auto con = consoles[0];
    con->deactivate();
    consoles.pop_front();
    consoles.push_back(con);
    con = consoles[0];
    con->activate();
    // Force the new console to repaint.
    std::string seq = csi("7t");
    con->WriteInput(seq);
}

HANDLE inPipe()
{
    return getConsole()->inPipe();
}

HANDLE outPipe()
{
    return getConsole()->outPipe();
}

void newConsole()
{
    auto con = new VtConsole(ReadCallback, g_headless, g_useConpty, { lastTerminalWidth, lastTerminalHeight });
    con->spawn();
    consoles.push_back(con);
}

void signalConsole()
{
    // The 0th console is always our active one.
    // This is a test-only scenario to set the window to 30 wide by 10 tall.
    consoles.at(0)->signalWindow(30, 10);
}

std::string csi(string seq)
{
    // Note: This doesn't do anything for the debug console currently.
    //      Somewhere, the TTY eats the control sequences. Still useful though.
    string fullSeq = "\x1b[";
    fullSeq += seq;
    return fullSeq;
}

void printKeyEvent(KEY_EVENT_RECORD keyEvent)
{
    // If printable:
    if (keyEvent.uChar.AsciiChar > ' ' && keyEvent.uChar.AsciiChar != '\x7f')
    {
        wprintf(L"Down: %d Repeat: %d KeyCode: 0x%x ScanCode: 0x%x Char: %c (0x%x) KeyState: 0x%x\r\n",
                keyEvent.bKeyDown,
                keyEvent.wRepeatCount,
                keyEvent.wVirtualKeyCode,
                keyEvent.wVirtualScanCode,
                keyEvent.uChar.AsciiChar,
                keyEvent.uChar.AsciiChar,
                keyEvent.dwControlKeyState);
    }
    else
    {
        wprintf(L"Down: %d Repeat: %d KeyCode: 0x%x ScanCode: 0x%x Char:(0x%x) KeyState: 0x%x\r\n",
                keyEvent.bKeyDown,
                keyEvent.wRepeatCount,
                keyEvent.wVirtualKeyCode,
                keyEvent.wVirtualScanCode,
                keyEvent.uChar.AsciiChar,
                keyEvent.dwControlKeyState);
    }
}

void toPrintableBuffer(char c, char* printBuffer, int* printCch)
{
    if (c == '\x1b')
    {
        printBuffer[0] = '^';
        printBuffer[1] = '[';
        *printCch = 2;
    }
    else if (c == '\x03')
    {
        printBuffer[0] = '^';
        printBuffer[1] = 'C';
        *printCch = 2;
    }
    else if (c == '\x0')
    {
        printBuffer[0] = '\\';
        printBuffer[1] = '0';
        *printCch = 2;
    }
    else if (c == '\r')
    {
        printBuffer[0] = '\\';
        printBuffer[1] = 'r';
        *printCch = 2;
    }
    else if (c == '\n')
    {
        printBuffer[0] = '\\';
        printBuffer[1] = 'n';
        *printCch = 2;
    }
    else if (c < '\x20')
    {
        printBuffer[0] = '^';
        printBuffer[1] = c + 0x40;
        *printCch = 2;
    }
    else if (c == '\x7f')
    {
        printBuffer[0] = '\\';
        printBuffer[1] = 'x';
        printBuffer[2] = '7';
        printBuffer[3] = 'f';
        *printCch = 4;
    }
    else
    {
        printBuffer[0] = (char)c;
        *printCch = 1;
    }
}

std::string toPrintableString(std::string& inString)
{
    std::string retval = "";
    for (size_t i = 0; i < inString.length(); i++)
    {
        char c = inString[i];
        if (c < '\x20')
        {
            retval += "^";
            char actual = (c + 0x40);
            retval += std::string(1, actual);
        }
        else if (c == '\x7f')
        {
            retval += "\\x7f";
        }
        else if (c == '\x20')
        {
            retval += "SPC";
        }
        else
        {
            retval += std::string(1, c);
        }
    }
    return retval;
}

void doResize(const unsigned short width, const unsigned short height)
{
    lastTerminalWidth = width;
    lastTerminalHeight = height;

    for (auto console : consoles)
    {
        console->Resize(height, width);
    }
}

void handleResize()
{
    CONSOLE_SCREEN_BUFFER_INFOEX csbiex = { 0 };
    csbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    bool fSuccess = !!GetConsoleScreenBufferInfoEx(hOut, &csbiex);
    if (fSuccess)
    {
        SMALL_RECT srViewport = csbiex.srWindow;

        unsigned short width = srViewport.Right - srViewport.Left + 1;
        unsigned short height = srViewport.Bottom - srViewport.Top + 1;

        doResize(width, height);
    }
}

void handleManyEvents(const INPUT_RECORD* const inputBuffer, int cEvents)
{
    char* const buffer = new char[cEvents];
    char* const printableBuffer = new char[cEvents * 4];
    memset(buffer, 0, cEvents);
    memset(printableBuffer, 0, cEvents * 4);

    char* nextBuffer = buffer;
    char* nextPrintable = printableBuffer;
    int bufferCch = 0;
    int printableCch = 0;

    for (int i = 0; i < cEvents; ++i)
    {
        INPUT_RECORD event = inputBuffer[i];

        if (event.EventType == KEY_EVENT)
        {
            KEY_EVENT_RECORD keyEvent = event.Event.KeyEvent;
            if (keyEvent.bKeyDown)
            {
                const char c = keyEvent.uChar.AsciiChar;

                if (c == '\0' && keyEvent.wVirtualScanCode != 0)
                {
                    // This is a special keyboard key that was pressed, not actually NUL
                    continue;
                }
                if (doUnicode)
                {
                    switch (c)
                    {
                    case '1':
                        lang = TEST_LANG_CYRILLIC;
                        break;
                    case '2':
                        lang = TEST_LANG_CHINESE;
                        break;
                    case '3':
                        lang = TEST_LANG_JAPANESE;
                        break;
                    case '4':
                        lang = TEST_LANG_KOREAN;
                        break;
                    case '#':
                        lang = TEST_LANG_GOOD_POUND;
                        break;
                    case '$':
                        lang = TEST_LANG_BAD_POUND;
                        break;
                    default:
                        doUnicode = false;
                        lang = TEST_LANG_NONE;
                        break;
                    }
                }
                else if (!prefixPressed)
                {
                    if (c == '\x2')
                    {
                        prefixPressed = true;
                    }
                    else
                    {
                        *nextBuffer = c;
                        nextBuffer++;
                        bufferCch++;
                    }
                }
                else
                {
                    switch (c)
                    {
                    case 'n':
                    case '\t':
                        nextConsole();
                        break;
                    case 't':
                        newConsole();
                        nextConsole();
                        break;
                    case 'u':
                        doUnicode = true;
                        break;
                    case 'r':
                        signalConsole();
                        break;
                    default:
                        *nextBuffer = c;
                        nextBuffer++;
                        bufferCch++;
                    }
                    prefixPressed = false;
                }
                int numPrintable = 0;
                toPrintableBuffer(c, nextPrintable, &numPrintable);
                nextPrintable += numPrintable;
                printableCch += numPrintable;
            }
        }
        else if (event.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            WINDOW_BUFFER_SIZE_RECORD resize = event.Event.WindowBufferSizeEvent;
            handleResize();
        }
    }

    if (bufferCch > 0)
    {
        std::string vtseq = std::string(buffer, bufferCch);
        std::string printSeq = std::string(printableBuffer, printableCch);

        getConsole()->WriteInput(vtseq);
        PrintInputToDebug(vtseq);
    }
    if (doUnicode && lang != TEST_LANG_NONE)
    {
        std::string str;
        switch (lang)
        {
        case TEST_LANG_CYRILLIC:
            str = "Лорем ипсум долор сит амет, пер цлита поссит ех, ат мунере фабулас петентиум сит.";
            break;
        case TEST_LANG_CHINESE:
            str = "側経意責家方家閉討店暖育田庁載社転線宇。";
            break;
        case TEST_LANG_JAPANESE:
            str = "旅ロ京青利セムレ弱改フヨス波府かばぼ意送でぼ調掲察たス日西重ケアナ住橋ユムミク順待ふかんぼ人奨貯鏡すびそ。";
            break;
        case TEST_LANG_KOREAN:
            str = "국민경제의 발전을 위한 중요정책의 수립에 관하여 대통령의 자문에 응하기 위하여 국민경제자문회의를 둘 수 있다.";
            break;
        case TEST_LANG_GOOD_POUND:
            str = "\xc2\xa3"; // UTF-8 £
            break;
        case TEST_LANG_BAD_POUND:
            str = "\xa3"; // UTF-16 £
            break;
        default:
            str = "";
            break;
        }
        getConsole()->WriteInput(str);
        PrintInputToDebug(str);

        doUnicode = false;
        lang = TEST_LANG_NONE;
    }
}

void PrintInputToDebug(std::string& rawInput)
{
    if (debug != nullptr)
    {
        std::string printable = toPrintableString(rawInput);
        std::stringstream ss;
        ss << "Input \"" << printable << "\" [" << rawInput.length() << "]\n";
        std::string output = ss.str();
        debug->WriteInput(output);
    }
}

void PrintOutputToDebug(std::string& rawOutput)
{
    if (debug != nullptr)
    {
        std::string printable = toPrintableString(rawOutput);
        std::stringstream ss;
        ss << printable << "\n";
        std::string output = ss.str();
        debug->WriteInput(output);
    }
}

void SetupOutput()
{
    DWORD dwMode = 0;
    THROW_LAST_ERROR_IF(!GetConsoleMode(hOut, &dwMode));
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    dwMode |= DISABLE_NEWLINE_AUTO_RETURN;
    THROW_LAST_ERROR_IF(!SetConsoleMode(hOut, dwMode));
}
void SetupInput()
{
    DWORD dwInMode = 0;
    GetConsoleMode(hIn, &dwInMode);
    dwInMode = ENABLE_VIRTUAL_TERMINAL_INPUT;
    SetConsoleMode(hIn, dwInMode);
}

DWORD WINAPI InputThread(LPVOID /*lpParameter*/)
{
    // Because the input thread ends up owning the lifetime of the application,
    // Set/restore the CP here.

    unsigned int launchOutputCP = GetConsoleOutputCP();
    unsigned int launchCP = GetConsoleCP();
    THROW_LAST_ERROR_IF(!SetConsoleOutputCP(CP_UTF8));
    THROW_LAST_ERROR_IF(!SetConsoleCP(CP_UTF8));
    auto restore = wil::scope_exit([&] {
        SetConsoleOutputCP(launchOutputCP);
        SetConsoleCP(launchCP);
    });

    for (;;)
    {
        INPUT_RECORD rc[256];
        DWORD dwRead = 0;
        // Not to future self: You can't read utf-8 from the console yet.
        bool fSuccess = !!ReadConsoleInput(hIn, rc, 256, &dwRead);
        if (fSuccess)
        {
            handleManyEvents(rc, dwRead);
        }
        else
        {
            exit(GetLastError());
        }
    }
}

void CreateIOThreads()
{
    // The VtConsoles themselves handle their output threads.

    DWORD dwInputThreadId = (DWORD)-1;
    HANDLE hInputThread = CreateThread(nullptr,
                                       0,
                                       InputThread,
                                       nullptr,
                                       0,
                                       &dwInputThreadId);
    hInputThread;
}

BOOL WINAPI CtrlHandler(DWORD fdwCtrlType)
{
    switch (fdwCtrlType)
    {
    // Handle the CTRL-C signal.
    case CTRL_C_EVENT:
    case CTRL_BREAK_EVENT:
        return true;
    }

    return false;
}

// this function has unreachable code due to its unusual lifetime. We
// disable the warning about it here.
#pragma warning(push)
#pragma warning(disable : 4702)
int __cdecl wmain(int argc, WCHAR* argv[])
{
    // initialize random seed:
    srand((unsigned int)time(NULL));
    SetConsoleCtrlHandler(CtrlHandler, TRUE);

    hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    hIn = GetStdHandle(STD_INPUT_HANDLE);

    bool fUseDebug = false;

    if (argc > 1)
    {
        for (int i = 0; i < argc; ++i)
        {
            std::wstring arg = argv[i];
            if (arg == std::wstring(L"--headless"))
            {
                g_headless = true;
            }
            if (arg == std::wstring(L"--conpty"))
            {
                g_useConpty = true;
            }
            else if (arg == std::wstring(L"--debug"))
            {
                fUseDebug = true;
            }
            else if (arg == std::wstring(L"--out") && i + 1 < argc)
            {
                g_useOutfile = true;
                outfile = argv[i + 1];
                i++;
            }
        }
    }

    if (g_useConpty)
    {
        printf("Launching vtpipeterm with conpty API...\n");
        Sleep(1000);
    }

    if (g_useOutfile)
    {
        hOutFile = CreateFileW(outfile.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hOutFile == INVALID_HANDLE_VALUE)
        {
            printf("Failed to open outfile (%ls) for writing\n", outfile.c_str());
            Sleep(1000);
            exit(0);
        }
    }

    SetupOutput();
    SetupInput();

    // handleResize will get our initial terminal dimensions.
    handleResize();

    newConsole();
    getConsole()->activate();
    CreateIOThreads();

    if (fUseDebug)
    {
        // Create a debug console for writting debugging output to.
        debug = new VtConsole(DebugReadCallback, false, false, { 80, 32 });
        // Echo stdin to stdout, but ignore newlines (so cat doesn't echo the input)
        // debug->spawn(L"ubuntu run tr -d '\n' | cat -sA");
        debug->spawn(L"wsl tr -d '\n' | cat -sA");
        debug->activate();
    }

    // Exit the thread so the CRT won't clean us up and kill. The IO thread owns the lifetime now.
    ExitThread(S_OK);
    // We won't hit this. The ExitThread above will kill the caller at this point.
    assert(false);
    return 0;
}
#pragma warning(pop)
