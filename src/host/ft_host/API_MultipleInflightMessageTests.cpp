// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

class MultipleInflightMessageTests
{
    BEGIN_TEST_CLASS(MultipleInflightMessageTests)
    END_TEST_CLASS()

    // This test is intended to make sure that we do not regress after the _handlePostCharInputLoop fix in OpenConsole:c0ab9cb5b
    TEST_METHOD(WriteWhileReadingInputCrash)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method") // Don't pollute other tests by isolating our potential crash and buffer resizing to this test.
        END_TEST_METHOD_PROPERTIES()

        using namespace std::string_view_literals;

        const auto inputHandle = GetStdInputHandle();
        const auto outputHandle = GetStdOutputHandle();

        DWORD originalConsoleMode{};
        VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(inputHandle, &originalConsoleMode));
        auto restoreMode{ wil::scope_exit([=]() {
            SetConsoleMode(inputHandle, originalConsoleMode);
        }) };
        VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(inputHandle, ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

        // Prime the console with some fake input records.
        const std::array<INPUT_RECORD, 4> inputRecords{
            INPUT_RECORD{ KEY_EVENT, KEY_EVENT_RECORD{ TRUE, 1, 'h', 0, L'h', 0 } },
            INPUT_RECORD{ KEY_EVENT, KEY_EVENT_RECORD{ FALSE, 1, 'h', 0, L'h', 0 } },
            INPUT_RECORD{ KEY_EVENT, KEY_EVENT_RECORD{ TRUE, 1, 'i', 0, L'i', 0 } },
            INPUT_RECORD{ KEY_EVENT, KEY_EVENT_RECORD{ FALSE, 1, 'i', 0, L'i', 0 } },
        };
        DWORD written{};
        VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInput(inputHandle, inputRecords.data(), static_cast<DWORD>(inputRecords.size()), &written));
        VERIFY_ARE_EQUAL(inputRecords.size(), written);

        // !!! LOAD BEARING !!!
        // This buffer needs to be large enough to force API_MSG to heap allocate (!)
        std::array<wchar_t, 129> buffer;
        DWORD read{};

        bool readerThreadLaunched{ false };
        std::condition_variable readerThreadLaunchCV;
        std::mutex readerThreadLaunchMutex;

        std::unique_lock lock{ readerThreadLaunchMutex };

        std::thread readerThread{ [&]() {
            WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
            {
                std::scoped_lock lock{ readerThreadLaunchMutex };
                readerThreadLaunched = true;
            }
            readerThreadLaunchCV.notify_all();
            VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleW(inputHandle, buffer.data(), static_cast<DWORD>(buffer.size()), &read, nullptr));
        } };

        // CV wait should allow readerThread to progress
        readerThreadLaunchCV.wait(lock, [&]() { return readerThreadLaunched; });
        // should not progress until readerThreadLaunched is set.
        Sleep(50); // Yeah, it's not science.

        std::thread writerThread{ [&]() {
            WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
            DWORD temp{};
            // !!! LOAD BEARING !!!
            // This buffer must be large enough to trigger a *re-allocation* in the API message handler.
            std::array<wchar_t, 4096> anEvenLargerBuffer;
            VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterW(outputHandle, anEvenLargerBuffer.data(), static_cast<DWORD>(anEvenLargerBuffer.size()), COORD{ 1, 1 }, &temp)); // has payload (output buffer)
            VERIFY_ARE_EQUAL(4096u, temp);

            const std::array<INPUT_RECORD, 2> inputRecords{
                INPUT_RECORD{ KEY_EVENT, KEY_EVENT_RECORD{ TRUE, 1, '1', 0, L'!', SHIFT_PRESSED } },
                INPUT_RECORD{ KEY_EVENT, KEY_EVENT_RECORD{ FALSE, 1, '1', 0, L'!', SHIFT_PRESSED } },
            };
            DWORD written{};
            VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(inputHandle, inputRecords.data(), static_cast<DWORD>(inputRecords.size()), &written));
            VERIFY_ARE_EQUAL(inputRecords.size(), written);

            // !!! LOAD BEARING !!!
            // It is (apparently) important that this come in two different writes.

            const std::array<INPUT_RECORD, 2> inputRecords2{
                INPUT_RECORD{ KEY_EVENT, KEY_EVENT_RECORD{ TRUE, 1, VK_RETURN, 0, L'\r', 0 } },
                INPUT_RECORD{ KEY_EVENT, KEY_EVENT_RECORD{ FALSE, 1, VK_RETURN, 0, L'\r', 0 } },
            };
            VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(inputHandle, inputRecords2.data(), static_cast<DWORD>(inputRecords2.size()), &written));
            VERIFY_ARE_EQUAL(inputRecords2.size(), written);
        } };

        writerThread.join();
        readerThread.join();

        VERIFY_ARE_EQUAL(L"hi!\r"sv, (std::wstring_view{ buffer.data(), read }));
    }
};
