// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include <io.h>
#include <fcntl.h>

#include "../../types/inc/IInputEvent.hpp"

#define JAPANESE_CP 932u

// CHAR_INFO's .Char member is a union of a wchar_t UnicodeChar and char AsciiChar.
// If they share the same offsetof we can write the lower byte of the former to
// overwrite the latter, while ensuring that the high byte is properly cleared to 0.
static_assert(offsetof(CHAR_INFO, Char.UnicodeChar) == offsetof(CHAR_INFO, Char.AsciiChar));

template<typename T>
constexpr CHAR_INFO makeCharInfo(T ch, WORD attr)
{
    CHAR_INFO info{};
    // If T is a char, it'll be a signed integer, whereas UnicodeChar is an unsigned one.
    // A negative char like -1 would then result in a wchar_t of 0xffff instead of the expected 0xff.
    // Casting ch to a unsigned integer first prevents such "sign extension".
    info.Char.UnicodeChar = static_cast<WCHAR>(til::as_unsigned(ch));
    info.Attributes = attr;
    return info;
}

using CharInfoPattern = std::array<CHAR_INFO, 16>;

// These two are the same strings but in different encodings.
// Both strings are exactly 16 "cells" wide which matches the size of CharInfoPattern.
static constexpr std::string_view dbcsInput{ "Q\x82\xA2\x82\xa9\x82\xc8ZYXWVUT\x82\xc9" }; // Shift-JIS (Codepage 932)
static constexpr std::wstring_view unicodeInput{ L"QいかなZYXWVUTに" }; // Regular UTF-16

using namespace WEX::Logging;
using WEX::TestExecution::TestData;
using namespace WEX::Common;

namespace DbcsWriteRead
{
    enum WriteMode
    {
        CrtWrite = 0,
        WriteConsoleOutputFunc = 1,
        WriteConsoleOutputCharacterFunc = 2,
        WriteConsoleFunc = 3
    };

    enum ReadMode
    {
        ReadConsoleOutputFunc = 0,
        ReadConsoleOutputCharacterFunc = 1
    };

    enum UnicodeMode
    {
        Ascii = 0,
        UnicodeSingle,
        UnicodeDoubled,
    };

    void TestRunner(_In_opt_ WORD* const pwAttrOverride,
                    const bool fUseTrueType,
                    const DbcsWriteRead::WriteMode WriteMode,
                    const UnicodeMode fWriteInUnicode,
                    const DbcsWriteRead::ReadMode ReadMode,
                    const bool fReadWithUnicode);

    bool Setup(_In_ bool fIsTrueType,
               _Out_ HANDLE* const phOut,
               _Out_ WORD* const pwAttributes);

    void SendOutput(const HANDLE hOut,
                    const WriteMode WriteMode,
                    const UnicodeMode fIsUnicode,
                    const WORD wAttr);

    void RetrieveOutput(const HANDLE hOut,
                        const DbcsWriteRead::ReadMode ReadMode,
                        const bool fReadUnicode,
                        CharInfoPattern& rgChars);

    void Verify(std::span<CHAR_INFO> rgExpected, std::span<CHAR_INFO> rgActual);

    void PrepExpected(
        const WORD wAttrWritten,
        const DbcsWriteRead::WriteMode WriteMode,
        const DbcsWriteRead::UnicodeMode fWriteWithUnicode,
        const bool fIsTrueTypeFont,
        const DbcsWriteRead::ReadMode ReadMode,
        const bool fReadWithUnicode,
        CharInfoPattern& expected);

    const CharInfoPattern& PrepReadConsoleOutput(
        const DbcsWriteRead::WriteMode WriteMode,
        const UnicodeMode fWriteWithUnicode,
        const bool fIsTrueTypeFont,
        const bool fReadWithUnicode);

    const CharInfoPattern& PrepReadConsoleOutputCharacter(
        const DbcsWriteRead::WriteMode WriteMode,
        const UnicodeMode fWriteWithUnicode,
        const bool fIsTrueTypeFont,
        const bool fReadWithUnicode);
};

class DbcsTests
{
    BEGIN_TEST_CLASS(DbcsTests)
        TEST_CLASS_PROPERTY(L"IsolationLevel", L"Class")
    END_TEST_CLASS();

    TEST_METHOD_SETUP(DbcsTestSetup);

    // This test must come before ones that launch another process as launching another process can tamper with the codepage
    // in ways that this test is not expecting.
    TEST_METHOD(TestMultibyteInputRetrieval);
    TEST_METHOD(TestMultibyteInputCoalescing);

    BEGIN_TEST_METHOD(TestDbcsWriteRead)
        TEST_METHOD_PROPERTY(L"Data:fUseTrueTypeFont", L"{true, false}")
        TEST_METHOD_PROPERTY(L"Data:WriteMode", L"{0, 1, 2, 3}")
        TEST_METHOD_PROPERTY(L"Data:fWriteInUnicode", L"{0, 1, 2}")
        TEST_METHOD_PROPERTY(L"Data:ReadMode", L"{0, 1}")
        TEST_METHOD_PROPERTY(L"Data:fReadInUnicode", L"{true, false}")
    END_TEST_METHOD()

    TEST_METHOD(TestDbcsBisect);

    BEGIN_TEST_METHOD(TestDbcsBisectWriteCellsBeginW)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDbcsBisectWriteCellsEndW)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDbcsBisectWriteCellsBeginA)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDbcsBisectWriteCellsEndA)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDbcsOneByOne)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDbcsTrailLead)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDbcsStdCoutScenario)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestDbcsBackupRestore)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()

    BEGIN_TEST_METHOD(TestInvalidTrailer)
        TEST_METHOD_PROPERTY(L"IsolationLevel", L"Method")
    END_TEST_METHOD()
};

bool DbcsTests::DbcsTestSetup()
{
    return true;
}

bool DbcsWriteRead::Setup(_In_ bool fIsTrueType,
                          _Out_ HANDLE* const phOut,
                          _Out_ WORD* const pwAttributes)
{
    const auto hOut = GetStdOutputHandle();

    // Ensure that the console is set into the appropriate codepage for the test
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(SetConsoleCP(JAPANESE_CP));
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(SetConsoleOutputCP(JAPANESE_CP));

    // Now set up the font. Many of these APIs are oddly dependent on font, so set as appropriate.
    CONSOLE_FONT_INFOEX cfiex = { 0 };
    cfiex.cbSize = sizeof(cfiex);
    if (!fIsTrueType)
    {
        // We use Terminal as the raster font name always.
        wcscpy_s(cfiex.FaceName, L"Terminal");

        // Use default raster font size from Japanese system.
        cfiex.dwFontSize.X = 8;
        cfiex.dwFontSize.Y = 18;
    }
    else
    {
        wcscpy_s(cfiex.FaceName, L"MS Gothic");
        cfiex.dwFontSize.Y = 16;
    }

    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(OneCoreDelay::SetCurrentConsoleFontEx(hOut, FALSE, &cfiex));

    // Ensure that we set the font we expected to set
    CONSOLE_FONT_INFOEX cfiexGet = { 0 };
    cfiexGet.cbSize = sizeof(cfiexGet);
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(OneCoreDelay::GetCurrentConsoleFontEx(hOut, FALSE, &cfiexGet));

    if (0 != NoThrowString(cfiex.FaceName).CompareNoCase(cfiexGet.FaceName))
    {
        Log::Comment(L"Could not change font. This system doesn't have the fonts we need to perform this test. Skipping.");
        Log::Result(WEX::Logging::TestResults::Result::Skipped);
        return false;
    }

    // Retrieve some of the information about the preferences/settings for the console buffer including
    // the size of the buffer and the default colors (attributes) to use.
    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(sbiex);
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(GetConsoleScreenBufferInfoEx(hOut, &sbiex));

    // ensure first line of console is cleared out with spaces so nothing interferes with the text these tests will be writing.
    COORD coordZero = { 0 };
    DWORD dwWritten;
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(FillConsoleOutputCharacterW(hOut, L'\x20', sbiex.dwSize.X, coordZero, &dwWritten));
    VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(FillConsoleOutputAttribute(hOut, sbiex.wAttributes, sbiex.dwSize.X, coordZero, &dwWritten));

    // Move the cursor to the 0,0 position into our empty line so the tests can write (important for the CRT tests that specify no location)
    if (!SetConsoleCursorPosition(GetStdOutputHandle(), coordZero))
    {
        VERIFY_FAIL(L"Failed to set cursor position");
    }

    // Give back the output handle and the default attributes so tests can verify attributes didn't change on roundtrip
    *phOut = hOut;
    *pwAttributes = sbiex.wAttributes;

    return true;
}

