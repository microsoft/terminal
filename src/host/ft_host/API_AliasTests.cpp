// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <future>

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

    BEGIN_TEST_METHOD(TestCookedAliasProcessing)
        TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestCookedTextEntry)
        TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestCookedAlphaPermutations)
        //TEST_METHOD_PROPERTY(L"TestTimeout", L"00:01:00")
        TEST_METHOD_PROPERTY(L"Data:inputcp", L"{437, 932}")
        TEST_METHOD_PROPERTY(L"Data:outputcp", L"{437, 932}")
        TEST_METHOD_PROPERTY(L"Data:inputmode", L"{487, 481}") // 487 is 0x1e7, 485 is 0x1e1 (ENABLE_LINE_INPUT on/off)
        TEST_METHOD_PROPERTY(L"Data:outputmode", L"{7}")
        TEST_METHOD_PROPERTY(L"Data:font", L"{Consolas, MS Gothic}")
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

static std::vector<INPUT_RECORD> _stringToInputs(std::wstring_view wstr)
{
    std::vector<INPUT_RECORD> result;
    for (const auto& wch : wstr)
    {
        INPUT_RECORD ir = { 0 };
        ir.EventType = KEY_EVENT;
        ir.Event.KeyEvent.bKeyDown = TRUE;
        ir.Event.KeyEvent.dwControlKeyState = 0;
        ir.Event.KeyEvent.uChar.UnicodeChar = wch;
        ir.Event.KeyEvent.wRepeatCount = 1;
        ir.Event.KeyEvent.wVirtualKeyCode = VkKeyScanW(wch);
        ir.Event.KeyEvent.wVirtualScanCode = gsl::narrow<WORD>(MapVirtualKeyW(ir.Event.KeyEvent.wVirtualKeyCode, MAPVK_VK_TO_VSC));

        result.emplace_back(ir);

        ir.Event.KeyEvent.bKeyDown = FALSE;

        result.emplace_back(ir);
    }
    return result;
}

void AliasTests::TestCookedAliasProcessing()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&]
    {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    auto modulePath = wil::GetModuleFileNameW<std::wstring>(nullptr);
    std::filesystem::path path{ modulePath };
    auto fileName = path.filename();
    auto exeName = fileName.wstring();

    VERIFY_WIN32_BOOL_SUCCEEDED(AddConsoleAliasW(L"foo", L"echo bar$Techo baz$Techo bam", exeName.data()));

    std::wstring commandWritten = L"foo\r\n";
    std::queue<std::string> commandExpected;
    commandExpected.push("echo bar\r");
    commandExpected.push("echo baz\r");
    commandExpected.push("echo bam\r");

    auto inputs = _stringToInputs(commandWritten);

    DWORD written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(in, inputs.data(), gsl::narrow<DWORD>(inputs.size()), &written));

    std::string buf;
    buf.resize(500);

    while (!commandExpected.empty())
    {
        DWORD read = 0;

        auto tryRead = std::async(std::launch::async, [&] {
            VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleA(in, buf.data(), gsl::narrow<DWORD>(buf.size()), &read, nullptr));
        });
        
        if (std::future_status::ready != tryRead.wait_for(std::chrono::seconds{ 5 }))
        {
            // Shove something into the input to unstick it then fail.
            auto events = _stringToInputs(L"a\r\n");
            WriteConsoleInputW(in, events.data(), gsl::narrow<DWORD>(events.size()), &written);
            VERIFY_FAILED(HRESULT_FROM_NT(STATUS_TIMEOUT));

            // If somehow this still isn't enough to unstick the thread, the whole test timeout is 1 min
            // set in the parameters/metadata at the top.

            return;
        }

        auto actual = std::string{ buf.data(), read };

        auto expected = commandExpected.front();
        commandExpected.pop();

        VERIFY_ARE_EQUAL(expected, actual);
    }
}

void AliasTests::TestCookedTextEntry()
{
    const auto in = GetStdInputHandle();

    DWORD originalInMode = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));

    DWORD originalCodepage = GetConsoleCP();

    auto restoreInModeOnExit = wil::scope_exit([&] {
        SetConsoleMode(in, originalInMode);
        SetConsoleCP(originalCodepage);
    });

    const DWORD testInMode = ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT;
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, testInMode));

    auto modulePath = wil::GetModuleFileNameW<std::wstring>(nullptr);
    std::filesystem::path path{ modulePath };
    auto fileName = path.filename();
    auto exeName = fileName.wstring();

    
    std::wstring commandWritten = L"foo\r\n";
    std::queue<std::string> commandExpected;
    commandExpected.push("foo\r\n");

    auto inputs = _stringToInputs(commandWritten);

    DWORD written = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(in, inputs.data(), gsl::narrow<DWORD>(inputs.size()), &written));

    std::string buf;
    buf.resize(500);

    while (!commandExpected.empty())
    {
        DWORD read = 0;

        auto tryRead = std::async(std::launch::async, [&] {
            VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleA(in, buf.data(), gsl::narrow<DWORD>(buf.size()), &read, nullptr));
        });

        if (std::future_status::ready != tryRead.wait_for(std::chrono::seconds{ 5 }))
        {
            // Shove something into the input to unstick it then fail.
            auto events = _stringToInputs(L"a\r\n");
            WriteConsoleInputW(in, events.data(), gsl::narrow<DWORD>(events.size()), &written);
            VERIFY_FAILED(HRESULT_FROM_NT(STATUS_TIMEOUT));

            // If somehow this still isn't enough to unstick the thread, the whole test timeout is 1 min
            // set in the parameters/metadata at the top.

            return;
        }

        auto actual = std::string{ buf.data(), read };

        auto expected = commandExpected.front();
        commandExpected.pop();

        VERIFY_ARE_EQUAL(expected, actual);
    }
}

