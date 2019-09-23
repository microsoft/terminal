// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

using WEX::Logging::Log;
using namespace WEX::Common;

// This class is intended to test:
// GetConsoleTitle
class TitleTests
{
    BEGIN_TEST_CLASS(TitleTests)
    END_TEST_CLASS()

    TEST_METHOD_SETUP(TestSetup);
    TEST_METHOD_CLEANUP(TestCleanup);

    TEST_METHOD(TestGetConsoleTitleA);
    TEST_METHOD(TestGetConsoleTitleW);
};

bool TitleTests::TestSetup()
{
    return Common::TestBufferSetup();
}

bool TitleTests::TestCleanup()
{
    return Common::TestBufferCleanup();
}

void TestGetConsoleTitleAFillHelper(_Out_writes_all_(cchBuffer) char* const chBuffer,
                                    const size_t cchBuffer,
                                    const char chFill)
{
    for (size_t i = 0; i < cchBuffer; i++)
    {
        chBuffer[i] = chFill;
    }
}

void TestGetConsoleTitleWFillHelper(_Out_writes_all_(cchBuffer) wchar_t* const wchBuffer,
                                    const size_t cchBuffer,
                                    const wchar_t wchFill)
{
    for (size_t i = 0; i < cchBuffer; i++)
    {
        wchBuffer[i] = wchFill;
    }
}

void TestGetConsoleTitleAPrepExpectedHelper(_In_reads_(cchTitle) const char* const chTitle,
                                            const size_t cchTitle,
                                            _Inout_updates_all_(cchReadBuffer) char* const chReadBuffer,
                                            const size_t cchReadBuffer,
                                            _Inout_updates_all_(cchReadExpected) char* const chReadExpected,
                                            const size_t cchReadExpected,
                                            const size_t cchTryToRead)
{
    // Fill our read buffer and expected with all Zs to start
    TestGetConsoleTitleAFillHelper(chReadBuffer, cchReadBuffer, 'Z');
    TestGetConsoleTitleAFillHelper(chReadExpected, cchReadExpected, 'Z');

    // Prep expected data
    if (cchTryToRead >= cchTitle - 1)
    {
        VERIFY_SUCCEEDED(StringCchCopyNA(chReadExpected, cchReadExpected, chTitle, cchTryToRead)); // Copy as much room as we said we had leaving space for null terminator

        if (cchTryToRead == cchTitle - 1)
        {
            chReadExpected[cchTryToRead] = 'Z';
        }
    }
    else
    {
        chReadExpected[0] = '\0';
    }
}

void TestGetConsoleTitleWPrepExpectedHelper(_In_reads_(cchTitle) const wchar_t* const wchTitle,
                                            const size_t cchTitle,
                                            _Inout_updates_all_(cchReadBuffer) wchar_t* const wchReadBuffer,
                                            const size_t cchReadBuffer,
                                            _Inout_updates_all_(cchReadExpected) wchar_t* const wchReadExpected,
                                            const size_t cchReadExpected,
                                            const size_t cchTryToRead)
{
    // Fill our read buffer and expected with all Zs to start
    TestGetConsoleTitleWFillHelper(wchReadBuffer, cchReadBuffer, L'Z');
    TestGetConsoleTitleWFillHelper(wchReadExpected, cchReadExpected, L'Z');

    // Prep expected data
    size_t const cchCopy = min(cchTitle, cchTryToRead);
    VERIFY_SUCCEEDED(StringCchCopyNW(wchReadExpected, cchReadBuffer, wchTitle, cchCopy - 1)); // Copy as much room as we said we had leaving space for null terminator
}

