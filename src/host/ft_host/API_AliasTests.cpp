// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

// This particular block is necessary so we can include both a UNICODE and non-UNICODE version
// of our test supporting function so we can accurately portray and measure both types of text
// and test both versions of the console API.

// 1. Turn on Unicode if it isn't on already (it really should be) and include the headers
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "API_AliasTestsHelpers.hpp"

// 2. Undefine Unicode and include the header again to get the other version of the functions
#undef UNICODE
#undef _UNICODE
#include "API_AliasTestsHelpers.hpp"

// 3. Finish up by putting Unicode back on for the rest of the code (like it should have been in the first place)
#define UNICODE
#define _UNICODE
// End double include block.

// This class is intended to test:
// GetConsoleAlias
using namespace WEX::TestExecution;
using namespace WEX::Common;
using WEX::Logging::Log;

class AliasTests
{
    BEGIN_TEST_CLASS(AliasTests)
    END_TEST_CLASS()

    BEGIN_TEST_METHOD(TestGetConsoleAlias)
        TEST_METHOD_PROPERTY(L"Data:strSource", L"{g}")
        TEST_METHOD_PROPERTY(L"Data:strExpectedTarget", L"{cmd.exe /k echo foo}")
        TEST_METHOD_PROPERTY(L"Data:strExeName", L"{cmd.exe}")
        TEST_METHOD_PROPERTY(L"Data:dwSource", L"{0, 1}")
        TEST_METHOD_PROPERTY(L"Data:dwTarget", L"{0, 1, 2, 3, 4, 5, 6}")
        TEST_METHOD_PROPERTY(L"Data:dwExeName", L"{0, 1}")
        TEST_METHOD_PROPERTY(L"Data:bUnicode", L"{FALSE, TRUE}")
        TEST_METHOD_PROPERTY(L"Data:bSetFirst", L"{FALSE, TRUE}")
    END_TEST_METHOD()
};

// Caller must free ppsz if not null.
void ConvertWToA(_In_ PCWSTR pwsz,
                 _Out_ char** ppsz)
{
    *ppsz = nullptr;

    UINT const cp = CP_ACP;

    DWORD const dwBytesNeeded = WideCharToMultiByte(cp, 0, pwsz, -1, nullptr, 0, nullptr, nullptr);
    VERIFY_WIN32_BOOL_SUCCEEDED(dwBytesNeeded, L"Verify that WC2MB could detect bytes needed for conversion.");

    char* psz = new char[dwBytesNeeded];
    VERIFY_IS_NOT_NULL(psz, L"Verify we could allocate necessary bytes for conversion.");

    VERIFY_WIN32_BOOL_SUCCEEDED(WideCharToMultiByte(cp, 0, pwsz, -1, psz, dwBytesNeeded, nullptr, nullptr), L"Verify that WC2MB did the conversion successfully.");

    *ppsz = psz;
}

void AliasTests::TestGetConsoleAlias()
{
    // Retrieve combinatorial parameters.
    DWORD dwSource, dwTarget, dwExeName;
    bool bUnicode, bSetFirst;

    String strSource, strExpectedTarget, strExeName;

    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"strSource", strSource), L"Get source string");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"strExpectedTarget", strExpectedTarget), L"Get expected target string");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"strExeName", strExeName), L"Get EXE name");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"dwSource", dwSource), L"Get source string type");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"dwTarget", dwTarget), L"Get target string type");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"dwExeName", dwExeName), L"Get EXE Name type");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"bUnicode", bUnicode), L"Get whether this test is running in Unicode.");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"bSetFirst", bSetFirst), L"Whether we should set this alias before trying to get it.");

    Log::Comment(String().Format(L"Source type: %d  Target type: %d  Exe type: %d  Unicode: %d  Set First: %d\r\n", dwSource, dwTarget, dwExeName, bUnicode, bSetFirst));

    if (bUnicode)
    {
        TestGetConsoleAliasHelperW((wchar_t*)strSource.GetBuffer(),
                                   (wchar_t*)strExpectedTarget.GetBuffer(),
                                   (wchar_t*)strExeName.GetBuffer(),
                                   dwSource,
                                   dwTarget,
                                   dwExeName,
                                   bUnicode,
                                   bSetFirst);
    }
    else
    {
        // If we're not Unicode, we need to convert all the Unicode strings from our test into A strings.
        char* szSource = nullptr;
        char* szExpectedTarget = nullptr;
        char* szExeName = nullptr;

        auto cleanupSource = wil::scope_exit([&] {
            if (nullptr != szSource)
            {
                delete[] szSource;
                szSource = nullptr;
            }
        });

        auto cleanupExpectedTarget = wil::scope_exit([&] {
            if (nullptr != szExpectedTarget)
            {
                delete[] szExpectedTarget;
                szExpectedTarget = nullptr;
            }
        });

        auto cleanupExeName = wil::scope_exit([&] {
            if (nullptr != szExeName)
            {
                delete[] szExeName;
                szExeName = nullptr;
            }
        });

        ConvertWToA(strSource, &szSource);
        ConvertWToA(strExpectedTarget, &szExpectedTarget);
        ConvertWToA(strExeName, &szExeName);

        TestGetConsoleAliasHelperA(szSource,
                                   szExpectedTarget,
                                   szExeName,
                                   dwSource,
                                   dwTarget,
                                   dwExeName,
                                   bUnicode,
                                   bSetFirst);
    }
}
