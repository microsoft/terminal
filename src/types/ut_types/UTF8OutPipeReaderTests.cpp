// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "..\inc\UTF8OutPipeReader.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

class UTF8OutPipeReaderTests
{
    TEST_CLASS(UTF8OutPipeReaderTests);

    TEST_METHOD(TestUtf8MergePartials)
    {
        // The test uses the base character 'a' + 'COMBINING DIAERESIS' (U+0308) as an example for a composite character
        // Their UTF-8 representation consists of three bytes:
        //   a    b    c
        // 0x61 0xCC 0x88
        //
        // And the character 'GOTHIC LETTER HWAIR' (U+10348) as an example for a UTF-8 multi-byte character
        // Its UTF-8 representation consists of four bytes:
        //   1    2    3    4
        // 0xF0 0x90 0x8D 0x88
        //
        // For the test a std::string is filled with 4108 '.' characters to make sure it exceeds the
        //  buffer size of 4096 bytes in UTF8OutPipeReader.
        //
        // This figure shows how the string is getting changed for the 10 sub-tests. The letters a to c
        //  represent the bytes of the base + combining characters. The digits 1 to 4 represent the four
        //  bytes of the 'Hwair' letter. The vertical bar represents the buffer boundary. The horizontal
        //  bars represent the expected boundaries of populated UTF-8 chunks.
        // Test  1: [more points] . . P a b c 1 2 3 4-Q|R S T U V W X Y Z . .-.
        // Test  2: [more points] . . P Q a b c-1 2 3 4|R S T U V W X Y Z . .-.
        // Test  3: [more points] . . P Q R a b c-1 2 3|4 S T U V W X Y Z . .-.
        // Test  4: [more points] . . P Q R S a b c-1 2|3 4 T U V W X Y Z . .-.
        // Test  5: [more points] . . P Q R S T a b c-1|2 3 4 U V W X Y Z . .-.
        // Test  6: [more points] . . P Q R S T U a b c+1 2 3 4 V W X Y Z . .-.
        // Test  7: [more points] . . P Q R S T U V-a b|c 1 2 3 4 W X Y Z . .-.
        // Test  8: [more points] . . P Q R S T U V W-a|b c 1 2 3 4 X Y Z . .-.
        // Test  9: [more points] . . P Q R S T U V W-X|a b c 1 2 3 4 Y Z . .-.
        // Test 10: [more points] . . P Q R S T U V W-X|Y a b c 1 2 3 4 Z . .-.
        //
        // At the beginning of a test the whole string is converted into a std::wstring containing the
        //  UTF-16 target for reference.
        // During the test a second wstring is concatenated out of the chunks that we get from
        //  UTF8OutPipeReader::Read(). Each chunk is separately converted to wstring in order to
        //  make sure it would be corrupted if we get UTF-8 partials or split composite characters.
        // The test is positive if both wstrings are equal.

        const size_t bufferSize{ 4096 }; // NOTE: This has to match the buffer size in UTF8OutPipeReader!
        std::string utf8TestString(bufferSize + 12, '.'); // create a test string with the required size

        // Test 1:
        //                                                                       ||
        utf8TestString.replace(bufferSize - 9, 18, "P\x61\xCC\x88\xF0\x90\x8D\x88QRSTUVWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 2:
        //                                                                      | |
        utf8TestString.replace(bufferSize - 9, 18, "PQ\x61\xCC\x88\xF0\x90\x8D\x88RSTUVWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 3:
        //                                                                   |   |
        utf8TestString.replace(bufferSize - 9, 18, "PQR\x61\xCC\x88\xF0\x90\x8D\x88STUVWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 4:
        //                                                                |   |
        utf8TestString.replace(bufferSize - 9, 18, "PQRS\x61\xCC\x88\xF0\x90\x8D\x88TUVWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 5:
        //                                                             |   |
        utf8TestString.replace(bufferSize - 9, 18, "PQRST\x61\xCC\x88\xF0\x90\x8D\x88UVWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 6:
        //                                                          |   |
        utf8TestString.replace(bufferSize - 9, 18, "PQRSTU\x61\xCC\x88\xF0\x90\x8D\x88VWXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 7:
        //                                                       |   |
        utf8TestString.replace(bufferSize - 9, 18, "PQRSTUV\x61\xCC\x88\xF0\x90\x8D\x88WXYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 8:
        //                                                    |   |
        utf8TestString.replace(bufferSize - 9, 18, "PQRSTUVW\x61\xCC\x88\xF0\x90\x8D\x88XYZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 9:
        //                                                  |  |
        utf8TestString.replace(bufferSize - 9, 18, "PQRSTUVWX\x61\xCC\x88\xF0\x90\x8D\x88YZ");
        VERIFY_SUCCEEDED(RunTest(utf8TestString));

        // Test 10:
        //                                                  ||
        utf8TestString.replace(bufferSize - 9, 18, "PQRSTUVWXY\x61\xCC\x88\xF0\x90\x8D\x88Z");
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
        wil::unique_hfile outPipe{};
        wil::unique_hfile inPipe{};

        SECURITY_ATTRIBUTES sa{ sizeof(SECURITY_ATTRIBUTES) };
        CreatePipe(&outPipe, &inPipe, &sa, 0); // create the pipe handles

        UTF8OutPipeReader reader{ outPipe.get() };

        std::wstring_view utf16StrView{};
        THROW_IF_FAILED(reader._UTF8ToUtf16Precomposed(utf8TestString, utf16StrView));
        const std::wstring utf16Expected{ utf16StrView }; // contains the whole string converted to UTF-16
        std::wstring utf16Actual{}; // will be concatenated from the converted chunks

        ThreadData data{ inPipe, utf8TestString };
        wil::unique_handle threadHandle{ CreateThread(nullptr, 0, WritePipeThread, &data, 0, nullptr) }; // create a thread that writes to the pipe
        RETURN_HR_IF_NULL(E_FAIL, threadHandle.get());

        // process the chunks that we get from UTF8OutPipeReader::GetUtf16Precomposed
        while (true)
        {
            // get a chunk of data
            HRESULT hr = reader.Read(utf16StrView);
            THROW_IF_FAILED(hr);

            if (utf16StrView.empty() || hr == S_FALSE)
            {
                // this is okay, no data was left or the pipe was closed
                break;
            }

            // append the chunk to the resulting wstring
            utf16Actual += utf16StrView;
        }

        WaitForSingleObject(threadHandle.get(), 2000);

        // the test passed if both wstrings are equal
        if (utf16Actual == utf16Expected)
        {
            return S_OK;
        }

        return E_FAIL;
    }
};
