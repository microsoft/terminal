// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"

#include "../buffer/out/outputCellIterator.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

static constexpr TextAttribute InvalidTextAttribute{ INVALID_COLOR, INVALID_COLOR };

class OutputCellIteratorTests
{
    CommonState* m_state;

    TEST_CLASS(OutputCellIteratorTests);

    TEST_METHOD(CharacterFillDoubleWidth)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const wchar_t wch = L'\x30a2'; // katakana A
        const size_t limit = 5;

        OutputCellIterator it(wch, limit);

        OutputCellView expectedLead({ &wch, 1 },
                                    DbcsAttribute(DbcsAttribute::Attribute::Leading),
                                    InvalidTextAttribute,
                                    TextAttributeBehavior::Current);

        OutputCellView expectedTrail({ &wch, 1 },
                                     DbcsAttribute(DbcsAttribute::Attribute::Trailing),
                                     InvalidTextAttribute,
                                     TextAttributeBehavior::Current);

        for (size_t i = 0; i < limit; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expectedLead, *it);
            it++;
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expectedTrail, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(CharacterFillLimited)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const wchar_t wch = L'Q';
        const size_t limit = 5;

        OutputCellIterator it(wch, limit);

        OutputCellView expected({ &wch, 1 },
                                DbcsAttribute{},
                                InvalidTextAttribute,
                                TextAttributeBehavior::Current);