void DbcsWriteRead::SendOutput(const HANDLE hOut,
                               const DbcsWriteRead::WriteMode WriteMode,
                               const UnicodeMode fIsUnicode,
                               const WORD wAttr)
{
    // These parameters will be used to print out the written rectangle if we used the console APIs (not the CRT APIs)
    // This information will be stored and printed out at the very end after we move the cursor off of the text we just printed.
    // The cursor auto-moves for CRT, but we have to manually move it for some of the Console APIs.
    auto fUseRectWritten = false;
    SMALL_RECT srWrittenExpected = { 0 };
    SMALL_RECT srWritten = { 0 };

    auto fUseDwordWritten = false;
    DWORD dwWrittenExpected = 0;
    DWORD dwWritten = 0;

    switch (WriteMode)
    {
    case DbcsWriteRead::WriteMode::CrtWrite:
    {
        // Align the CRT's mode with the text we're about to write.
        // If you call a W function on the CRT while the mode is still set to A,
        // the CRT will helpfully back-convert your text from W to A before sending it to the driver.
        if (fIsUnicode)
        {
            _setmode(_fileno(stdout), _O_WTEXT);
        }
        else
        {
            _setmode(_fileno(stdout), _O_TEXT);
        }

        // Write each character in the string individually out through the CRT
        if (fIsUnicode)
        {
            for (const auto& ch : unicodeInput)
            {
                putwchar(ch);
            }
        }
        else
        {
            for (const auto& ch : dbcsInput)
            {
                putchar(ch);
            }
        }
        break;
    }
    case DbcsWriteRead::WriteMode::WriteConsoleOutputFunc:
    {
        // If we're going to be using WriteConsoleOutput, we need to create up a nice
        // CHAR_INFO buffer to pass into the method containing the string and possibly attributes
        std::vector<CHAR_INFO> rgChars;
        rgChars.reserve(dbcsInput.size());

        switch (fIsUnicode)
        {
        case UnicodeMode::UnicodeSingle:
            for (const auto& ch : unicodeInput)
            {
                rgChars.push_back(makeCharInfo(ch, wAttr));
            }
            break;
        case UnicodeMode::UnicodeDoubled:
            for (const auto& ch : unicodeInput)
            {
                // For the sake of this test we're going to simply assume that any non-ASCII character is wide.
                if (ch < 0x80)
                {
                    rgChars.push_back(makeCharInfo(ch, wAttr));
                }
                else
                {
                    rgChars.push_back(makeCharInfo(ch, wAttr | COMMON_LVB_LEADING_BYTE));
                    rgChars.push_back(makeCharInfo(ch, wAttr | COMMON_LVB_TRAILING_BYTE));
                }
            }
            break;
        default:
            for (const auto& ch : dbcsInput)
            {
                rgChars.push_back(makeCharInfo(ch, wAttr));
            }
            break;
        }

        // This is the stated size of the buffer we're passing.
        // This console API can treat the buffer as a 2D array. We're only doing 1 dimension so the Y is 1 and the X is the number of CHAR_INFO characters.
        COORD coordBufferSize = { 0 };
        coordBufferSize.Y = 1;
        coordBufferSize.X = gsl::narrow<SHORT>(rgChars.size());

        // We want to write to the coordinate 0,0 of the buffer. The test setup function has blanked out that line.
        COORD coordBufferTarget = { 0 };

        // inclusive rectangle (bottom and right are INSIDE the read area. usually are exclusive.)
        SMALL_RECT srWriteRegion = { 0 };

        // Since we could have full-width characters, we have to "allow" the console to write up to the entire A string length (up to double the W length)
        srWriteRegion.Right = gsl::narrow<SHORT>(dbcsInput.size()) - 1;

        // Save the expected written rectangle for comparison after the call
        srWrittenExpected = { 0 };
        srWrittenExpected.Right = coordBufferSize.X - 1; // we expect that the written report will be the number of characters inserted, not the size of buffer consumed

        // NOTE: Don't VERIFY these calls or we will overwrite the text in the buffer with the log message.
        if (fIsUnicode)
        {
            WriteConsoleOutputW(hOut, rgChars.data(), coordBufferSize, coordBufferTarget, &srWriteRegion);
        }
        else
        {
            WriteConsoleOutputA(hOut, rgChars.data(), coordBufferSize, coordBufferTarget, &srWriteRegion);
        }

        // Save write region so we can print it out after we move the cursor out of the way
        srWritten = srWriteRegion;
        fUseRectWritten = true;
        break;
    }
    case DbcsWriteRead::WriteMode::WriteConsoleOutputCharacterFunc:
    {
        COORD coordBufferTarget = { 0 };

        if (fIsUnicode)
        {
            dwWrittenExpected = gsl::narrow<DWORD>(unicodeInput.size());
            WriteConsoleOutputCharacterW(hOut, unicodeInput.data(), dwWrittenExpected, coordBufferTarget, &dwWritten);
        }
        else
        {
            dwWrittenExpected = gsl::narrow<DWORD>(dbcsInput.size());
            WriteConsoleOutputCharacterA(hOut, dbcsInput.data(), dwWrittenExpected, coordBufferTarget, &dwWritten);
        }

        fUseDwordWritten = true;
        break;
    }
    case DbcsWriteRead::WriteMode::WriteConsoleFunc:
    {
        if (fIsUnicode)
        {
            dwWrittenExpected = gsl::narrow<DWORD>(unicodeInput.size());
            WriteConsoleW(hOut, unicodeInput.data(), dwWrittenExpected, &dwWritten, nullptr);
        }
        else
        {
            dwWrittenExpected = gsl::narrow<DWORD>(dbcsInput.size());
            WriteConsoleA(hOut, dbcsInput.data(), dwWrittenExpected, &dwWritten, nullptr);
        }

        fUseDwordWritten = true;
        break;
    }
    default:
        VERIFY_FAIL(L"Unsupported write mode.");
    }

    // Move the cursor down a line in case log info prints out.
    COORD coordSetCursor = { 0 };
    coordSetCursor.Y = 1;
    SetConsoleCursorPosition(hOut, coordSetCursor);

    // If we had log info to print, print it now that it's safe (cursor out of the test data we printed)
    // This only matters for when the test is run in the same window as the runner and could print log information.
    if (fUseRectWritten)
    {
        Log::Comment(NoThrowString().Format(L"WriteRegion T: %d L: %d B: %d R: %d", srWritten.Top, srWritten.Left, srWritten.Bottom, srWritten.Right));
        VERIFY_ARE_EQUAL(srWrittenExpected, srWritten);
    }
    else if (fUseDwordWritten)
    {
        Log::Comment(NoThrowString().Format(L"Chars Written: %d", dwWritten));
        VERIFY_ARE_EQUAL(dwWrittenExpected, dwWritten);
    }
}

namespace PrepPattern
{
    static constexpr WORD zeroed = 0x0000;
    static constexpr WORD white = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
    // If the lower byte in our test data is 0xff it indicates that it's "flexible"
    // and supposed to be replaced with whatever color attributes were written.
    // The upper byte contains leading/trailing flags we're testing for.
    static constexpr WORD colored = 0x00ff;

    static constexpr WORD leading = COMMON_LVB_LEADING_BYTE;
    static constexpr WORD trailing = COMMON_LVB_TRAILING_BYTE;