void TestGetConsoleTitleAVerifyHelper(_Inout_updates_(cchReadBuffer) char* const chReadBuffer,
                                      const size_t cchReadBuffer,
                                      const size_t cchTryToRead,
                                      const DWORD dwExpectedRetVal,
                                      const DWORD dwExpectedLastError,
                                      _In_reads_(cchExpected) const char* const chReadExpected,
                                      const size_t cchExpected)
{
    VERIFY_ARE_EQUAL(cchExpected, cchReadBuffer);

    SetLastError(0);
    DWORD const dwRetVal = GetConsoleTitleA(chReadBuffer, (DWORD)cchTryToRead);
    DWORD const dwLastError = GetLastError();

    VERIFY_ARE_EQUAL(dwExpectedRetVal, dwRetVal);
    VERIFY_ARE_EQUAL(dwExpectedLastError, dwLastError);

    if (chReadExpected != nullptr)
    {
        for (size_t i = 0; i < cchExpected; i++)
        {
            wchar_t const wchExpectedVis = chReadExpected[i] < 0x30 ? (wchar_t)chReadExpected[i] + 0x2400 : chReadExpected[i];
            wchar_t const wchBufferVis = chReadBuffer[i] < 0x30 ? (wchar_t)chReadBuffer[i] + 0x2400 : chReadBuffer[i];

            // We must verify every individual character, not as a string, because we might be expecting a null
            // in the middle and need to verify it then keep going and read what's past that.
            VERIFY_ARE_EQUAL(chReadExpected[i], chReadBuffer[i], NoThrowString().Format(L"%c (0x%02x) == %c (0x%02x)", wchExpectedVis, chReadExpected[i], wchBufferVis, chReadBuffer[i]));
        }
    }
    else
    {
        VERIFY_ARE_EQUAL(chReadExpected, chReadBuffer);
        VERIFY_ARE_EQUAL(0u, cchTryToRead);
    }
}

void TestGetConsoleTitleWVerifyHelper(_Inout_updates_(cchReadBuffer) wchar_t* const wchReadBuffer,
                                      const size_t cchReadBuffer,
                                      const size_t cchTryToRead,
                                      const DWORD dwExpectedRetVal,
                                      const DWORD dwExpectedLastError,
                                      _In_reads_(cchExpected) const wchar_t* const wchReadExpected,
                                      const size_t cchExpected)
{
    VERIFY_ARE_EQUAL(cchExpected, cchReadBuffer);

    SetLastError(0);
    DWORD const dwRetVal = GetConsoleTitleW(wchReadBuffer, (DWORD)cchTryToRead);
    DWORD const dwLastError = GetLastError();

    VERIFY_ARE_EQUAL(dwExpectedRetVal, dwRetVal);
    VERIFY_ARE_EQUAL(dwExpectedLastError, dwLastError);

    if (wchReadExpected != nullptr)
    {
        for (size_t i = 0; i < cchExpected; i++)
        {
            wchar_t const wchExpectedVis = wchReadExpected[i] < 0x30 ? wchReadExpected[i] + 0x2400 : wchReadExpected[i];
            wchar_t const wchBufferVis = wchReadBuffer[i] < 0x30 ? wchReadBuffer[i] + 0x2400 : wchReadBuffer[i];

            // We must verify every individual character, not as a string, because we might be expecting a null
            // in the middle and need to verify it then keep going and read what's past that.
            VERIFY_ARE_EQUAL(wchReadExpected[i], wchReadBuffer[i], NoThrowString().Format(L"%c (0x%02x) == %c (0x%02x)", wchExpectedVis, wchReadExpected[i], wchBufferVis, wchReadBuffer[i]));
        }
    }
    else
    {
        VERIFY_ARE_EQUAL(wchReadExpected, wchReadBuffer);
        VERIFY_ARE_EQUAL(0u, cchTryToRead);
    }
}

