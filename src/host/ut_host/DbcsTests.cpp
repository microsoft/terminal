// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "CommonState.hpp"

#include "globals.h"
#include "../buffer/out/textBuffer.hpp"

#include "dbcs.h"

#include "input.h"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

class DbcsTests
{
    TEST_CLASS(DbcsTests);

    TEST_METHOD(TestUnicodeRasterFontCellMungeOnRead)
    {
        const size_t cchTestSize = 20;

        // create test array of 20 characters
        CHAR_INFO rgci[cchTestSize];

        // pick a color to use for attributes to ensure it's preserved.
        WORD const wAttrTest = FOREGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_INTENSITY;

        // target array will look like
        // abcdeLTLTLTLTLTpqrst
        // where L is a leading half of a double-wide char sequence
        // and T is the trailing half of a double-wide char sequence

        // fill ASCII characters first by looping and
        // incrementing. we'll cover up the middle later
        WCHAR wch = L'a';
        for (size_t i = 0; i < ARRAYSIZE(rgci); i++)
        {
            rgci[i].Char.UnicodeChar = wch;
            rgci[i].Attributes = wAttrTest;
            wch++;
        }

        // we're going to do katakana KA, GA, KI, GI, KU
        // for the double wide characters.
        WCHAR wchDouble = 0x30AB;
        for (size_t i = 5; i < 15; i += 2)
        {
            rgci[i].Char.UnicodeChar = wchDouble;
            rgci[i].Attributes = COMMON_LVB_LEADING_BYTE | wAttrTest;
            rgci[i + 1].Char.UnicodeChar = wchDouble;
            rgci[i + 1].Attributes = COMMON_LVB_TRAILING_BYTE | wAttrTest;
            wchDouble++;
        }

        const gsl::span<CHAR_INFO> buffer(rgci, ARRAYSIZE(rgci));

        // feed it into UnicodeRasterFontCellMungeOnRead to confirm that it is working properly.
        // do it in-place to confirm that it can operate properly in the common case.
        DWORD dwResult = UnicodeRasterFontCellMungeOnRead(buffer);

        // the final length returned should be the same as the length we started with
        if (VERIFY_ARE_EQUAL(ARRAYSIZE(rgci), dwResult, L"Ensure the length claims that we are the same before and after."))
        {
            Log::Comment(L"Ensure the letters are now as expected.");
            // the expected behavior is to reduce the LEADING/TRAILING double copies into a single copy
            WCHAR wchExpected[]{ 'a', 'b', 'c', 'd', 'e', 0x30AB, 0x30AC, 0x30AD, 0x30AE, 0x30AF, 'p', 'q', 'r', 's', 't' };
            for (size_t i = 0; i < ARRAYSIZE(wchExpected); i++)
            {
                VERIFY_ARE_EQUAL(wchExpected[i], rgci[i].Char.UnicodeChar);

                // and simultaneously strip the LEADING/TRAILING attributes
                // no other attributes should be affected (test against color flags we set).
                VERIFY_ARE_EQUAL(wAttrTest, rgci[i].Attributes);
            }

            // and all extra portions of the array should be zeroed.
            for (size_t i = ARRAYSIZE(wchExpected); i < ARRAYSIZE(rgci); i++)
            {
                VERIFY_ARE_EQUAL(rgci[i].Char.UnicodeChar, 0);
                VERIFY_ARE_EQUAL(rgci[i].Attributes, 0);
            }
        }
    }
};