    constexpr void replaceColorPlaceholders(std::span<CHAR_INFO> pattern, WORD attr)
    {
        for (auto& info : pattern)
        {
            if ((info.Attributes & colored) == colored)
            {
                info.Attributes &= 0xff00 | attr;
            }
        }
    }

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x029 | 0x3044 (0x44) | Hiragana I
    // 0x029 | 0x304B (0x4B) | Hiragana KA
    // 0x029 | 0x306A (0x6A) | Hiragana NA
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x029 | 0x0055 (0x55) | U
    // 0x029 | 0x0054 (0x54) | T
    // 0x029 | 0x306B (0x6B) | Hiragana NI
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // ...
    // "Null Padded" means any unused data in the buffer will be filled with null and null attribute.
    // "Dedupe" means that any full-width characters in the buffer (despite being stored doubled inside the buffer)
    //    will be returned as single copies.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    static constexpr CharInfoPattern NullPaddedDedupeW{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x3044, colored),
        makeCharInfo(0x304b, colored),
        makeCharInfo(0x306a, colored),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0055, colored),
        makeCharInfo(0x0054, colored),
        makeCharInfo(0x306b, colored),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x029 | 0x3044 (0x44) | Hiragana I
    // 0x029 | 0x304B (0x4B) | Hiragana KA
    // 0x029 | 0x306A (0x6A) | Hiragana NA
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x029 | 0x0055 (0x55) | U
    // 0x029 | 0x0054 (0x54) | T
    // 0x029 | 0x306B (0x6B) | Hiragana NI
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // ...
    // "Space Padded" means any unused data in the buffer will be filled with spaces and the default attribute.
    // "Dedupe" means that any full-width characters in the buffer (despite being stored doubled inside the buffer)
    //    will be returned as single copies.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    static constexpr CharInfoPattern SpacePaddedDedupeW{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x3044, colored),
        makeCharInfo(0x304b, colored),
        makeCharInfo(0x306a, colored),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0055, colored),
        makeCharInfo(0x0054, colored),
        makeCharInfo(0x306b, colored),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x029 | 0x0055 (0x55) | U
    // 0x029 | 0x0054 (0x54) | T
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // ...
    // "Space Padded" means any unused data in the buffer will be filled with spaces and the default attribute.
    // "Dedupe" means that any full-width characters in the buffer will be returned as single copies.
    //    But due to the target being a DBCS character set that can't represent these in a single char, it's null.
    // "A" means that we intend in-codepage (char) data to be browsed in the resulting struct
    static constexpr CharInfoPattern SpacePaddedDedupeInvalidA{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0055, colored),
        makeCharInfo(0x0054, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x029 | 0x3044 (0x44) | Hiragana I
    // 0x029 | 0x304B (0x4B) | Hiragana KA
    // 0x029 | 0x306A (0x6A) | Hiragana NA
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // ...
    // "Space Padded" means most of the unused data in the buffer will be filled with spaces and the default attribute.
    // "Dedupe" means that any full-width characters in the buffer (despite being stored doubled inside the buffer)
    //    will be returned as single copies.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    // "Truncated" means that this pattern trims off some of the end of the buffer with NULLs.
    static constexpr CharInfoPattern SpacePaddedDedupeTruncatedW{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x3044, colored),
        makeCharInfo(0x304b, colored),
        makeCharInfo(0x306a, colored),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x029 | 0x3044 (0x44) | Hiragana I
    // 0x029 | 0x3044 (0x44) | Hiragana I
    // 0x029 | 0x304B (0x4B) | Hiragana KA
    // 0x029 | 0x304B (0x4B) | Hiragana KA
    // 0x029 | 0x306A (0x6A) | Hiragana NA
    // 0x029 | 0x306A (0x6A) | Hiragana NA
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // 0x000 | 0x0000 (0x00) | <null>
    // ...
    // "Doubled" means that any full-width characters in the buffer are returned twice.
    // "Truncated" means that this pattern trims off some of the end of the buffer with NULLs.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    static constexpr CharInfoPattern DoubledTruncatedW{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x3044, colored),
        makeCharInfo(0x3044, colored),
        makeCharInfo(0x304b, colored),
        makeCharInfo(0x304b, colored),
        makeCharInfo(0x306a, colored),
        makeCharInfo(0x306a, colored),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
        makeCharInfo(0x0000, zeroed),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x129 | 0x0082 (0x82) | Hiragana I Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00A2 (0xA2) | Hiragana I Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana KA Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00A9 (0xA9) | Hiragana KA Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana NA Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00C8 (0xC8) | Hiragana NA Shift-JIS Codepage 932 Trail Byte
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // ...
    // "Space Padded" means most of the unused data in the buffer will be filled with spaces and the default attribute.
    // "Dedupe" means that any full-width characters in the buffer (despite being stored doubled inside the buffer)
    //    will be returned as single copies.
    // "A" means that we intend in-codepage (char) data to be browsed in the resulting struct (even though wchar and char are unioned.)
    static constexpr CharInfoPattern SpacePaddedDedupeA{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00a2, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00a9, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00c8, colored | trailing),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0020, white),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x129 | 0x3044 (0x44) | Hiragana I
    // 0x229 | 0x3044 (0x44) | Hiragana I
    // 0x129 | 0x304B (0x4B) | Hiragana KA
    // 0x229 | 0x304B (0x4B) | Hiragana KA
    // 0x129 | 0x306A (0x6A) | Hiragana NA
    // 0x229 | 0x306A (0x6A) | Hiragana NA
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x029 | 0x0055 (0x55) | U
    // 0x029 | 0x0054 (0x54) | T
    // 0x129 | 0x306B (0x6B) | Hiragana NI
    // 0x229 | 0x306B (0x6B) | Hiragana NI
    // ...
    // "Doubled" means that any full-width characters in the buffer are returned twice with a leading and trailing byte marker.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    static constexpr CharInfoPattern DoubledW{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x3044, colored | leading),
        makeCharInfo(0x3044, colored | trailing),
        makeCharInfo(0x304b, colored | leading),
        makeCharInfo(0x304b, colored | trailing),
        makeCharInfo(0x306a, colored | leading),
        makeCharInfo(0x306a, colored | trailing),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0055, colored),
        makeCharInfo(0x0054, colored),
        makeCharInfo(0x306b, colored | leading),
        makeCharInfo(0x306b, colored | trailing),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x129 | 0x0082 (0x82) | Hiragana I Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00A2 (0xA2) | Hiragana I Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana KA Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00A9 (0xA9) | Hiragana KA Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana NA Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00C8 (0xC8) | Hiragana NA Shift-JIS Codepage 932 Trail Byte
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x029 | 0x0055 (0x55) | U
    // 0x029 | 0x0054 (0x54) | T
    // 0x129 | 0x0082 (0x82) | Hiragana NI Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00C9 (0xC9) | Hiragana NI Shift-JIS Codepage 932 Trail Byte
    // ...
    // "A" means that we intend in-codepage (char) data to be browsed in the resulting struct.
    // This one returns pretty much exactly as expected.
    static constexpr CharInfoPattern A{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00a2, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00a9, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00c8, colored | trailing),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0055, colored),
        makeCharInfo(0x0054, colored),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00c9, colored | trailing),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x129 | 0x0082 (0x82) | Hiragana I Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00A2 (0xA2) | Hiragana I Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana I Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00A2 (0xA2) | Hiragana I Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana KA Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00A9 (0xA9) | Hiragana KA Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana KA Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00A9 (0xA9) | Hiragana KA Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana NA Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00C8 (0xC8) | Hiragana NA Shift-JIS Codepage 932 Trail Byte
    // 0x129 | 0x0082 (0x82) | Hiragana NA Shift-JIS Codepage 932 Lead Byte
    // 0x229 | 0x00C8 (0xC8) | Hiragana NA Shift-JIS Codepage 932 Trail Byte
    // 0x029 | 0x005A (0x5A) | Z
    // 0x029 | 0x0059 (0x59) | Y
    // 0x029 | 0x0058 (0x58) | X
    // ...
    // "Doubled" means that any full-width characters in the buffer are returned twice.
    // "A" means that we intend in-codepage (char) data to be browsed in the resulting struct.
    static constexpr CharInfoPattern DoubledA{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00a2, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00a2, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00a9, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00a9, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00c8, colored | trailing),
        makeCharInfo(0x0082, colored | leading),
        makeCharInfo(0x00c8, colored | trailing),
        makeCharInfo(0x005a, colored),
        makeCharInfo(0x0059, colored),
        makeCharInfo(0x0058, colored),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x129 | 0x3044 (0x44) | Hiragana I
    // 0x229 | 0x304B (0x4B) | Hiragana KA
    // 0x129 | 0x306A (0x6A) | Hiragana NA
    // 0x229 | 0x005A (0x5A) | Z
    // 0x129 | 0x0059 (0x59) | Y
    // 0x229 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x029 | 0x0055 (0x55) | U
    // 0x029 | 0x0054 (0x54) | T
    // 0x029 | 0x306B (0x6B) | Hiragana NI
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x129 | 0x0000 (0x00) | <null>
    // 0x229 | 0x0000 (0x00) | <null>
    // ...
    // "Null" means any unused data in the buffer will be filled with null.
    // "CoverAChar" means that the attributes belong to the A version of the call, but we've placed de-duped W characters over the top.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    static constexpr CharInfoPattern WNullCoverAChar{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x3044, colored | leading),
        makeCharInfo(0x304b, colored | trailing),
        makeCharInfo(0x306a, colored | leading),
        makeCharInfo(0x005a, colored | trailing),
        makeCharInfo(0x0059, colored | leading),
        makeCharInfo(0x0058, colored | trailing),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0055, colored),
        makeCharInfo(0x0054, colored),
        makeCharInfo(0x306b, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored | leading),
        makeCharInfo(0x0000, colored | trailing),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x129 | 0x3044 (0x44) | Hiragana I
    // 0x229 | 0x3044 (0x44) | Hiragana I
    // 0x129 | 0x304B (0x4B) | Hiragana KA
    // 0x229 | 0x304B (0x4B) | Hiragana KA
    // 0x129 | 0x306A (0x6A) | Hiragana NA
    // 0x229 | 0x306A (0x6A) | Hiragana NA
    // 0x129 | 0x005A (0x5A) | Z
    // 0x229 | 0x0059 (0x59) | Y
    // 0x129 | 0x0058 (0x58) | X
    // 0x229 | 0x0000 (0x00) | <null>
    // 0x129 | 0x0000 (0x00) | <null>
    // 0x229 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // ...
    // "Doubled" means that any full-width characters in the buffer are returned twice.
    // "Truncated" means that this pattern trims off some of the end of the buffer with NULLs.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    static constexpr CharInfoPattern DoubledTruncatedCoverAChar{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x3044, colored | leading),
        makeCharInfo(0x3044, colored | trailing),
        makeCharInfo(0x304b, colored | leading),
        makeCharInfo(0x304b, colored | trailing),
        makeCharInfo(0x306a, colored | leading),
        makeCharInfo(0x306a, colored | trailing),
        makeCharInfo(0x005a, colored | leading),
        makeCharInfo(0x0059, colored | trailing),
        makeCharInfo(0x0058, colored | leading),
        makeCharInfo(0x0000, colored | trailing),
        makeCharInfo(0x0000, colored | leading),
        makeCharInfo(0x0000, colored | trailing),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0051 (0x51) | Q
    // 0x129 | 0x3044 (0x44) | Hiragana I
    // 0x229 | 0x304B (0x4B) | Hiragana KA
    // 0x129 | 0x306A (0x6A) | Hiragana NA
    // 0x229 | 0x005A (0x5A) | Z
    // 0x129 | 0x0059 (0x59) | Y
    // 0x229 | 0x0058 (0x58) | X
    // 0x029 | 0x0057 (0x57) | W
    // 0x029 | 0x0056 (0x56) | V
    // 0x029 | 0x0020 (0x20) | <space>
    // 0x029 | 0x0020 (0x20) | <space>
    // 0x029 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0020 (0x20) | <space>
    // 0x007 | 0x0000 (0x00) | <null>
    // 0x007 | 0x0000 (0x00) | <null>
    // 0x007 | 0x0000 (0x00) | <null>
    // ...
    // "Space Padded" means most of the unused data in the buffer will be filled with spaces and the default attribute.
    // "Dedupe" means that any full-width characters in the buffer (despite being stored doubled inside the buffer)
    //    will be returned as single copies.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    // "Truncated" means that this pattern trims off some of the end of the buffer with NULLs.
    // "A Cover Attr" means that after all the other operations, we will finally run through and cover up the attributes
    //     again with what they would have been for multi-byte data (leading and trailing flags)
    static constexpr CharInfoPattern ACoverAttrSpacePaddedDedupeTruncatedW{
        makeCharInfo(0x0051, colored),
        makeCharInfo(0x3044, colored | leading),
        makeCharInfo(0x304b, colored | trailing),
        makeCharInfo(0x306a, colored | leading),
        makeCharInfo(0x005a, colored | trailing),
        makeCharInfo(0x0059, colored | leading),
        makeCharInfo(0x0058, colored | trailing),
        makeCharInfo(0x0057, colored),
        makeCharInfo(0x0056, colored),
        makeCharInfo(0x0020, colored),
        makeCharInfo(0x0020, colored),
        makeCharInfo(0x0020, colored),
        makeCharInfo(0x0020, white),
        makeCharInfo(0x0000, white),
        makeCharInfo(0x0000, white),
        makeCharInfo(0x0000, white),
    };

    // Receive Output Table:
    // attr  | wchar  (char) | symbol
    // ------------------------------------
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x029 | 0x0000 (0x00) | <null>
    // 0x007 | 0x0000 (0x00) | <null>
    // 0x007 | 0x0000 (0x00) | <null>
    // 0x007 | 0x0000 (0x00) | <null>
    // 0x007 | 0x0000 (0x00) | <null>
    // ...
    // "Space Padded" means most of the unused data in the buffer will be filled with spaces and the default attribute.
    // "Dedupe" means that any full-width characters in the buffer (despite being stored doubled inside the buffer)
    //    will be returned as single copies.
    // "W" means that we intend Unicode data to be browsed in the resulting struct (even though wchar and char are unioned.)
    // "Truncated" means that this pattern trims off some of the end of the buffer with NULLs.
    // "A Cover Attr" means that after all the other operations, we will finally run through and cover up the attributes
    //     again with what they would have been for multi-byte data (leading and trailing flags)
    static constexpr CharInfoPattern TrueTypeCharANullWithAttrs{
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, colored),
        makeCharInfo(0x0000, white),
        makeCharInfo(0x0000, white),
        makeCharInfo(0x0000, white),
        makeCharInfo(0x0000, white),
    };
}

