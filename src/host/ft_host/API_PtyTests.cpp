// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using WEX::Logging::Log;
using WEX::TestExecution::TestData;
using namespace WEX::Common;

class PtyTests
{
    BEGIN_TEST_CLASS(PtyTests)
    END_TEST_CLASS()

    HRESULT SpawnClient(HPCON hPC, wil::unique_process_information& pi)
    {
        HRESULT hr = E_FAIL;
        STARTUPINFOEXW startupInfo{};
        if (S_OK == (hr = InitializeStartupInfoAttachedToPseudoConsole(&startupInfo, hPC)))
        {
            // Launch ping to emit some text back via the pipe
            auto szCommand = _wcsdup(L"cmd.exe");

            hr = CreateProcessW(
                     NULL,
                     szCommand,
                     NULL,
                     NULL,
                     FALSE,
                     EXTENDED_STARTUPINFO_PRESENT,
                     NULL,
                     NULL,
                     &startupInfo.StartupInfo,
                     &pi) ?
                     S_OK :
                     GetLastError();

            free(szCommand);
        }
        return hr;
    }

    struct Baton
    {
        HPCON hPC;
        HANDLE ev;
        DWORD closeMethod;
        HANDLE inputWriter;
        HANDLE outputReader;
    };

    HRESULT RunTest(bool inherit, bool read, bool write, DWORD endSessionBy)
    {
        DWORD mode;
        GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode);
        mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), mode);
        HANDLE h1i, h1o, h2i, h2o;
        auto f = !!CreatePipe(&h1o, &h1i, nullptr, 0);
        if (!f)
        {
            fprintf(stderr, "Beefed it at pipe 1\n");
            return 1;
        }
        f = !!CreatePipe(&h2o, &h2i, nullptr, 0);
        if (!f)
        {
            fprintf(stderr, "Beefed it at pipe 2\n");
            return 1;
        }

        DWORD dwFlags = 0;
        if (inherit)
        {
            dwFlags = PSEUDOCONSOLE_INHERIT_CURSOR;
        }

        HPCON hPC;
        auto hr = CreatePseudoConsole({ 80, 25 }, h1o, h2i, dwFlags, &hPC);
        if (hr != S_OK)
        {
            fprintf(stderr, "Failed: %8.08x\n", hr);
            return 1;
        }
        CloseHandle(h1o);
        CloseHandle(h2i);

        if (write)
        {
            const char* buffer = "\x1b[0;0R";

            DWORD dwWritten = 0;

            WriteFile(h1i, buffer, 6, &dwWritten, nullptr);

            const auto glewrite = GetLastError();
        }

        wil::unique_process_information pi;

        hr = SpawnClient(hPC, pi);

        if (read)
        {
            byte bufferOut[256];
            DWORD dwRead = 0;
            ReadFile(h2o, &bufferOut, ARRAYSIZE(bufferOut), &dwRead, nullptr);

            const auto gleread = GetLastError();
        }

        if (hr != S_OK)
        {
            fprintf(stderr, "Spawn took a trip to beeftown: %8.08x\n", hr);
            return 1;
        }

        fprintf(stderr, "Letting CMD actually spawn?\n");
        Sleep(1000); // Let it settle?

        Baton b{ hPC, nullptr };
        b.ev = CreateEventW(nullptr, TRUE, FALSE, L"It is an event");
        b.closeMethod = endSessionBy;
        b.inputWriter = h1i;
        b.outputReader = h2o;

        CreateThread(
            nullptr, 0, [](LPVOID c) -> DWORD {
                Baton& b = *reinterpret_cast<Baton*>(c);
                fprintf(stderr, "Closing?\n");

                switch (b.closeMethod)
                {
                case 0:
                    ClosePseudoConsole(b.hPC);
                    break;
                case 1:
                    CloseHandle(b.inputWriter);
                    break;
                case 2:
                    CloseHandle(b.outputReader);
                    break;
                }

                SetEvent(b.ev);
                return 0;
            },
            (LPVOID)&b,
            0,
            nullptr);

        auto r = WaitForSingleObject(b.ev, 5000);
        switch (r)
        {
        case WAIT_OBJECT_0:
            Log::Comment(L"Hey look it works.");
            break;
        case WAIT_TIMEOUT:
            Log::Comment(L"\x1b[4;1;31mYOU DEADLOCKED IT\x1b[m\n");
            return HRESULT_FROM_WIN32(WAIT_TIMEOUT);
        case WAIT_FAILED:
        {
            auto gle = GetLastError();
            Log::Comment(String().Format(L"You somehow broke it even worse (GLE=%d)", gle));
            return HRESULT_FROM_WIN32(gle);
        }
        }
        return S_OK;
    }

    // Initializes the specified startup info struct with the required properties and
    // updates its thread attribute list with the specified ConPTY handle
    HRESULT InitializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEXW* pStartupInfo, HPCON hPC)
    {
        HRESULT hr{ E_UNEXPECTED };

        if (pStartupInfo)
        {
            size_t attrListSize{};

            pStartupInfo->StartupInfo.cb = sizeof(STARTUPINFOEXW);

            // Get the size of the thread attribute list.
            InitializeProcThreadAttributeList(NULL, 1, 0, &attrListSize);

            // Allocate a thread attribute list of the correct size
            pStartupInfo->lpAttributeList =
                reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));

            // Initialize thread attribute list
            if (pStartupInfo->lpAttributeList && InitializeProcThreadAttributeList(pStartupInfo->lpAttributeList, 1, 0, &attrListSize))
            {
                // Set Pseudo Console attribute
                hr = UpdateProcThreadAttribute(
                         pStartupInfo->lpAttributeList,
                         0,
                         PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
                         hPC,
                         sizeof(HPCON),
                         NULL,
                         NULL) ?
                         S_OK :
                         HRESULT_FROM_WIN32(GetLastError());
            }
            else
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
            }
        }
        return hr;
    }

    TEST_METHOD(PtyInitAndShutdown)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:inheritCursor", L"{true, false}")
            TEST_METHOD_PROPERTY(L"Data:readOutput", L"{true, false}")
            TEST_METHOD_PROPERTY(L"Data:writeInput", L"{true, false}")
            TEST_METHOD_PROPERTY(L"Data:endSessionBy", L"{0, 1, 2}")
        END_TEST_METHOD_PROPERTIES()

        bool inheritCursor;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"inheritCursor", inheritCursor));

        bool readOutput;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"readOutput", readOutput));

        bool writeInput;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"writeInput", writeInput));

        DWORD endSessionBy;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"endSessionBy", endSessionBy));

        VERIFY_SUCCEEDED(RunTest(inheritCursor, readOutput, writeInput, endSessionBy));
    }
};