// tests todo:
// - test alpha in/out in 437/932 permutations
// - test leftover leadbyte/trailbyte when buffer too small
void AliasTests::TestCookedAlphaPermutations()
{
    DWORD inputcp, outputcp, inputmode, outputmode;
    String font;

    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"inputcp", inputcp), L"Get input cp");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"outputcp", outputcp), L"Get output cp");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"inputmode", inputmode), L"Get input mode");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"outputmode", outputmode), L"Get output mode");
    VERIFY_SUCCEEDED_RETURN(TestData::TryGetValue(L"font", font), L"Get font");

    std::wstring wstrFont{ font };
    if (wstrFont == L"MS Gothic")
    {
        // MS Gothic... but in full width characters and the katakana representation...
        // MS GOSHIKKU romanized...
        wstrFont = L"\xff2d\xff33\x0020\x30b4\x30b7\x30c3\x30af";
    }

    const auto in = GetStdInputHandle();
    const auto out = GetStdOutputHandle();

    Log::Comment(L"Backup original modes and codepages and font.");

    DWORD originalInMode, originalOutMode, originalInputCP, originalOutputCP;
    CONSOLE_FONT_INFOEX originalFont = { 0 };
    originalFont.cbSize = sizeof(originalFont);
    
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(in, &originalInMode));
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleMode(out, &originalOutMode));
    originalInputCP = GetConsoleCP();
    originalOutputCP = GetConsoleOutputCP();
    VERIFY_WIN32_BOOL_SUCCEEDED(GetCurrentConsoleFontEx(out, FALSE, &originalFont));

    auto restoreModesOnExit = wil::scope_exit([&]
    {
        SetConsoleMode(in, originalInMode);
        SetConsoleMode(out, originalOutMode);
        SetConsoleCP(originalInputCP);
        SetConsoleOutputCP(originalOutputCP);
        SetCurrentConsoleFontEx(out, FALSE, &originalFont);
    });

    Log::Comment(L"Apply our modes and codepages and font.");
    
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(in, inputmode));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleMode(out, outputmode));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(inputcp));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(outputcp));

    auto ourFont = originalFont;
    wmemcpy_s(ourFont.FaceName, ARRAYSIZE(ourFont.FaceName), wstrFont.data(), wstrFont.size());

    VERIFY_WIN32_BOOL_SUCCEEDED(SetCurrentConsoleFontEx(out, FALSE, &ourFont));

    const wchar_t alpha = L'\u03b1';
    const std::string alpha437 = "\xe0";
    const std::string alpha932 = "\x83\xbf";

    std::string expected = inputcp == 932 ? alpha932 : alpha437;

    std::wstring sendInput;
    sendInput.append(&alpha, 1);

    // If we're in line input, we have to send a newline and we'll get one back.
    if (WI_IsFlagSet(inputmode, ENABLE_LINE_INPUT))
    {
        expected.append("\r\n");
        sendInput.append(L"\r\n");
    }

    Log::Comment(L"send the string");
    auto sendInputRecords = _stringToInputs(sendInput);
    DWORD written;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(in, sendInputRecords.data(), gsl::narrow<DWORD>(sendInputRecords.size()), &written));

    Log::Comment(L"receive the string");
    std::string recvInput;
    recvInput.resize(500); // excessively big
    DWORD read;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleA(in, recvInput.data(), gsl::narrow<DWORD>(recvInput.size()), &read, nullptr));

    recvInput.resize(read);

    // corruption magic
    // In MS Gothic, alpha is full width (2 columns)
    // In Consolas, alpha is half width (1 column)
    // Alpha itself is an ambiguous character, meaning the console finds the width
    // by asking the font.
    // Unfortunately, there's some code mixed up in the cooked read for a long time where
    // the width is used as a predictor of how many bytes it will consume.
    // In this specific combination of using a font where the ambiguous alpha is half width,
    // the output code page doesn't support double bytes, and the input code page does...
    // The result is stomped with a null as the conversion fails thinking it doesn't have enough space.
    if (wstrFont == L"Consolas" && inputcp == 932 && outputcp == 437)
    {
        VERIFY_IS_GREATER_THAN_OR_EQUAL(recvInput.size(), 1);

        VERIFY_ARE_EQUAL('\x00', recvInput[0]);

        if (WI_IsFlagSet(inputmode, ENABLE_LINE_INPUT))
        {
            VERIFY_IS_GREATER_THAN_OR_EQUAL(recvInput.size(), 3);
            VERIFY_ARE_EQUAL('\r', recvInput[1]);
            VERIFY_ARE_EQUAL('\n', recvInput[2]);
        }
    }
    // end corruption magic
    else
    {
        VERIFY_ARE_EQUAL(expected, recvInput);
    }
}

// TODO tests:
// - leaving behind a lead/trail byte
// - leaving behind a lead/trail byte and having more data
// -- doing it in a loop/continuously.
// - read it char by char
// - change the codepage in the middle of reading and/or between commands