void TitleTests::TestGetConsoleTitleA()
{
    const char* const szTestTitle = "TestTitle";
    size_t const cchTestTitle = strlen(szTestTitle);

    Log::Comment(NoThrowString().Format(L"Set up the initial console title to '%S'.", szTestTitle));
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(SetConsoleTitleA(szTestTitle));

    size_t cchReadBuffer = cchTestTitle + 1 + 4; // string length + null terminator + 4 bonus spots to check overruns/extra length.
    wistd::unique_ptr<char[]> chReadBuffer = wil::make_unique_nothrow<char[]>(cchReadBuffer);
    VERIFY_IS_NOT_NULL(chReadBuffer.get());

    wistd::unique_ptr<char[]> chReadExpected = wil::make_unique_nothrow<char[]>(cchReadBuffer);
    VERIFY_IS_NOT_NULL(chReadExpected.get());

    size_t cchTryToRead = 0;

    Log::Comment(L"Test 1: Say we have half the buffer size necessary.");
    cchTryToRead = cchTestTitle / 2;

    // Prepare the buffers and expected data
    TestGetConsoleTitleAPrepExpectedHelper(szTestTitle,
                                           cchTestTitle + 1,
                                           chReadBuffer.get(),
                                           cchReadBuffer,
                                           chReadExpected.get(),
                                           cchReadBuffer,
                                           cchTryToRead);

    // Run the call and test it out.
    TestGetConsoleTitleAVerifyHelper(chReadBuffer.get(), cchReadBuffer, cchTryToRead, 0, S_OK, chReadExpected.get(), cchReadBuffer);

    Log::Comment(L"Test 2: Say we have have exactly the string length with no null space.");
    cchTryToRead = cchTestTitle;

    // Prepare the buffers and expected data
    TestGetConsoleTitleAPrepExpectedHelper(szTestTitle,
                                           cchTestTitle + 1,
                                           chReadBuffer.get(),
                                           cchReadBuffer,
                                           chReadExpected.get(),
                                           cchReadBuffer,
                                           cchTryToRead);

    // Run the call and test it out.
    TestGetConsoleTitleAVerifyHelper(chReadBuffer.get(), cchReadBuffer, cchTryToRead, (DWORD)cchTestTitle, S_OK, chReadExpected.get(), cchReadBuffer);

    Log::Comment(L"Test 3: Say we have have the string length plus one null space.");
    cchTryToRead = cchTestTitle + 1;

    // Prepare the buffers and expected data
    TestGetConsoleTitleAPrepExpectedHelper(szTestTitle,
                                           cchTestTitle + 1,
                                           chReadBuffer.get(),
                                           cchReadBuffer,
                                           chReadExpected.get(),
                                           cchReadBuffer,
                                           cchTryToRead);

    // Run the call and test it out.
    TestGetConsoleTitleAVerifyHelper(chReadBuffer.get(), cchReadBuffer, cchTryToRead, (DWORD)cchTestTitle, S_OK, chReadExpected.get(), cchReadBuffer);

    Log::Comment(L"Test 4: Say we have the string length with a null space and an extra space.");
    cchTryToRead = cchTestTitle + 1 + 1;

    // Prepare the buffers and expected data
    TestGetConsoleTitleAPrepExpectedHelper(szTestTitle,
                                           cchTestTitle + 1,
                                           chReadBuffer.get(),
                                           cchReadBuffer,
                                           chReadExpected.get(),
                                           cchReadBuffer,
                                           cchTryToRead);

    // Run the call and test it out.
    TestGetConsoleTitleAVerifyHelper(chReadBuffer.get(), cchReadBuffer, cchTryToRead, (DWORD)cchTestTitle, S_OK, chReadExpected.get(), cchReadBuffer);

    Log::Comment(L"Test 5: Say we have no buffer.");
    cchTryToRead = cchTestTitle;

    // Run the call and test it out.
    TestGetConsoleTitleAVerifyHelper(nullptr, 0, 0, 0, S_OK, nullptr, 0);
}