const CharInfoPattern& DbcsWriteRead::PrepReadConsoleOutput(
    const DbcsWriteRead::WriteMode WriteMode,
    const UnicodeMode fWriteWithUnicode,
    const bool fIsTrueTypeFont,
    const bool fReadWithUnicode)
{
    switch (WriteMode)
    {
    case DbcsWriteRead::WriteMode::WriteConsoleOutputFunc:
        switch (fWriteWithUnicode)
        {
        case UnicodeMode::UnicodeSingle:
            if (fReadWithUnicode)
            {
                if (fIsTrueTypeFont)
                {
                    // When written with WriteConsoleOutputW and read back with ReadConsoleOutputW when the font is TrueType,
                    // we will get a deduplicated set of Unicode characters with no lead/trailing markings and space padded at the end.
                    return PrepPattern::SpacePaddedDedupeW;
                }
                else
                {
                    // When written with WriteConsoleOutputW and read back with ReadConsoleOutputW when the font is Raster,
                    // we will get a deduplicated set of Unicode characters with no lead/trailing markings and space padded at the end...
                    // ... except something weird happens with truncation (TODO figure out what)
                    return PrepPattern::SpacePaddedDedupeTruncatedW;
                }
            }
            else
            {
                if (fIsTrueTypeFont)
                {
                    // Normally this would be SpacePaddedDedupeA (analogous to the SpacePaddedDedupeW above), but since the narrow
                    // unicode chars can't be represented as narrow DBCS (since those don't exist) we get SpacePaddedDedupeInvalidA.
                    return PrepPattern::SpacePaddedDedupeInvalidA;
                }
                else
                {
                    // When written with WriteConsoleOutputW and read back with ReadConsoleOutputA under Raster font, we will get the
                    // double-byte sequences stomped on top of a Unicode filled CHAR_INFO structure that used -1 for trailing bytes.
                    return PrepPattern::SpacePaddedDedupeA;
                }
            }
            break;
        case UnicodeMode::UnicodeDoubled:
            if (fReadWithUnicode)
            {
                if (fIsTrueTypeFont)
                {
                    // In a TrueType font, we will get back Unicode characters doubled up and marked with leading and trailing bytes.
                    return PrepPattern::DoubledW;
                }
                else
                {
                    // We get the same as SpacePaddedDedupeTruncatedW above, but due to the unicode chars being doubled, we get DoubledTruncatedW.
                    return PrepPattern::DoubledTruncatedW;
                }
            }
            else
            {
                if (fIsTrueTypeFont)
                {
                    // In a TrueType font, we will get back Unicode characters doubled up and marked with leading and trailing bytes.
                    return PrepPattern::A;
                }
                else
                {
                    // When written with WriteConsoleOutputW and read back with ReadConsoleOutputA under Raster font,
                    // we will get the double-byte sequences doubled up, because each narrow cell is written as a DBCS separately.
                    return PrepPattern::DoubledA;
                }
            }
            break;
        default:
            if (fReadWithUnicode)
            {
                if (fIsTrueTypeFont)
                {
                    // When written with WriteConsoleOutputW and read back with ReadConsoleOutputA when the font is TrueType,
                    // we will get back Unicode characters doubled up and marked with leading and trailing bytes.
                    return PrepPattern::DoubledW;
                }
                else
                {
                    // When written with WriteConsoleOutputA and read back with ReadConsoleOutputW when the font is Raster,
                    // we will get back de-duplicated Unicode characters with no lead / trail markings.The extra array space will remain null.
                    return PrepPattern::NullPaddedDedupeW;
                }
            }
            else
            {
                // When written with WriteConsoleOutputA and read back with ReadConsoleOutputA,
                // we will get back the double-byte sequences appropriately labeled with leading/trailing bytes.
                return PrepPattern::A;
            }
            break;
        }
        break;
    case DbcsWriteRead::WriteMode::CrtWrite:
    case DbcsWriteRead::WriteMode::WriteConsoleOutputCharacterFunc:
    case DbcsWriteRead::WriteMode::WriteConsoleFunc:
        // Writing with the CRT down here.
        if (fReadWithUnicode)
        {
            // If we wrote with the CRT and are reading back with the W functions, the font does matter.
            if (fIsTrueTypeFont)
            {
                // In a TrueType font, we will get back Unicode characters doubled up and marked with leading and trailing bytes.
                return PrepPattern::DoubledW;
            }
            else
            {
                // In a Raster font, we will get back de-duplicated Unicode characters with no lead/trail markings. The extra array space will remain null.
                return PrepPattern::NullPaddedDedupeW;
            }
        }
        else
        {
            // If we wrote with the CRT and are reading with A functions, the font doesn't matter.
            // We will always get back the double-byte sequences appropriately labeled with leading/trailing bytes.
            return PrepPattern::A;
        }
        break;
    default:
        VERIFY_FAIL(L"Unsupported write mode");
        std::terminate();
    }
}

const CharInfoPattern& DbcsWriteRead::PrepReadConsoleOutputCharacter(
    const DbcsWriteRead::WriteMode WriteMode,
    const UnicodeMode fWriteWithUnicode,
    const bool fIsTrueTypeFont,
    const bool fReadWithUnicode)
{
    if (DbcsWriteRead::WriteMode::WriteConsoleOutputFunc == WriteMode)
    {
        switch (fWriteWithUnicode)
        {
        case UnicodeMode::UnicodeSingle:
            if (fReadWithUnicode)
            {
                if (fIsTrueTypeFont)
                {
                    return PrepPattern::SpacePaddedDedupeW;
                }
                else
                {
                    return PrepPattern::ACoverAttrSpacePaddedDedupeTruncatedW;
                }
            }
            else
            {
                if (fIsTrueTypeFont)
                {
                    return PrepPattern::TrueTypeCharANullWithAttrs;
                }
                else
                {
                    return PrepPattern::SpacePaddedDedupeA;
                }
            }
            break;
        case UnicodeMode::UnicodeDoubled:
            if (fReadWithUnicode)
            {
                if (fIsTrueTypeFont)
                {
                    return PrepPattern::WNullCoverAChar;
                }
                else
                {
                    return PrepPattern::DoubledTruncatedCoverAChar;
                }
            }
            else
            {
                if (fIsTrueTypeFont)
                {
                    return PrepPattern::A;
                }
                else
                {
                    return PrepPattern::DoubledA;
                }
            }
            break;
        default:
            if (fReadWithUnicode)
            {
                return PrepPattern::WNullCoverAChar;
            }
            else
            {
                return PrepPattern::A;
            }
            break;
        }
    }
    else
    {
        if (fReadWithUnicode)
        {
            return PrepPattern::WNullCoverAChar;
        }
        else
        {
            return PrepPattern::A;
        }
    }
}

void DbcsWriteRead::PrepExpected(const WORD wAttrWritten,
                                 const DbcsWriteRead::WriteMode WriteMode,
                                 const DbcsWriteRead::UnicodeMode fWriteWithUnicode,
                                 const bool fIsTrueTypeFont,
                                 const DbcsWriteRead::ReadMode ReadMode,
                                 const bool fReadWithUnicode,
                                 CharInfoPattern& expected)
{
    switch (ReadMode)
    {
    case DbcsWriteRead::ReadMode::ReadConsoleOutputFunc:
    {
        expected = DbcsWriteRead::PrepReadConsoleOutput(WriteMode, fWriteWithUnicode, fIsTrueTypeFont, fReadWithUnicode);
        break;
    }
    case DbcsWriteRead::ReadMode::ReadConsoleOutputCharacterFunc:
    {
        expected = DbcsWriteRead::PrepReadConsoleOutputCharacter(WriteMode, fWriteWithUnicode, fIsTrueTypeFont, fReadWithUnicode);
        break;
    }
    default:
    {
        VERIFY_FAIL(L"Unknown read mode.");
        break;
    }
    }

    PrepPattern::replaceColorPlaceholders(expected, wAttrWritten);
}

void DbcsWriteRead::RetrieveOutput(const HANDLE hOut,
                                   const DbcsWriteRead::ReadMode ReadMode,
                                   const bool fReadUnicode,
                                   CharInfoPattern& rgChars)
{
    COORD coordBufferTarget = { 0 };

    switch (ReadMode)
    {
    case DbcsWriteRead::ReadMode::ReadConsoleOutputFunc:
    {
        // Since we wrote (in SendOutput function) to the 0,0 line, we need to read back the same width from that line.
        COORD coordBufferSize = { 0 };
        coordBufferSize.Y = 1;
        coordBufferSize.X = gsl::narrow<SHORT>(rgChars.size());

        SMALL_RECT srReadRegion = { 0 }; // inclusive rectangle (bottom and right are INSIDE the read area. usually are exclusive.)
        srReadRegion.Right = coordBufferSize.X - 1;

        // return value for read region shouldn't change
        const auto srReadRegionExpected = srReadRegion;

        if (!fReadUnicode)
        {
            VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(ReadConsoleOutputA(hOut, rgChars.data(), coordBufferSize, coordBufferTarget, &srReadRegion));
        }
        else
        {
            VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(ReadConsoleOutputW(hOut, rgChars.data(), coordBufferSize, coordBufferTarget, &srReadRegion));
        }

        Log::Comment(NoThrowString().Format(L"ReadRegion T: %d L: %d B: %d R: %d", srReadRegion.Top, srReadRegion.Left, srReadRegion.Bottom, srReadRegion.Right));
        VERIFY_ARE_EQUAL(srReadRegionExpected, srReadRegion);
        break;
    }
    case DbcsWriteRead::ReadMode::ReadConsoleOutputCharacterFunc:
    {
        const auto cChars = gsl::narrow<DWORD>(rgChars.size());
        DWORD dwRead = 0;
        if (!fReadUnicode)
        {
            auto psRead = new char[cChars];
            VERIFY_IS_NOT_NULL(psRead);
            VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(ReadConsoleOutputCharacterA(hOut, psRead, cChars, coordBufferTarget, &dwRead));

            for (size_t i = 0; i < dwRead; i++)
            {
                rgChars[i].Char.AsciiChar = psRead[i];
            }

            delete[] psRead;
        }
        else
        {
            auto pwsRead = new wchar_t[cChars];
            VERIFY_IS_NOT_NULL(pwsRead);
            VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(ReadConsoleOutputCharacterW(hOut, pwsRead, cChars, coordBufferTarget, &dwRead));

            for (size_t i = 0; i < dwRead; i++)
            {
                rgChars[i].Char.UnicodeChar = pwsRead[i];
            }

            delete[] pwsRead;
        }

        auto pwAttrs = new WORD[cChars];
        VERIFY_IS_NOT_NULL(pwAttrs);
        VERIFY_WIN32_BOOL_SUCCEEDED_RETURN(ReadConsoleOutputAttribute(hOut, pwAttrs, cChars, coordBufferTarget, &dwRead));

        for (size_t i = 0; i < dwRead; i++)
        {
            rgChars[i].Attributes = pwAttrs[i];
        }

        delete[] pwAttrs;
        break;
    }
    default:
        VERIFY_FAIL(L"Unknown read mode");
        break;
    }
}