        for (size_t i = 0; i < limit; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(CharacterFillUnlimited)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const wchar_t wch = L'Q';

        OutputCellIterator it(wch);

        OutputCellView expected({ &wch, 1 },
                                DbcsAttribute{},
                                InvalidTextAttribute,
                                TextAttributeBehavior::Current);

        for (size_t i = 0; i < SHORT_MAX; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_TRUE(it);
    }

    TEST_METHOD(AttributeFillLimited)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        TextAttribute attr;
        attr.SetFromLegacy(FOREGROUND_RED | BACKGROUND_BLUE);

        const size_t limit = 5;

        OutputCellIterator it(attr, limit);

        OutputCellView expected({},
                                {},
                                attr,
                                TextAttributeBehavior::StoredOnly);

        for (size_t i = 0; i < limit; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(AttributeFillUnlimited)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        TextAttribute attr;
        attr.SetFromLegacy(FOREGROUND_RED | BACKGROUND_BLUE);

        OutputCellIterator it(attr);

        OutputCellView expected({},
                                {},
                                attr,
                                TextAttributeBehavior::StoredOnly);

        for (size_t i = 0; i < SHORT_MAX; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_TRUE(it);
    }

    TEST_METHOD(TextAndAttributeFillLimited)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const wchar_t wch = L'Q';

        TextAttribute attr;
        attr.SetFromLegacy(FOREGROUND_RED | BACKGROUND_BLUE);

        const size_t limit = 5;

        OutputCellIterator it(wch, attr, limit);

        OutputCellView expected({ &wch, 1 },
                                {},
                                attr,
                                TextAttributeBehavior::Stored);

        for (size_t i = 0; i < limit; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(TextAndAttributeFillUnlimited)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const wchar_t wch = L'Q';

        TextAttribute attr;
        attr.SetFromLegacy(FOREGROUND_RED | BACKGROUND_BLUE);

        OutputCellIterator it(wch, attr);

        OutputCellView expected({ &wch, 1 },
                                {},
                                attr,
                                TextAttributeBehavior::Stored);

        for (size_t i = 0; i < SHORT_MAX; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_TRUE(it);
    }

    TEST_METHOD(CharInfoFillLimited)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        CHAR_INFO ci;
        ci.Char.UnicodeChar = L'Q';
        ci.Attributes = FOREGROUND_RED | BACKGROUND_BLUE;

        const size_t limit = 5;

        OutputCellIterator it(ci, limit);

        OutputCellView expected({ &ci.Char.UnicodeChar, 1 },
                                {},
                                TextAttribute(ci.Attributes),
                                TextAttributeBehavior::Stored);

        for (size_t i = 0; i < limit; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(CharInfoFillUnlimited)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        CHAR_INFO ci;
        ci.Char.UnicodeChar = L'Q';
        ci.Attributes = FOREGROUND_RED | BACKGROUND_BLUE;

        OutputCellIterator it(ci);

        OutputCellView expected({ &ci.Char.UnicodeChar, 1 },
                                {},
                                TextAttribute(ci.Attributes),
                                TextAttributeBehavior::Stored);

        for (size_t i = 0; i < SHORT_MAX; i++)
        {
            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_TRUE(it);
    }

    TEST_METHOD(StringData)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const std::wstring testText(L"The quick brown fox jumps over the lazy dog.");

        OutputCellIterator it(testText);

        for (const auto& wch : testText)
        {
            OutputCellView expected({ &wch, 1 },
                                    {},
                                    InvalidTextAttribute,
                                    TextAttributeBehavior::Current);

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(FullWidthStringData)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const std::wstring testText(L"\x30a2\x30a3\x30a4\x30a5\x30a6");

        OutputCellIterator it(testText);

        for (const auto& wch : testText)
        {
            auto expected = OutputCellView({ &wch, 1 },
                                           DbcsAttribute(DbcsAttribute::Attribute::Leading),
                                           InvalidTextAttribute,
                                           TextAttributeBehavior::Current);

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;

            expected = OutputCellView({ &wch, 1 },
                                      DbcsAttribute(DbcsAttribute::Attribute::Trailing),
                                      InvalidTextAttribute,
                                      TextAttributeBehavior::Current);

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(StringDataWithColor)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const std::wstring testText(L"The quick brown fox jumps over the lazy dog.");
        TextAttribute color;
        color.SetFromLegacy(FOREGROUND_GREEN | FOREGROUND_INTENSITY);

        OutputCellIterator it(testText, color);

        for (const auto& wch : testText)
        {
            OutputCellView expected({ &wch, 1 },
                                    {},
                                    color,
                                    TextAttributeBehavior::Stored);

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(FullWidthStringDataWithColor)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const std::wstring testText(L"\x30a2\x30a3\x30a4\x30a5\x30a6");
        TextAttribute color;
        color.SetFromLegacy(FOREGROUND_GREEN | FOREGROUND_INTENSITY);

        OutputCellIterator it(testText, color);

        for (const auto& wch : testText)
        {
            auto expected = OutputCellView({ &wch, 1 },
                                           DbcsAttribute(DbcsAttribute::Attribute::Leading),
                                           color,
                                           TextAttributeBehavior::Stored);

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;

            expected = OutputCellView({ &wch, 1 },
                                      DbcsAttribute(DbcsAttribute::Attribute::Trailing),
                                      color,
                                      TextAttributeBehavior::Stored);

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(LegacyColorDataRun)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const std::vector<WORD> colors{ FOREGROUND_GREEN, FOREGROUND_RED | BACKGROUND_BLUE, FOREGROUND_BLUE | FOREGROUND_INTENSITY, BACKGROUND_GREEN };
        const std::basic_string_view<WORD> view{ colors.data(), colors.size() };

        OutputCellIterator it(view, false);

        for (const auto& color : colors)
        {
            auto expected = OutputCellView({},
                                           {},
                                           { color },
                                           TextAttributeBehavior::StoredOnly);

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(LegacyCharInfoRun)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        std::vector<CHAR_INFO> charInfos;

        for (auto i = 0; i < 5; i++)
        {
            CHAR_INFO ci;
            ci.Char.UnicodeChar = static_cast<wchar_t>(L'A' + i);
            ci.Attributes = gsl::narrow<WORD>(i);

            charInfos.push_back(ci);
        }

        const std::basic_string_view<CHAR_INFO> view{ charInfos.data(), charInfos.size() };

        OutputCellIterator it(view);

        for (const auto& ci : charInfos)
        {
            auto expected = OutputCellView({ &ci.Char.UnicodeChar, 1 },
                                           {},
                                           { ci.Attributes },
                                           TextAttributeBehavior::Stored);

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(OutputCellRun)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        std::vector<OutputCell> cells;

        for (auto i = 0; i < 5; i++)
        {
            const std::wstring pair(L"\xd834\xdd1e");
            OutputCell cell(pair, {}, gsl::narrow<WORD>(i));
            cells.push_back(cell);
        }

        const std::basic_string_view<OutputCell> view{ cells.data(), cells.size() };

        OutputCellIterator it(view);

        for (const auto& cell : cells)
        {
            auto expected = OutputCellView(cell.Chars(),
                                           cell.DbcsAttr(),
                                           cell.TextAttr(),
                                           cell.TextAttrBehavior());

            VERIFY_IS_TRUE(it);
            VERIFY_ARE_EQUAL(expected, *it);
            it++;
        }

        VERIFY_IS_FALSE(it);
    }

    TEST_METHOD(DistanceStandard)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const std::wstring testText(L"The quick brown fox jumps over the lazy dog.");

        OutputCellIterator it(testText);

        const auto original = it;

        ptrdiff_t expected = 0;
        for (const auto& wch : testText)
        {
            wch; // unused
            VERIFY_IS_TRUE(it);
            it++;

            expected++;
        }

        VERIFY_IS_FALSE(it);
        VERIFY_ARE_EQUAL(expected, it.GetCellDistance(original));
        VERIFY_ARE_EQUAL(expected, it.GetInputDistance(original));
    }

    TEST_METHOD(DistanceFullWidth)
    {
        SetVerifyOutput settings(VerifyOutputSettings::LogOnlyFailures);

        const std::wstring testText(L"QWER\x30a2\x30a3\x30a4\x30a5\x30a6TYUI");

        OutputCellIterator it(testText);

        const auto original = it;

        ptrdiff_t cellsExpected = 0;
        ptrdiff_t inputExpected = 0;
        for (const auto& wch : testText)
        {
            wch; // unused
            VERIFY_IS_TRUE(it);
            const auto value = *it;
            it++;

            if (value.DbcsAttr().IsLeading() || value.DbcsAttr().IsTrailing())
            {
                VERIFY_IS_TRUE(it);
                it++;
                cellsExpected++;
            }

            cellsExpected++;
            inputExpected++;
        }

        VERIFY_IS_FALSE(it);
        VERIFY_ARE_EQUAL(cellsExpected, it.GetCellDistance(original));
        VERIFY_ARE_EQUAL(inputExpected, it.GetInputDistance(original));
    }
};