void TitleTests::TestGetConsoleTitleW()
{
    const wchar_t* const wszTestTitle = L"TestTitle";
    size_t const cchTestTitle = wcslen(wszTestTitle);

    Log::Comment(NoThrowString().Format(L"Set up the initial console title to '%s'.", wszTestTitle));
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(SetConsoleTitleW(wszTestTitle));

    size_t cchReadBuffer = cchTestTitle + 1 + 4; // string length + null terminator + 4 bonus spots to check overruns/extra length.
    wistd::unique_ptr<wchar_t[]> wchReadBuffer = wil::make_unique_nothrow<wchar_t[]>(cchReadBuffer);
    VERIFY_IS_NOT_NULL(wchReadBuffer.get());

    wistd::unique_ptr<wchar_t[]> wchReadExpected = wil::make_unique_nothrow<wchar_t[]>(cchReadBuffer);
    VERIFY_IS_NOT_NULL(wchReadExpected.get());

    size_t cchTryToRead = 0;

    Log::Comment(L"Test 1: Say we have half the buffer size necessary.");
    cchTryToRead = cchTestTitle / 2;

    // Prepare the buffers and expected data
    TestGetConsoleTitleWPrepExpectedHelper(wszTestTitle,
                                           cchTestTitle + 1,
                                           wchReadBuffer.get(),
                                           cchReadBuffer,
                                           wchReadExpected.get(),
                                           cchReadBuffer,
                                           cchTryToRead);

    // Run the call and test it out.
    TestGetConsoleTitleWVerifyHelper(wchReadBuffer.get(), cchReadBuffer, cchTryToRead, (DWORD)cchTestTitle, S_OK, wchReadExpected.get(), cchReadBuffer);

    Log::Comment(L"Test 2: Say we have have exactly the string length with no null space.");
    cchTryToRead = cchTestTitle;

    // Prepare the buffers and expected data
    TestGetConsoleTitleWPrepExpectedHelper(wszTestTitle,
                                           cchTestTitle + 1,
                                           wchReadBuffer.get(),
                                           cchReadBuffer,
                                           wchReadExpected.get(),
                                           cchReadBuffer,
                                           cchTryToRead);

    // Run the call and test it out.
    TestGetConsoleTitleWVerifyHelper(wchReadBuffer.get(), cchReadBuffer, cchTryToRead, (DWORD)cchTestTitle, S_OK, wchReadExpected.get(), cchReadBuffer);

    Log::Comment(L"Test 3: Say we have have the string length plus one null space.");
    cchTryToRead = cchTestTitle + 1;

    // Prepare the buffers and expected data
    TestGetConsoleTitleWPrepExpectedHelper(wszTestTitle,
                                           cchTestTitle + 1,
                                           wchReadBuffer.get(),
                                           cchReadBuffer,
                                           wchReadExpected.get(),
                                           cchReadBuffer,
                                           cchTryToRead);

    // Run the call and test it out.
    TestGetConsoleTitleWVerifyHelper(wchReadBuffer.get(), cchReadBuffer, cchTryToRead, (DWORD)cchTestTitle, S_OK, wchReadExpected.get(), cchReadBuffer);

    Log::Comment(L"Test 4: Say we have the string length with a null space and an extra space.");
    cchTryToRead = cchTestTitle + 1 + 1;

    // Prepare the buffers and expected data
    TestGetConsoleTitleWPrepExpectedHelper(wszTestTitle,
                                           cchTestTitle + 1,
                                           wchReadBuffer.get(),
                                           cchReadBuffer,
                                           wchReadExpected.get(),
                                           cchReadBuffer,
                                           cchTryToRead);

    // Run the call and test it out.
    TestGetConsoleTitleWVerifyHelper(wchReadBuffer.get(), cchReadBuffer, cchTryToRead, (DWORD)cchTestTitle, S_OK, wchReadExpected.get(), cchReadBuffer);

    Log::Comment(L"Test 5: Say we have no buffer.");
    cchTryToRead = cchTestTitle;

    // Run the call and test it out.
    TestGetConsoleTitleWVerifyHelper(nullptr, 0, 0, 0, S_OK, nullptr, 0);
}