void DbcsWriteRead::Verify(std::span<CHAR_INFO> rgExpected, std::span<CHAR_INFO> rgActual)
{
    VERIFY_ARE_EQUAL(rgExpected.size(), rgActual.size());
    // We will walk through for the number of CHAR_INFOs expected.
    for (size_t i = 0; i < rgExpected.size(); i++)
    {
        // Uncomment these lines for help debugging the verification.
        /*
        Log::Comment(NoThrowString().Format(L"Index: %d:", i));
        Log::Comment(VerifyOutputTraits<CHAR_INFO>::ToString(rgExpected[i]));
        Log::Comment(VerifyOutputTraits<CHAR_INFO>::ToString(rgActual[i]));
        */

        VERIFY_ARE_EQUAL(rgExpected[i], rgActual[i]);
    }
}

void DbcsWriteRead::TestRunner(_In_opt_ WORD* const pwAttrOverride,
                               const bool fUseTrueType,
                               const DbcsWriteRead::WriteMode WriteMode,
                               const UnicodeMode fWriteInUnicode,
                               const DbcsWriteRead::ReadMode ReadMode,
                               const bool fReadWithUnicode)
{
    // First we need to set up the tests by clearing out the first line of the buffer,
    // retrieving the appropriate output handle, and getting the colors (attributes)
    // used by default in the buffer (set during clearing as well).
    HANDLE hOut;
    WORD wAttributes;
    if (!DbcsWriteRead::Setup(fUseTrueType, &hOut, &wAttributes))
    {
        // If we can't set up (setup will detect systems where this test cannot operate) then return early.
        return;
    }

    // Some tests might want to override the colors applied to ensure both parts of the CHAR_INFO union
    // work for methods that support sending that union. (i.e. not the CRT path)
    if (nullptr != pwAttrOverride)
    {
        wAttributes = *pwAttrOverride;
    }

    // Write the string under test into the appropriate WRITE API for this test.
    DbcsWriteRead::SendOutput(hOut, WriteMode, fWriteInUnicode, wAttributes);

    // Prepare the array of CHAR_INFO structs that we expect to receive back when we will call read in a moment.
    // This can vary based on font, unicode/non-unicode (when reading AND writing), and codepage.
    CharInfoPattern pciExpected;
    DbcsWriteRead::PrepExpected(wAttributes, WriteMode, fWriteInUnicode, fUseTrueType, ReadMode, fReadWithUnicode, pciExpected);

    // Now call the appropriate READ API for this test.
    CharInfoPattern pciActual{};
    DbcsWriteRead::RetrieveOutput(hOut, ReadMode, fReadWithUnicode, pciActual);

    // Loop through and verify that our expected array matches what was actually returned by the given API.
    DbcsWriteRead::Verify(pciExpected, pciActual);
}

void DbcsTests::TestDbcsWriteRead()
{
    bool fUseTrueTypeFont;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fUseTrueTypeFont", fUseTrueTypeFont));

    int iWriteMode;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"WriteMode", iWriteMode));
    auto WriteMode = (DbcsWriteRead::WriteMode)iWriteMode;

    int iWriteInUnicode;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fWriteInUnicode", iWriteInUnicode));
    auto fWriteInUnicode = (DbcsWriteRead::UnicodeMode)iWriteInUnicode;

    int iReadMode;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"ReadMode", iReadMode));
    auto ReadMode = (DbcsWriteRead::ReadMode)iReadMode;

    bool fReadInUnicode;
    VERIFY_SUCCEEDED(TestData::TryGetValue(L"fReadInUnicode", fReadInUnicode));

    // UnicodeDoubled is only relevant for WriteConsoleOutputW
    if (fWriteInUnicode == DbcsWriteRead::UnicodeMode::UnicodeDoubled && WriteMode != DbcsWriteRead::WriteMode::WriteConsoleOutputFunc)
    {
        return;
    }

    auto pwszWriteMode = L"";
    switch (WriteMode)
    {
    case DbcsWriteRead::WriteMode::CrtWrite:
        pwszWriteMode = L"CRT";
        break;
    case DbcsWriteRead::WriteMode::WriteConsoleOutputFunc:
        pwszWriteMode = L"WriteConsoleOutput";
        break;
    case DbcsWriteRead::WriteMode::WriteConsoleOutputCharacterFunc:
        pwszWriteMode = L"WriteConsoleOutputCharacter";
        break;
    case DbcsWriteRead::WriteMode::WriteConsoleFunc:
        pwszWriteMode = L"WriteConsole";
        break;
    default:
        VERIFY_FAIL(L"Write mode not supported");
    }

    auto pwszReadMode = L"";
    switch (ReadMode)
    {
    case DbcsWriteRead::ReadMode::ReadConsoleOutputFunc:
        pwszReadMode = L"ReadConsoleOutput";
        break;
    case DbcsWriteRead::ReadMode::ReadConsoleOutputCharacterFunc:
        pwszReadMode = L"ReadConsoleOutputCharacter";
        break;
    default:
        VERIFY_FAIL(L"Read mode not supported");
    }

    auto testInfo = NoThrowString().Format(L"\r\n\r\n\r\nUse '%s' font. Write with %s '%s'%s. Check Read with %s '%s' API. Use %d codepage.\r\n",
                                           fUseTrueTypeFont ? L"TrueType" : L"Raster",
                                           pwszWriteMode,
                                           fWriteInUnicode ? L"W" : L"A",
                                           fWriteInUnicode == DbcsWriteRead::UnicodeMode::UnicodeDoubled ? L" (doubled)" : L"",
                                           pwszReadMode,
                                           fReadInUnicode ? L"W" : L"A",
                                           JAPANESE_CP);

    Log::Comment(testInfo);

    WORD wAttributes = 0;

    if (WriteMode == 1)
    {
        Log::Comment(L"We will also try to change the color since WriteConsoleOutput supports it.");
        wAttributes = FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_GREEN;
    }

    DbcsWriteRead::TestRunner(wAttributes != 0 ? &wAttributes : nullptr,
                              fUseTrueTypeFont,
                              WriteMode,
                              fWriteInUnicode,
                              ReadMode,
                              fReadInUnicode);

    Log::Comment(testInfo);
}

// This test covers bisect-prevention handling. This is the behavior where a double-wide character will not be spliced
// across a line boundary and will instead be advanced onto the next line.
// It additionally exercises the word wrap functionality to ensure that the bisect calculations continue
// to apply properly when wrap occurs.
void DbcsTests::TestDbcsBisect()
{
    const auto hOut = GetStdOutputHandle();

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(JAPANESE_CP));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(JAPANESE_CP));

    auto dwCP = GetConsoleCP();
    VERIFY_ARE_EQUAL(dwCP, JAPANESE_CP);

    auto dwOutputCP = GetConsoleOutputCP();
    VERIFY_ARE_EQUAL(dwOutputCP, JAPANESE_CP);

    CONSOLE_SCREEN_BUFFER_INFOEX sbiex = { 0 };
    sbiex.cbSize = sizeof(CONSOLE_SCREEN_BUFFER_INFOEX);
    auto fSuccess = GetConsoleScreenBufferInfoEx(hOut, &sbiex);

    if (CheckLastError(fSuccess, L"GetConsoleScreenBufferInfoEx"))
    {
        Log::Comment(L"Set cursor position to the last column in the buffer width.");
        sbiex.dwCursorPosition.X = sbiex.dwSize.X - 1;

        const auto coordEndOfLine = sbiex.dwCursorPosition; // this is the end of line position we're going to write at
        COORD coordStartOfNextLine;
        coordStartOfNextLine.X = 0;
        coordStartOfNextLine.Y = sbiex.dwCursorPosition.Y + 1;

        fSuccess = SetConsoleCursorPosition(hOut, sbiex.dwCursorPosition);
        if (CheckLastError(fSuccess, L"SetConsoleScreenBufferInfoEx"))
        {
            Log::Comment(L"Attempt to write (standard WriteConsole) a double-wide character and ensure that it is placed onto the following line, not bisected.");
            DWORD dwWritten = 0;
            const auto wchHiraganaU = L'\x3046';
            const auto wchSpace = L' ';
            fSuccess = WriteConsoleW(hOut, &wchHiraganaU, 1, &dwWritten, nullptr);

            if (CheckLastError(fSuccess, L"WriteConsoleW"))
            {
                VERIFY_ARE_EQUAL(1u, dwWritten, L"We should have only written the one character.");

                // Read the end of line character and the start of the next line.
                // A proper bisect should have left the end of line character empty (a space)
                // and then put the character at the beginning of the next line.

                Log::Comment(L"Confirm that the end of line was left empty to prevent bisect.");
                WCHAR wchBuffer;
                fSuccess = ReadConsoleOutputCharacterW(hOut, &wchBuffer, 1, coordEndOfLine, &dwWritten);
                if (CheckLastError(fSuccess, L"ReadConsoleOutputCharacterW"))
                {
                    VERIFY_ARE_EQUAL(1u, dwWritten, L"We should have only read one character back at the end of the line.");

                    VERIFY_ARE_EQUAL(wchSpace, wchBuffer, L"A space character should have been left at the end of the line.");

                    Log::Comment(L"Confirm that the wide character was written on the next line down instead.");
                    WCHAR wchBuffer2[2];
                    fSuccess = ReadConsoleOutputCharacterW(hOut, wchBuffer2, 2, coordStartOfNextLine, &dwWritten);
                    if (CheckLastError(fSuccess, L"ReadConsoleOutputCharacterW"))
                    {
                        VERIFY_ARE_EQUAL(1u, dwWritten, L"We should have only read one character back at the beginning of the next line.");

                        VERIFY_ARE_EQUAL(wchHiraganaU, wchBuffer2[0], L"The same character we passed in should have been read back.");

                        Log::Comment(L"Confirm that the cursor has advanced past the double wide character.");
                        fSuccess = GetConsoleScreenBufferInfoEx(hOut, &sbiex);
                        if (CheckLastError(fSuccess, L"GetConsoleScreenBufferInfoEx"))
                        {
                            VERIFY_ARE_EQUAL(coordStartOfNextLine.Y, sbiex.dwCursorPosition.Y, L"Cursor has moved down to next line.");
                            VERIFY_ARE_EQUAL(coordStartOfNextLine.X + 2, sbiex.dwCursorPosition.X, L"Cursor has advanced two spaces on next line for double wide character.");

                            // TODO: This bit needs to move into a UIA test
                            /*Log::Comment(L"We can only run the resize test in the v2 console. We'll skip it if it turns out v2 is off.");
                            if (IsV2Console())
                            {
                                Log::Comment(L"Test that the character moves back up when the window is unwrapped. Make the window one larger.");
                                sbiex.srWindow.Right++;
                                sbiex.dwSize.X++;
                                fSuccess = SetConsoleScreenBufferInfoEx(hOut, &sbiex);
                                if (CheckLastError(fSuccess, L"SetConsoleScreenBufferInfoEx"))
                                {
                                    ZeroMemory(wchBuffer2, ARRAYSIZE(wchBuffer2) * sizeof(WCHAR));
                                    Log::Comment(L"Check that the character rolled back up onto the previous line.");
                                    fSuccess = ReadConsoleOutputCharacterW(hOut, wchBuffer2, 2, coordEndOfLine, &dwWritten);
                                    if (CheckLastError(fSuccess, L"ReadConsoleOutputCharacterW"))
                                    {
                                        VERIFY_ARE_EQUAL(1u, dwWritten, L"We should have read 1 character up on the previous line.");

                                        VERIFY_ARE_EQUAL(wchHiraganaU, wchBuffer2[0], L"The character should now be up one line.");

                                        Log::Comment(L"Now shrink the window one more time and make sure the character rolls back down a line.");
                                        sbiex.srWindow.Right--;
                                        sbiex.dwSize.X--;
                                        fSuccess = SetConsoleScreenBufferInfoEx(hOut, &sbiex);
                                        if (CheckLastError(fSuccess, L"SetConsoleScreenBufferInfoEx"))
                                        {
                                            ZeroMemory(wchBuffer2, ARRAYSIZE(wchBuffer2) * sizeof(WCHAR));
                                            Log::Comment(L"Check that the character rolled down onto the next line again.");
                                            fSuccess = ReadConsoleOutputCharacterW(hOut, wchBuffer2, 2, coordStartOfNextLine, &dwWritten);
                                            if (CheckLastError(fSuccess, L"ReadConsoleOutputCharacterW"))
                                            {
                                                VERIFY_ARE_EQUAL(1u, dwWritten, L"We should have read 1 character back down again on the next line.");

                                                VERIFY_ARE_EQUAL(wchHiraganaU, wchBuffer2[0], L"The character should now be down on the 2nd line again.");
                                            }
                                        }
                                    }
                                }
                            }*/
                        }
                    }
                }
            }
        }
    }
}

