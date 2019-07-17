// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "..\inc\UTF8OutPipeReader.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class UTF8OutPipeReaderTests
{
    TEST_CLASS(UTF8OutPipeReaderTests);

    TEST_METHOD(TestUtf8MergePartials)
    {
        // The test uses the character 'GOTHIC LETTER HWAIR' (U+10348) as an example
        // Its UTF-8 representation consists of four bytes:
        //   1    2    3    4
        // 0xF0 0x90 0x8D 0x88
        //
        // For the test a std::string is filled with 4104 '.' characters to make sure it exceeds the
        //  buffer size of 4096 bytes in UTF8OutPipeReader.
        //
        // This figure shows how the string is getting changed for the 7 sub-tests. The digits 1 to 4
        //  represent the four bytes of the 'Hwair' letter. The vertical bar represents the buffer boundary.
        // Test 1: [more points] . . S 1 2 3 4 T|U V W X Y Z . .
        // Test 2: [more points] . . S T 1 2 3 4|U V W X Y Z . .
        // Test 3: [more points] . . S T U 1 2 3|4 V W X Y Z . .
        // Test 4: [more points] . . S T U V 1 2|3 4 W X Y Z . .
        // Test 5: [more points] . . S T U V W 1|2 3 4 X Y Z . .
        // Test 6: [more points] . . S T U V W X|1 2 3 4 Y Z . .
        // Test 7: [more points] . . S T U V W X|Y 1 2 3 4 Z . .
        //
        // Tests 1, 6, and 7 prove proper ASCII handling.
        // Test 2 leaves all four bytes of 'Hwair' in the first chunk.
        // Test 3, 4, and 5 move the partials from the end of the first chunk to the begin of the
        //  second chunk.
        //
        // At the beginning of a test the whole string is converted into a winrt::hstring for reference.
        // During the test a second hstring is concatenated out of the chunks that we get from
        //  UTF8OutPipeReader::Read. Each chunk is separately converted to hstring in order to make
        //  sure it would be corrupted if we get UTF-8 partials.
        // The test is positive if both hstrings are equal.

        const size_t bufferSize{ 4096 }; // NOTE: This has to match the buffer size in UTF8OutPipeReader!
        std::string utf8TestString(bufferSize + 8, '.'); // create a test string with the required size

        // Test 1:
        //                                                           ||
        utf8TestString.replace(bufferSize - 6, 12, "S\xF0\x90\x8D\x88TUVWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 2:
        //                                                          | |
        utf8TestString.replace(bufferSize - 6, 12, "ST\xF0\x90\x8D\x88UVWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 3:
        //                                                       |   |
        utf8TestString.replace(bufferSize - 6, 12, "STU\xF0\x90\x8D\x88VWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 4:
        //                                                    |   |
        utf8TestString.replace(bufferSize - 6, 12, "STUV\xF0\x90\x8D\x88WXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 5:
        //                                                 |   |
        utf8TestString.replace(bufferSize - 6, 12, "STUVW\xF0\x90\x8D\x88XYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 6:
        //                                               |  |
        utf8TestString.replace(bufferSize - 6, 12, "STUVWX\xF0\x90\x8D\x88YZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 7:
        //                                               ||
        utf8TestString.replace(bufferSize - 6, 12, "STUVWXY\xF0\x90\x8D\x88Z");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));
    }

    struct ThreadData
    {
        wil::unique_hfile& inPipe;
        std::string& utf8TestString;
    };

    // Thread function which writes the UTF-8 data to the pipe.
    static DWORD WINAPI WritePipeThread(LPVOID threadArg)
    {
        ThreadData* pThreadData{ reinterpret_cast<ThreadData*>(threadArg) };
        DWORD length{};

        WriteFile(pThreadData->inPipe.get(), pThreadData->utf8TestString.c_str(), static_cast<DWORD>(pThreadData->utf8TestString.size()), &length, nullptr);
        pThreadData->inPipe.reset();

        return 0;
    }

    // Performs the sub-tests.
    HRESULT RunTest(std::string& utf8TestString)
    {
        std::string_view strView{}; // contains the chunk that we get from UTF8OutPipeReader::Read
        const winrt::hstring utf16Expected{ winrt::to_hstring(utf8TestString) }; // contains the whole string converted to UTF-16
        winrt::hstring utf16Actual{}; // will be concatenated from the converted chunks

        wil::unique_hfile outPipe{};
        wil::unique_hfile inPipe{};

        SECURITY_ATTRIBUTES sa{ sizeof(SECURITY_ATTRIBUTES) };
        CreatePipe(&outPipe, &inPipe, &sa, 0); // create the pipe handles

        UTF8OutPipeReader reader{ outPipe.get() };

        ThreadData data{ inPipe, utf8TestString };

        wil::unique_handle threadHandle{ CreateThread(nullptr, 0, WritePipeThread, &data, 0, nullptr) }; // create a thread that writes to the pipe
        RETURN_HR_IF_NULL(E_FAIL, threadHandle.get());

        // process the chunks that we get from UTF8OutPipeReader::Read
        while (true)
        {
            // get a chunk of UTF-8 data
            THROW_IF_FAILED(reader.Read(strView));

            if (strView.empty())
            {
                // this is okay, no data left in the pipe
                break;
            }

            // convert the chunk to hstring and append it to the resulting hstring
            utf16Actual = utf16Actual + winrt::to_hstring(strView);
        }

        WaitForSingleObject(threadHandle.get(), 2000);

        // the test passed if both hstrings are equal
        if (utf16Actual == utf16Expected)
        {
            return S_OK;
        }

        return E_FAIL;
    }
};
