/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- TestUtils.h

Abstract:
- This file has helper functions for writing tests for the TerminalCore project.

Author(s):
    Mike Griese (migrie) January-2020
--*/

#include "../../buffer/out/textBuffer.hpp"

namespace TerminalCoreUnitTests
{
    class TestUtils;
};
class TerminalCoreUnitTests::TestUtils
{
public:
    static constexpr std::wstring_view Test100CharsString{
        LR"(!"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_`abcdefghijklmnopqrstuvwxyz{|}~!"#$%&)"
    };

    // Function Description:
    // - Helper function to validate that a number of characters in a row are all
    //   the same. Validates that the next end-start characters are all equal to the
    //   provided string. Will move the provided iterator as it validates. The
    //   caller should ensure that `iter` starts where they would like to validate.
    // Arguments:
    // - expectedChar: The character (or characters) we're expecting
    // - iter: a iterator pointing to the cell we'd like to start validating at.
    // - start: the first index in the range we'd like to validate
    // - end: the last index in the range we'd like to validate
    // Return Value:
    // - <none>
    static void VerifySpanOfText(const wchar_t* const expectedChar,
                                 TextBufferCellIterator& iter,
                                 const int start,
                                 const int end)
    {
        for (int x = start; x < end; x++)
        {
            WEX::TestExecution::SetVerifyOutput settings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
            if (iter->Chars() != expectedChar)
            {
                WEX::Logging::Log::Comment(WEX::Common::NoThrowString().Format(L"character [%d] was mismatched", x));
            }
            VERIFY_ARE_EQUAL(expectedChar, (iter++)->Chars());
        }
        WEX::Logging::Log::Comment(WEX::Common::NoThrowString().Format(
            L"Successfully validated %d characters were '%s'", end - start, expectedChar));
    };

    // Function Description:
    // - Helper function to validate that the next characters pointed to by `iter`
    //   are the provided string. Will increment iter as it walks the provided
    //   string of characters. It will leave `iter` on the first character after the
    //   expectedString.
    // Arguments:
    // - expectedString: The characters we're expecting
    // - iter: a iterator pointing to the cell we'd like to start validating at.
    // Return Value:
    // - <none>
    static void VerifyExpectedString(std::wstring_view expectedString,
                                     TextBufferCellIterator& iter)
    {
        size_t currentCharIndex = 0;
        for (const auto wch : expectedString)
        {
            // This test spews out a lot of verify logging by default because of
            // the loops, so suppress that to only show the failures.
            WEX::TestExecution::SetVerifyOutput settings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);

            wchar_t buffer[]{ wch, L'\0' };
            std::wstring_view view{ buffer, 1 };
            VERIFY_IS_TRUE(iter, L"Ensure iterator is still valid");

            if (view != (iter)->Chars())
            {
                WEX::Logging::Log::Comment(WEX::Common::NoThrowString().Format(L"character [%d] was mismatched", currentCharIndex));
            }

            VERIFY_ARE_EQUAL(view, (iter++)->Chars(), WEX::Common::NoThrowString().Format(L"%s", view.data()));
        }
        WEX::Logging::Log::Comment(WEX::Common::NoThrowString().Format(
            L"Successfully validated %d characters were '%s'", expectedString.size(), expectedString.data()));
    };

    // Function Description:
    // - Helper function to validate that the next characters in the buffer at the
    //   given location are the provided string. Will return an iterator on the
    //   first character after the expectedString.
    // Arguments:
    // - tb: the buffer who's content we should check
    // - expectedString: The characters we're expecting
    // - pos: the starting position in the buffer to check the contents of
    // Return Value:
    // - an iterator on the first character after the expectedString.
    static TextBufferCellIterator VerifyExpectedString(const TextBuffer& tb,
                                                       std::wstring_view expectedString,
                                                       const COORD pos)
    {
        auto iter = tb.GetCellDataAt(pos);
        VerifyExpectedString(expectedString, iter);
        return iter;
    };

    // Function Description:
    // - Replaces all escapes with the printable symbol for that escape
    //   character. This makes log parsing easier for debugging, as the literal
    //   escapes won't be written to the console output.
    // Arguments:
    // - str: the string to escape.
    // Return Value:
    // - A modified version of that string with non-printable characters replaced.
    static std::string ReplaceEscapes(const std::string& str)
    {
        std::string escaped = str;
        auto replaceFn = [&escaped](const std::string& search, const std::string& replace) {
            size_t pos = escaped.find(search, 0);
            while (pos != std::string::npos)
            {
                escaped.replace(pos, search.length(), replace);
                pos = escaped.find(search, pos + replace.length());
            }
        };
        replaceFn("\x1b", "^\x5b"); // ESC
        replaceFn("\x08", "^\x48"); // BS
        replaceFn("\x0A", "^\x4A"); // LF
        replaceFn("\x0D", "^\x4D"); // CR
        return escaped;
    }

    // Function Description:
    // - Replaces all escapes with the printable symbol for that escape
    //   character. This makes log parsing easier for debugging, as the literal
    //   escapes won't be written to the console output.
    // Arguments:
    // - wstr: the string to escape.
    // Return Value:
    // - A modified version of that string with non-printable characters replaced.
    static std::wstring ReplaceEscapes(const std::wstring& wstr)
    {
        std::wstring escaped = wstr;
        std::replace(escaped.begin(), escaped.end(), L'\x1b', L'\x241b'); // ESC
        std::replace(escaped.begin(), escaped.end(), L'\x08', L'\x2408'); // BS
        std::replace(escaped.begin(), escaped.end(), L'\x0A', L'\x240A'); // LF
        std::replace(escaped.begin(), escaped.end(), L'\x0D', L'\x240D'); // CR
        return escaped;
    }

    template<class... T>
    static bool VerifyLineContains(TextBufferCellIterator& actual, T&&... expectedContent)
    {
        auto expected = OutputCellIterator{ std::forward<T>(expectedContent)... };

        WEX::TestExecution::SetVerifyOutput settings(WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures);
        auto charsProcessed = 0;

        while (actual && expected)
        {
            auto actualChars = actual->Chars();
            auto expectedChars = expected->Chars();
            auto actualAttrs = actual->TextAttr();
            auto expectedAttrs = expected->TextAttr();

            auto mismatched = (actualChars != expectedChars || actualAttrs != expectedAttrs);
            if (mismatched)
            {
                Log::Comment(NoThrowString().Format(
                    L"Character or attribute at index %d was mismatched", charsProcessed));
            }

            VERIFY_ARE_EQUAL(expectedChars, actualChars);
            VERIFY_ARE_EQUAL(expectedAttrs, actualAttrs);
            if (mismatched)
            {
                return false;
            }
            ++actual;
            ++expected;
            ++charsProcessed;
        }

        WEX::Logging::Log::Comment(WEX::Common::NoThrowString().Format(
            L"Successfully validated the chars and attrs of %d cells", charsProcessed));
        return true;
    };

    template<class... T>
    static TextBufferCellIterator VerifyLineContains(const TextBuffer& tb, COORD position, T&&... expectedContent)
    {
        auto actual = tb.GetCellLineDataAt(position);
        VerifyLineContains(actual, std::forward<T>(expectedContent)...);
        return actual;
    }
};