// The following W versions of the tests check that we can't insert a bisecting cell even
// when we try to force one in by writing cell-by-cell.
// NOTE: This is a change in behavior from the legacy behavior.
// V1 console would allow a lead byte to be stored in the final cell and then display it improperly.
// It would also allow this data to be read back.
// I believe this was a long standing bug because every other API entry fastidiously checked that it wasn't possible to
// "bisect" a cell and all sorts of portions of the rest of the console code try to enforce that bisects across lines can't happen.
// For the most recent revision of the V2 console (approx November 2018), we're trying to make sure that the TextBuffer's internal state
// is always correct at insert (instead of correcting it on every read).
// If it turns out that we are proven wrong in the future and this causes major problems,
// the legacy behavior is to just let it be stored and compensate for it later. (On read in every API but ReadConsoleOutput and in the selection).
void DbcsTests::TestDbcsBisectWriteCellsEndW()
{
    const auto out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFOEX info = { 0 };
    info.cbSize = sizeof(info);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(out, &info));

    CHAR_INFO originalCell;
    originalCell.Char.UnicodeChar = L'\x30a2'; // Japanese full-width katakana A
    originalCell.Attributes = COMMON_LVB_LEADING_BYTE | FOREGROUND_RED;

    SMALL_RECT writeRegion;
    writeRegion.Top = 0;
    writeRegion.Bottom = 0;
    writeRegion.Left = info.dwSize.X - 1;
    writeRegion.Right = info.dwSize.X - 1;

    const auto originalWriteRegion = writeRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(out, &originalCell, { 1, 1 }, { 0, 0 }, &writeRegion));
    VERIFY_ARE_EQUAL(originalWriteRegion, writeRegion);

    auto readRegion = originalWriteRegion;
    const auto originalReadRegion = readRegion;
    CHAR_INFO readCell;

    CHAR_INFO expectedCell;
    expectedCell.Char.UnicodeChar = L' ';
    expectedCell.Attributes = originalCell.Attributes;
    WI_ClearAllFlags(expectedCell.Attributes, COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);

    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(out, &readCell, { 1, 1 }, { 0, 0 }, &readRegion));
    VERIFY_ARE_EQUAL(originalReadRegion, readRegion);

    VERIFY_ARE_NOT_EQUAL(originalCell, readCell);
    VERIFY_ARE_EQUAL(expectedCell, readCell);
}

// This test also reflects a change in the legacy behavior (see above)
void DbcsTests::TestDbcsBisectWriteCellsBeginW()
{
    const auto out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFOEX info = { 0 };
    info.cbSize = sizeof(info);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(out, &info));

    CHAR_INFO originalCell;
    originalCell.Char.UnicodeChar = L'\x30a2';
    originalCell.Attributes = COMMON_LVB_TRAILING_BYTE | FOREGROUND_RED;

    SMALL_RECT writeRegion;
    writeRegion.Top = 0;
    writeRegion.Bottom = 0;
    writeRegion.Left = 0;
    writeRegion.Right = 0;
    const auto originalWriteRegion = writeRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(out, &originalCell, { 1, 1 }, { 0, 0 }, &writeRegion));
    VERIFY_ARE_EQUAL(originalWriteRegion, writeRegion);

    auto readRegion = originalWriteRegion;
    const auto originalReadRegion = readRegion;
    CHAR_INFO readCell;

    CHAR_INFO expectedCell;
    expectedCell.Char.UnicodeChar = L' ';
    expectedCell.Attributes = originalCell.Attributes;
    WI_ClearAllFlags(expectedCell.Attributes, COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);

    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(out, &readCell, { 1, 1 }, { 0, 0 }, &readRegion));
    VERIFY_ARE_EQUAL(originalReadRegion, readRegion);

    VERIFY_ARE_NOT_EQUAL(originalCell, readCell);
    VERIFY_ARE_EQUAL(expectedCell, readCell);
}

void DbcsTests::TestDbcsBisectWriteCellsEndA()
{
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(JAPANESE_CP));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(JAPANESE_CP));

    const auto out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFOEX info = { 0 };
    info.cbSize = sizeof(info);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(out, &info));

    CHAR_INFO originalCell;
    originalCell.Char.AsciiChar = '\x82';
    originalCell.Attributes = COMMON_LVB_LEADING_BYTE | FOREGROUND_RED;

    SMALL_RECT writeRegion;
    writeRegion.Top = 0;
    writeRegion.Bottom = 0;
    writeRegion.Left = info.dwSize.X - 1;
    writeRegion.Right = info.dwSize.X - 1;
    const auto originalWriteRegion = writeRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputA(out, &originalCell, { 1, 1 }, { 0, 0 }, &writeRegion));
    VERIFY_ARE_EQUAL(originalWriteRegion, writeRegion);

    auto readRegion = originalWriteRegion;
    const auto originalReadRegion = readRegion;
    CHAR_INFO readCell;

    CHAR_INFO expectedCell;
    expectedCell.Char.UnicodeChar = L' ';
    expectedCell.Attributes = originalCell.Attributes;
    WI_ClearAllFlags(expectedCell.Attributes, COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);

    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputA(out, &readCell, { 1, 1 }, { 0, 0 }, &readRegion));
    VERIFY_ARE_EQUAL(originalReadRegion, readRegion);

    VERIFY_ARE_NOT_EQUAL(originalCell, readCell);
    VERIFY_ARE_EQUAL(expectedCell, readCell);
}

// This test maintains the legacy behavior for the 932 A codepage route.
void DbcsTests::TestDbcsBisectWriteCellsBeginA()
{
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleCP(JAPANESE_CP));
    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(JAPANESE_CP));

    const auto out = GetStdHandle(STD_OUTPUT_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFOEX info = { 0 };
    info.cbSize = sizeof(info);
    VERIFY_WIN32_BOOL_SUCCEEDED(GetConsoleScreenBufferInfoEx(out, &info));

    CHAR_INFO originalCell;
    originalCell.Char.AsciiChar = '\xA9';
    originalCell.Attributes = COMMON_LVB_TRAILING_BYTE | FOREGROUND_RED;

    SMALL_RECT writeRegion;
    writeRegion.Top = 0;
    writeRegion.Bottom = 0;
    writeRegion.Left = 0;
    writeRegion.Right = 0;
    const auto originalWriteRegion = writeRegion;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputA(out, &originalCell, { 1, 1 }, { 0, 0 }, &writeRegion));
    VERIFY_ARE_EQUAL(originalWriteRegion, writeRegion);

    auto readRegion = originalWriteRegion;
    const auto originalReadRegion = readRegion;
    CHAR_INFO readCell;

    CHAR_INFO expectedCell{};
    expectedCell.Char.AsciiChar = originalCell.Char.AsciiChar;
    expectedCell.Attributes = originalCell.Attributes;
    WI_ClearAllFlags(expectedCell.Attributes, COMMON_LVB_LEADING_BYTE | COMMON_LVB_TRAILING_BYTE);

    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputA(out, &readCell, { 1, 1 }, { 0, 0 }, &readRegion));
    VERIFY_ARE_EQUAL(originalReadRegion, readRegion);

    VERIFY_ARE_NOT_EQUAL(originalCell, readCell);
    VERIFY_ARE_EQUAL(expectedCell, readCell);
}

