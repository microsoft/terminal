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
        for (const auto wch : expectedString)
        {
            wchar_t buffer[]{ wch, L'\0' };
            std::wstring_view view{ buffer, 1 };
            VERIFY_IS_TRUE(iter, L"Ensure iterator is still valid");
            VERIFY_ARE_EQUAL(view, (iter++)->Chars(), WEX::Common::NoThrowString().Format(L"%s", view.data()));
        }
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
};