struct MultibyteInputData
{
    PCWSTR pwszInputText;
    PCSTR pszExpectedText;
};

// clang-format off
const MultibyteInputData MultibyteTestDataSet[] = {
    { L"\x3042", "\x82\xa0" },
    { L"\x3042" L"3", "\x82\xa0\x33" },
    { L"3" L"\x3042", "\x33\x82\xa0" },
    { L"3" L"\x3042" L"\x3044", "\x33\x82\xa0\x82\xa2" },
    { L"3" L"\x3042" L"\x3044" L"\x3042", "\x33\x82\xa0\x82\xa2\x82\xa0" },
    { L"3" L"\x3042" L"\x3044" L"\x3042" L"\x3044", "\x33\x82\xa0\x82\xa2\x82\xa0\x82\xa2" },
};
// clang-format on

void WriteStringToInput(HANDLE hIn, PCWSTR pwszString)
{
    const auto cchString = wcslen(pwszString);
    const auto cRecords = cchString * 2; // We need double the input records for button down then button up.

    const auto irString = new INPUT_RECORD[cRecords];
    VERIFY_IS_NOT_NULL(irString);

    for (size_t i = 0; i < cRecords; i++)
    {
        irString[i].EventType = KEY_EVENT;
        irString[i].Event.KeyEvent.bKeyDown = (i % 2 == 0) ? TRUE : FALSE;
        irString[i].Event.KeyEvent.dwControlKeyState = 0;
        irString[i].Event.KeyEvent.uChar.UnicodeChar = pwszString[i / 2];
        irString[i].Event.KeyEvent.wRepeatCount = 1;
        irString[i].Event.KeyEvent.wVirtualKeyCode = 0;
        irString[i].Event.KeyEvent.wVirtualScanCode = 0;
    }

    DWORD dwWritten;
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputW(hIn, irString, (DWORD)cRecords, &dwWritten));

    VERIFY_ARE_EQUAL(cRecords, dwWritten, L"We should have written the number of records that were sent in by our buffer.");

    delete[] irString;
}

void ReadStringWithGetCh(PCSTR pszExpectedText)
{
    const auto cchString = strlen(pszExpectedText);

    for (size_t i = 0; i < cchString; i++)
    {
        if (!VERIFY_ARE_EQUAL((BYTE)pszExpectedText[i], _getch()))
        {
            break;
        }
    }
}

void ReadStringWithReadConsoleInputAHelper(HANDLE hIn, PCSTR pszExpectedText, size_t cbBuffer)
{
    Log::Comment(String().Format(L"  = Attempting to read back the text with a %d record length buffer. =", cbBuffer));

    // Find out how many bytes we need to read.
    const auto cchExpectedText = strlen(pszExpectedText);

    // Increment read buffer of the size we were told.
    const auto irRead = new INPUT_RECORD[cbBuffer];
    VERIFY_IS_NOT_NULL(irRead);

    // Loop reading and comparing until we've read enough times to get all the text we expect.
    size_t cchRead = 0;

    while (cchRead < cchExpectedText)
    {
        DWORD dwRead;
        if (!VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInputA(hIn, irRead, (DWORD)cbBuffer, &dwRead), L"Attempt to read input into buffer."))
        {
            break;
        }

        VERIFY_IS_GREATER_THAN_OR_EQUAL(dwRead, (DWORD)0, L"Verify we read non-negative bytes.");

        for (size_t i = 0; i < dwRead; i++)
        {
            // We might read more events than the ones we're looking for because some other type of event was
            // inserted into the queue by outside action. Only look at the key down events.
            if (irRead[i].EventType == KEY_EVENT &&
                irRead[i].Event.KeyEvent.bKeyDown == TRUE)
            {
                if (!VERIFY_ARE_EQUAL((BYTE)pszExpectedText[cchRead], (BYTE)irRead[i].Event.KeyEvent.uChar.AsciiChar))
                {
                    break;
                }
                cchRead++;
            }
        }
    }

    delete[] irRead;
}

void ReadStringWithReadConsoleInputA(HANDLE hIn, PCWSTR pwszWriteText, PCSTR pszExpectedText)
{
    // Figure out how long the expected length is.
    const auto cchExpectedText = strlen(pszExpectedText);

    // Test every buffer size variation from 1 to the size of the string.
    for (size_t i = 1; i <= cchExpectedText; i++)
    {
        FlushConsoleInputBuffer(hIn);
        WriteStringToInput(hIn, pwszWriteText);
        ReadStringWithReadConsoleInputAHelper(hIn, pszExpectedText, i);
    }
}

void DbcsTests::TestMultibyteInputRetrieval()
{
    SetConsoleCP(932);

    auto dwCP = GetConsoleCP();
    if (!VERIFY_ARE_EQUAL(JAPANESE_CP, dwCP, L"Ensure input codepage is Japanese."))
    {
        return;
    }

    auto hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (!VERIFY_ARE_NOT_EQUAL(INVALID_HANDLE_VALUE, hIn, L"Get input handle."))
    {
        return;
    }

    const auto cDataSet = ARRAYSIZE(MultibyteTestDataSet);

    // for each item in our test data set...
    for (size_t i = 0; i < cDataSet; i++)
    {
        auto data = MultibyteTestDataSet[i];

        Log::Comment(String().Format(L"=== TEST #%d ===", i));
        Log::Comment(String().Format(L"=== Input '%ws' ===", data.pwszInputText));

        // test by writing the string and reading back the _getch way.
        Log::Comment(L" == SUBTEST A: Use _getch to retrieve. == ");
        FlushConsoleInputBuffer(hIn);
        WriteStringToInput(hIn, data.pwszInputText);
        ReadStringWithGetCh(data.pszExpectedText);

        // test by writing the string and reading back with variable length buffers the ReadConsoleInputA way.
        Log::Comment(L" == SUBTEST B: Use ReadConsoleInputA with variable length buffers to retrieve. == ");
        ReadStringWithReadConsoleInputA(hIn, data.pwszInputText, data.pszExpectedText);
    }

    FlushConsoleInputBuffer(hIn);
}

// This test ensures that two separate WriteConsoleInputA with trailing/leading DBCS are joined (coalesced) into a single wide character.
void DbcsTests::TestMultibyteInputCoalescing()
{
    SetConsoleCP(932);

    const auto in = GetStdHandle(STD_INPUT_HANDLE);
    FlushConsoleInputBuffer(in);

    DWORD count;
    {
        const auto record = SynthesizeKeyEvent(true, 1, 123, 456, 0x82, 789);
        VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputA(in, &record, 1, &count));
    }
    {
        const auto record = SynthesizeKeyEvent(true, 1, 234, 567, 0xA2, 890);
        VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleInputA(in, &record, 1, &count));
    }

    // Asking for 2 records and asserting we only got 1 ensures
    // that we receive the exact number of expected records.
    INPUT_RECORD actual[2];
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleInputW(in, &actual[0], 2, &count));
    VERIFY_ARE_EQUAL(1u, count);

    const auto expected = SynthesizeKeyEvent(true, 1, 123, 456, L'い', 789);
    VERIFY_ARE_EQUAL(expected, actual[0]);
}

void DbcsTests::TestDbcsOneByOne()
{
    const auto hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify output handle is valid.");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(936), L"Ensure output codepage is set to Simplified Chinese 936.");

    // This is Unicode characters U+6D4B U+8BD5 U+4E2D U+6587 in Simplified Chinese Codepage 936.
    // The English translation is "Test Chinese".
    // We write the bytes in hex to prevent storage/interpretation issues by the source control and compiler.
    char test[] = "\xb2\xe2\xca\xd4\xd6\xd0\xce\xc4";

    // Prepare structures for readback.
    COORD coordReadPos = { 0 };
    const auto cchReadBack = 2u;
    char chReadBack[2];
    DWORD dwReadOrWritten = 0u;

    for (size_t i = 0; i < strlen(test); i++)
    {
        const auto fIsLeadByte = (i % 2 == 0);
        Log::Comment(fIsLeadByte ? L"Writing lead byte." : L"Writing trailing byte.");
        VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleA(hOut, &(test[i]), 1u, &dwReadOrWritten, nullptr));
        VERIFY_ARE_EQUAL(1u, dwReadOrWritten, L"Verify the byte was reported written.");

        dwReadOrWritten = 0;
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterA(hOut, chReadBack, cchReadBack, coordReadPos, &dwReadOrWritten), L"Read back character.");
        if (fIsLeadByte)
        {
            Log::Comment(L"Characters should be empty (space) because we only wrote a lead. It should be held for later.");
            VERIFY_ARE_EQUAL((unsigned char)' ', (unsigned char)chReadBack[0]);
            VERIFY_ARE_EQUAL((unsigned char)' ', (unsigned char)chReadBack[1]);
        }
        else
        {
            Log::Comment(L"After trailing is written, character should be valid from Chinese plane (not checking exactly, just that it was composed.");
            VERIFY_IS_LESS_THAN((unsigned char)'\x80', (unsigned char)chReadBack[0]);
            VERIFY_IS_LESS_THAN((unsigned char)'\x80', (unsigned char)chReadBack[1]);
            coordReadPos.X += 2; // advance X for next read back. Move 2 positions because it's a wide char.
        }
    }
}

void DbcsTests::TestDbcsTrailLead()
{
    const auto hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify output handle is valid.");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(936), L"Ensure output codepage is set to Simplified Chinese 936.");

    // This is Unicode characters U+6D4B U+8BD5 U+4E2D U+6587 in Simplified Chinese Codepage 936.
    // The English translation is "Test Chinese".
    // We write the bytes in hex to prevent storage/interpretation issues by the source control and compiler.
    char test[] = "\xb2";
    char test2[] = "\xe2\xca";
    char test3[] = "\xd4\xd6\xd0\xce\xc4";

    // Prepare structures for readback.
    const COORD coordReadPos = { 0 };
    const auto cchReadBack = 8u;
    char chReadBack[9];
    DWORD dwReadOrWritten = 0u;
    DWORD cchTestLength = 0;

    Log::Comment(L"1. Write lead byte only.");
    cchTestLength = (DWORD)strlen(test);
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleA(hOut, test, cchTestLength, &dwReadOrWritten, nullptr), L"Write the string.");
    VERIFY_ARE_EQUAL(cchTestLength, dwReadOrWritten, L"Verify all characters reported as written.");
    dwReadOrWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterA(hOut, chReadBack, 2, coordReadPos, &dwReadOrWritten), L"Read back buffer.");
    Log::Comment(L"Verify nothing is written/displayed yet. The read byte should have been consumed/stored but not yet displayed.");
    VERIFY_ARE_EQUAL((unsigned char)' ', (unsigned char)chReadBack[0]);
    VERIFY_ARE_EQUAL((unsigned char)' ', (unsigned char)chReadBack[1]);

    Log::Comment(L"2. Write trailing and next lead.");
    cchTestLength = (DWORD)strlen(test2);
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleA(hOut, test2, cchTestLength, &dwReadOrWritten, nullptr), L"Write the string.");
    VERIFY_ARE_EQUAL(cchTestLength, dwReadOrWritten, L"Verify all characters reported as written.");
    dwReadOrWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterA(hOut, chReadBack, 4, coordReadPos, &dwReadOrWritten), L"Read back buffer.");
    Log::Comment(L"Verify previous lead and the trailing we just wrote formed a character. The final lead should have been consumed/stored and not yet displayed.");
    VERIFY_ARE_EQUAL((unsigned char)test[0], (unsigned char)chReadBack[0]);
    VERIFY_ARE_EQUAL((unsigned char)test2[0], (unsigned char)chReadBack[1]);
    VERIFY_ARE_EQUAL((unsigned char)' ', (unsigned char)chReadBack[2]);
    VERIFY_ARE_EQUAL((unsigned char)' ', (unsigned char)chReadBack[3]);

    Log::Comment(L"3. Write trailing and finish string.");
    cchTestLength = (DWORD)strlen(test3);
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleA(hOut, test3, cchTestLength, &dwReadOrWritten, nullptr), L"Write the string.");
    VERIFY_ARE_EQUAL(cchTestLength, dwReadOrWritten, L"Verify all characters reported as written.");
    dwReadOrWritten = 0;
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterA(hOut, chReadBack, cchReadBack, coordReadPos, &dwReadOrWritten), L"Read back buffer.");
    Log::Comment(L"Verify everything is displayed now that we've finished it off with the final trailing and rest of the string.");
    VERIFY_ARE_EQUAL((unsigned char)test[0], (unsigned char)chReadBack[0]);
    VERIFY_ARE_EQUAL((unsigned char)test2[0], (unsigned char)chReadBack[1]);
    VERIFY_ARE_EQUAL((unsigned char)test2[1], (unsigned char)chReadBack[2]);
    VERIFY_ARE_EQUAL((unsigned char)test3[0], (unsigned char)chReadBack[3]);
    VERIFY_ARE_EQUAL((unsigned char)test3[1], (unsigned char)chReadBack[4]);
    VERIFY_ARE_EQUAL((unsigned char)test3[2], (unsigned char)chReadBack[5]);
    VERIFY_ARE_EQUAL((unsigned char)test3[3], (unsigned char)chReadBack[6]);
    VERIFY_ARE_EQUAL((unsigned char)test3[4], (unsigned char)chReadBack[7]);
}

void DbcsTests::TestDbcsStdCoutScenario()
{
    const auto hOut = GetStdOutputHandle();
    VERIFY_IS_NOT_NULL(hOut, L"Verify output handle is valid.");

    VERIFY_WIN32_BOOL_SUCCEEDED(SetConsoleOutputCP(936), L"Ensure output codepage is set to Simplified Chinese 936.");

    // This is Unicode characters U+6D4B U+8BD5 U+4E2D U+6587 in Simplified Chinese Codepage 936.
    // The English translation is "Test Chinese".
    // We write the bytes in hex to prevent storage/interpretation issues by the source control and compiler.
    char test[] = "\xb2\xe2\xca\xd4\xd6\xd0\xce\xc4";
    Log::Comment(L"Write string using printf.");
    printf("%s\n", test);

    // Prepare structures for readback.
    COORD coordReadPos = { 0 };
    const auto cchReadBack = (DWORD)strlen(test);
    const auto psReadBack = wil::make_unique_failfast<char[]>(cchReadBack + 1);
    DWORD dwRead = 0;

    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterA(hOut, psReadBack.get(), cchReadBack, coordReadPos, &dwRead), L"Read back printf line.");
    VERIFY_ARE_EQUAL(cchReadBack, dwRead, L"We should have read as many characters as we expected (length of original printed line.)");
    VERIFY_ARE_EQUAL(String(test), String(psReadBack.get()), L"String should match what we wrote.");

    // Clean up and move down a line for next test.
    ZeroMemory(psReadBack.get(), cchReadBack);
    dwRead = 0;
    coordReadPos.Y++;

    Log::Comment(L"Write string using std::cout.");
    std::cout << test << std::endl;

    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputCharacterA(hOut, psReadBack.get(), cchReadBack, coordReadPos, &dwRead), L"Read back std::cout line.");
    VERIFY_ARE_EQUAL(cchReadBack, dwRead, L"We should have read as many characters as we expected (length of original printed line.)");
    VERIFY_ARE_EQUAL(String(test), String(psReadBack.get()), L"String should match what we wrote.");
}

// Read/WriteConsoleOutput allow a user to implement a restricted form of buffer "backup" and "restore".
// But what if the saved region clips ("bisects") a wide character? This test ensures that we restore proper
// wide characters when given an unpaired trailing/leading CHAR_INFO in the first/last column of the given region.
// In other words, writing a trailing CHAR_INFO will also automatically write a leading CHAR_INFO in the preceding cell.
void DbcsTests::TestDbcsBackupRestore()
{
    static_assert(PrepPattern::DoubledW.size() == 16);

    const auto out = GetStdHandle(STD_OUTPUT_HANDLE);

    // We backup/restore 2 lines at once to ensure that it works even then. After all, an incorrect implementation
    // might ignore all but the absolutely first CHAR_INFO instead of handling the first CHAR_INFO *on each row*.
    std::array<CHAR_INFO, 32> expected;
    std::ranges::copy(PrepPattern::DoubledW, expected.begin() + 0);
    std::ranges::copy(PrepPattern::DoubledW, expected.begin() + 16);

    PrepPattern::replaceColorPlaceholders(expected, FOREGROUND_BLUE | FOREGROUND_INTENSITY | BACKGROUND_GREEN);

    // DoubledW will show up like this in the top/left corner of the terminal:
    // +----------------
    // |QいかなZYXWVUTに
    // |QいかなZYXWVUTに
    //
    // Since those 4 Japanese characters probably aren't going to be monospace for you in your editor
    // (as they most likely aren't exactly 2 ASCII characters wide), I'll continue referring to them like this:
    // +----------------
    // |QaabbccZYXWVUTdd
    // |QaabbccZYXWVUTdd
    {
        SMALL_RECT region{ 0, 0, 15, 1 };
        VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(out, expected.data(), { 16, 2 }, {}, &region));
    }

    // Make a "backup" of the viewport. The twist is that our backup region only
    // copies the trailing/leading half of the first/last glyph respectively like so:
    // +----------------
    // |  abbccZYXWVUTd
    std::array<CHAR_INFO, 26> backup{};
    constexpr COORD backupSize{ 13, 2 };
    SMALL_RECT backupRegion{ 2, 0, 14, 1 };
    VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(out, backup.data(), backupSize, {}, &backupRegion));

    // Destroy the text with some narrow ASCII characters, resulting in:
    // +----------------
    // |Qxxxxxxxxxxxxxxx
    // |Qxxxxxxxxxxxxxxx
    {
        DWORD ignored;
        VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(out, L'x', 15, { 1, 0 }, &ignored));
        VERIFY_WIN32_BOOL_SUCCEEDED(FillConsoleOutputCharacterW(out, L'x', 15, { 1, 1 }, &ignored));
    }

    // Restore our "backup". The trailing half of the first wide glyph (indicated as "a" above)
    // as well as the leading half of the last wide glyph ("d"), will automatically get a
    // matching leading/trailing half respectively. In other words, this:
    // +----------------
    // |  abbccZYXWVUTd
    // |  abbccZYXWVUTd
    //
    // turns into this:
    // +----------------
    // | aabbccZYXWVUTdd
    // | aabbccZYXWVUTdd
    //
    // and so we restore this, overwriting all the "x" characters in the process:
    // +----------------
    // |QいかなZYXWVUTに
    // |QいかなZYXWVUTに
    VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(out, backup.data(), backupSize, {}, &backupRegion));

    std::array<CHAR_INFO, 32> infos{};
    {
        SMALL_RECT region{ 0, 0, 15, 1 };
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(out, infos.data(), { 16, 2 }, {}, &region));
    }
    DbcsWriteRead::Verify(expected, infos);
}

// As tested by TestDbcsBackupRestore(), we do want to allow users to write trailers into the buffer, to allow
// for an area of the buffer to be backed up and restored via Read/WriteConsoleOutput. But apart from that use
// case, we'd generally do best to avoid trailers whenever possible, as conhost basically ignored them in the
// past and only rendered leaders. Applications might now be relying on us effectively ignoring trailers.
void DbcsTests::TestInvalidTrailer()
{
    auto expected = PrepPattern::DoubledW;
    auto input = expected;
    decltype(input) output{};

    for (auto& v : input)
    {
        if (WI_IsFlagSet(v.Attributes, COMMON_LVB_TRAILING_BYTE))
        {
            v.Char.UnicodeChar = 0xfffd;
        }
    }

    {
        static constexpr COORD bufferSize{ 16, 1 };
        SMALL_RECT region{ 0, 0, 15, 0 };
        const auto out = GetStdHandle(STD_OUTPUT_HANDLE);
        VERIFY_WIN32_BOOL_SUCCEEDED(WriteConsoleOutputW(out, input.data(), bufferSize, {}, &region));
        VERIFY_WIN32_BOOL_SUCCEEDED(ReadConsoleOutputW(out, output.data(), bufferSize, {}, &region));
    }

    DbcsWriteRead::Verify(expected, output);
}
