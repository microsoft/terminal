// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"

#include "CommonState.hpp"

#include "../Ucs2CharRow.hpp"
#include "../Utf8CharRow.hpp"

using namespace WEX::Logging;
using namespace WEX::TestExecution;

class CharRowBaseTests
{
    TEST_CLASS(CharRowBaseTests);

    const size_t rowWidth = 80;
    const Ucs2CharRow::glyph_type ucs2Glyph = L'a';
    // hiragana ka U+304B
    const Utf8CharRow::glyph_type utf8Glyph = { '\xE3', '\x81', '\x8B' };

    const Ucs2CharRow::glyph_type ucs2DefaultGlyph = UNICODE_SPACE;
    const Utf8CharRow::glyph_type utf8DefaultGlyph = { UNICODE_SPACE };

    Ucs2CharRow::string_type ucs2Text = L"Loremipsumdolorsitamet,consecteturadipiscingelit.Nullametrutrummetus.Namquiseratal";
    std::vector<Utf8CharRow::glyph_type> utf8Text;

    Ucs2CharRow ucs2CharRow;
    Utf8CharRow utf8CharRow;

    CharRowBaseTests() :
        ucs2CharRow(rowWidth),
        utf8CharRow(rowWidth)
    {
        ucs2Text.resize(rowWidth);
        while (utf8Text.size() < rowWidth)
        {
            utf8Text.push_back({ 'a' });
            utf8Text.push_back({ '\xE3', '\x81', '\x9B' }); // hiragana se U+305B
            utf8Text.push_back({ '\xD0', '\x94' }); // cyrillic de U+0414
        }
        utf8Text.resize(rowWidth);
    }

    // sets single cell at column to glyph value matching class
    void SetGlyphAt(ICharRow* const pCharRow, const size_t column)
    {
        switch (pCharRow->GetSupportedEncoding())
        {
        case ICharRow::SupportedEncoding::Ucs2:
            static_cast<Ucs2CharRow* const>(pCharRow)->GetGlyphAt(column) = ucs2Glyph;
            break;
        case ICharRow::SupportedEncoding::Utf8:
            static_cast<Utf8CharRow* const>(pCharRow)->GetGlyphAt(column) = utf8Glyph;
            break;
        default:
            VERIFY_IS_TRUE(false);
        }
    }

    // sets cell data for class text data and passed in attrs
    void SetCellData(ICharRow* const pCharRow, const std::vector<DbcsAttribute>& attrs)
    {
        VERIFY_ARE_EQUAL(attrs.size(), rowWidth);
        switch (pCharRow->GetSupportedEncoding())
        {
        case ICharRow::SupportedEncoding::Ucs2:
        {
            auto& cells = static_cast<Ucs2CharRow* const>(pCharRow)->_data;
            for (size_t i = 0; i < cells.size(); ++i)
            {
                cells[i].first = ucs2Text[i];
                cells[i].second = attrs[i];
            }
            break;
        }
        case ICharRow::SupportedEncoding::Utf8:
        {
            auto& cells = static_cast<Utf8CharRow* const>(pCharRow)->_data;
            for (size_t i = 0; i < cells.size(); ++i)
            {
                cells[i].first = utf8Text[i];
                cells[i].second = attrs[i];
            }
            break;
        }
        default:
            VERIFY_IS_TRUE(false);
        }
    }

    TEST_METHOD_SETUP(MethodSetup)
    {
        // reset
        ucs2CharRow.Reset();
        utf8CharRow.Reset();
        // resize
        VERIFY_SUCCEEDED(ucs2CharRow.Resize(rowWidth));
        VERIFY_SUCCEEDED(utf8CharRow.Resize(rowWidth));

        return true;
    }

    TEST_METHOD(TestInitialize)
    {
        Ucs2CharRow row1(rowWidth);
        Utf8CharRow row2(rowWidth);
        VERIFY_ARE_EQUAL(row1.GetSupportedEncoding(), ICharRow::SupportedEncoding::Ucs2);
        VERIFY_ARE_EQUAL(row2.GetSupportedEncoding(), ICharRow::SupportedEncoding::Utf8);

        std::vector<ICharRow*> rows{ &row1, &row2 };
        for (ICharRow* const pCharRow : rows)
        {
            VERIFY_IS_FALSE(pCharRow->WasWrapForced());
            VERIFY_IS_FALSE(pCharRow->WasDoubleBytePadded());
            VERIFY_ARE_EQUAL(pCharRow->size(), rowWidth);

            // check that cell data was initialized correctly
            switch (pCharRow->GetSupportedEncoding())
            {
            case ICharRow::SupportedEncoding::Ucs2:
                for (auto& cell : static_cast<Ucs2CharRow*>(pCharRow)->_data)
                {
                    VERIFY_ARE_EQUAL(cell.first, ucs2DefaultGlyph);
                    VERIFY_IS_TRUE(cell.second.IsSingle());
                }
                break;
            case ICharRow::SupportedEncoding::Utf8:
                for (auto& cell : static_cast<Utf8CharRow*>(pCharRow)->_data)
                {
                    VERIFY_ARE_EQUAL(cell.first, utf8DefaultGlyph);
                    VERIFY_IS_TRUE(cell.second.IsSingle());
                }
                break;
            default:
                VERIFY_IS_TRUE(false);
            }
        }
    }

    TEST_METHOD(TestContainsText)
    {
        std::vector<ICharRow*> rows = { &ucs2CharRow, &utf8CharRow };
        const size_t index = 10;

        for (ICharRow* const pCharRow : rows)
        {
            // After init, should have no text
            VERIFY_IS_FALSE(pCharRow->ContainsText());

            // add some text
            SetGlyphAt(pCharRow, index);

            // should have text
            VERIFY_IS_TRUE(pCharRow->ContainsText());
        }
    }

    TEST_METHOD(TestMeasuring)
    {
        std::vector<ICharRow*> rows = { &ucs2CharRow, &utf8CharRow };

        for (ICharRow* const pCharRow : rows)
        {
            // clang-format off
            std::vector<std::tuple<std::wstring,
                                   std::vector<size_t>, // locations to fill with characters
                                   size_t, // MeasureLeft value
                                   size_t // MeasureRight value
                                   >> testData =
            {
                {
                    L"a row with all whitespace should measure the whole row",
                    {},
                    rowWidth,
                    0
                },

                {
                    L"a character as far left as possible",
                    { 0 },
                    0,
                    1
                },

                {
                    L"a character as far right as possible",
                    { rowWidth - 1 },
                    rowWidth - 1,
                    rowWidth
                },

                {
                    L"a character on the left side",
                    { 10 },
                    10,
                    11
                },

                {
                    L"a character on the right side",
                    { rowWidth - 12 },
                    rowWidth - 12,
                    rowWidth - 11
                },

                {
                    L"characters on both edges",
                    { 0, rowWidth - 1},
                    0,
                    rowWidth
                },

                {
                    L"characters near both edges",
                    { 7, rowWidth - 3 },
                    7,
                    rowWidth - 2
                },
            };
            // clang-format on

            for (auto data : testData)
            {
                Log::Comment(std::get<0>(data).c_str());
                // apply the character changes
                const auto cellLocations = std::get<1>(data);
                for (size_t index : cellLocations)
                {
                    SetGlyphAt(pCharRow, index);
                }

                // test measuring
                VERIFY_ARE_EQUAL(pCharRow->MeasureLeft(), std::get<2>(data));
                VERIFY_ARE_EQUAL(pCharRow->MeasureRight(), std::get<3>(data));

                // reset character changes
                for (size_t index : cellLocations)
                {
                    pCharRow->ClearCell(index);
                }
            }
        }
    }

    TEST_METHOD(TestResize)
    {
        std::vector<ICharRow*> rows = { &ucs2CharRow, &utf8CharRow };

        std::vector<DbcsAttribute> attrs(rowWidth);
        // change a bunch of random dbcs attributes
        for (auto& attr : attrs)
        {
            auto choice = rand() % 2;
            switch (choice)
            {
            case 0:
                attr.SetSingle();
                break;
            case 1:
                attr.SetLeading();
                break;
            case 2:
                attr.SetTrailing();
                break;
            default:
                VERIFY_IS_TRUE(false);
            }
        }

        const size_t smallSize = rowWidth / 2;
        const size_t bigSize = rowWidth * 2;

        for (ICharRow* const pCharRow : rows)
        {
            // fill cells with data
            SetCellData(pCharRow, attrs);

            // resize smaller
            VERIFY_SUCCEEDED(pCharRow->Resize(smallSize));
            VERIFY_ARE_EQUAL(pCharRow->size(), smallSize);

            // resize bigger
            VERIFY_SUCCEEDED(pCharRow->Resize(bigSize));
            VERIFY_ARE_EQUAL(pCharRow->size(), bigSize);

            switch (pCharRow->GetSupportedEncoding())
            {
            case ICharRow::SupportedEncoding::Ucs2:
                // data not clipped should not have changed
                for (size_t i = 0; i < smallSize; ++i)
                {
                    auto cell = static_cast<Ucs2CharRow*>(pCharRow)->_data[i];
                    VERIFY_ARE_EQUAL(cell.first, ucs2Text[i]);
                    VERIFY_ARE_EQUAL(cell.second, attrs[i]);
                }
                // newly added cells should be set to the defaults
                for (size_t i = smallSize + 1; i < bigSize; ++i)
                {
                    auto cell = static_cast<Ucs2CharRow*>(pCharRow)->_data[i];
                    VERIFY_ARE_EQUAL(cell.first, ucs2DefaultGlyph);
                    VERIFY_IS_TRUE(cell.second.IsSingle());
                }
                break;
            case ICharRow::SupportedEncoding::Utf8:
                // data not clipped should not have changed
                for (size_t i = 0; i < smallSize; ++i)
                {
                    auto cell = static_cast<Utf8CharRow*>(pCharRow)->_data[i];
                    VERIFY_ARE_EQUAL(cell.first, utf8Text[i]);
                    VERIFY_ARE_EQUAL(cell.second, attrs[i]);
                }
                // newly added cells should be set to the defaults
                for (size_t i = smallSize + 1; i < bigSize; ++i)
                {
                    auto cell = static_cast<Utf8CharRow*>(pCharRow)->_data[i];
                    VERIFY_ARE_EQUAL(cell.first, utf8DefaultGlyph);
                    VERIFY_IS_TRUE(cell.second.IsSingle());
                }
                break;
            default:
                VERIFY_IS_TRUE(false);
            }
        }
    }

    TEST_METHOD(TestClearCell)
    {
        std::vector<ICharRow*> rows = { &ucs2CharRow, &utf8CharRow };
        std::vector<DbcsAttribute> attrs(rowWidth, DbcsAttribute::Attribute::Leading);

        // generate random cell locations to clear
        std::vector<size_t> eraseIndices;
        for (auto i = 0; i < 10; ++i)
        {
            eraseIndices.push_back(rand() % rowWidth);
        }

        for (ICharRow* pCharRow : rows)
        {
            // fill cells with data
            SetCellData(pCharRow, attrs);

            for (auto index : eraseIndices)
            {
                pCharRow->ClearCell(index);
                switch (pCharRow->GetSupportedEncoding())
                {
                case ICharRow::SupportedEncoding::Ucs2:
                {
                    auto& cell = static_cast<Ucs2CharRow*>(pCharRow)->_data[index];
                    VERIFY_ARE_EQUAL(cell.first, ucs2DefaultGlyph);
                    VERIFY_ARE_EQUAL(cell.second, DbcsAttribute::Attribute::Single);
                    break;
                }
                case ICharRow::SupportedEncoding::Utf8:
                {
                    auto& cell = static_cast<Utf8CharRow*>(pCharRow)->_data[index];
                    VERIFY_ARE_EQUAL(cell.first, utf8DefaultGlyph);
                    VERIFY_ARE_EQUAL(cell.second, DbcsAttribute::Attribute::Single);
                    break;
                }
                default:
                    VERIFY_IS_TRUE(false);
                }
            }
        }
    }

    TEST_METHOD(TestClearGlyph)
    {
        std::vector<ICharRow*> rows = { &ucs2CharRow, &utf8CharRow };
        std::vector<DbcsAttribute> attrs(rowWidth, DbcsAttribute::Attribute::Leading);

        // generate random cell locations to clear
        std::vector<size_t> eraseIndices;
        for (auto i = 0; i < 10; ++i)
        {
            eraseIndices.push_back(rand() % rowWidth);
        }

        for (ICharRow* pCharRow : rows)
        {
            // fill cells with data
            SetCellData(pCharRow, attrs);

            for (auto index : eraseIndices)
            {
                pCharRow->ClearGlyph(index);
                switch (pCharRow->GetSupportedEncoding())
                {
                case ICharRow::SupportedEncoding::Ucs2:
                {
                    auto& cell = static_cast<Ucs2CharRow*>(pCharRow)->_data[index];
                    VERIFY_ARE_EQUAL(cell.first, ucs2DefaultGlyph);
                    VERIFY_ARE_EQUAL(cell.second, DbcsAttribute::Attribute::Leading);
                    break;
                }
                case ICharRow::SupportedEncoding::Utf8:
                {
                    auto& cell = static_cast<Utf8CharRow*>(pCharRow)->_data[index];
                    VERIFY_ARE_EQUAL(cell.first, utf8DefaultGlyph);
                    VERIFY_ARE_EQUAL(cell.second, DbcsAttribute::Attribute::Leading);
                    break;
                }
                default:
                    VERIFY_IS_TRUE(false);
                }
            }
        }
    }

    TEST_METHOD(TestGetText)
    {
        std::vector<ICharRow*> rows = { &ucs2CharRow, &utf8CharRow };
        std::vector<DbcsAttribute> attrs(rowWidth);
        // want to make sure that trailing cells are filtered out
        for (size_t i = 0; i < attrs.size(); ++i)
        {
            if (i % 2 == 0)
            {
                attrs[i].SetLeading();
            }
            else
            {
                attrs[i].SetTrailing();
            }
        }

        for (ICharRow* const pCharRow : rows)
        {
            // fill cells with data
            SetCellData(pCharRow, attrs);

            switch (pCharRow->GetSupportedEncoding())
            {
            case ICharRow::SupportedEncoding::Ucs2:
            {
                Ucs2CharRow::string_type expectedText = L"";
                for (size_t i = 0; i < ucs2Text.size(); ++i)
                {
                    if (i % 2 == 0)
                    {
                        expectedText += ucs2Text[i];
                    }
                }
                VERIFY_ARE_EQUAL(expectedText, static_cast<Ucs2CharRow*>(pCharRow)->GetText());
                break;
            }
            case ICharRow::SupportedEncoding::Utf8:
            {
                Utf8CharRow::string_type expectedText = "";
                for (size_t i = 0; i < utf8Text.size(); ++i)
                {
                    if (i % 2 == 0)
                    {
                        auto glyph = utf8Text[i];
                        for (auto ch : glyph)
                        {
                            expectedText += ch;
                        }
                    }
                }
                VERIFY_ARE_EQUAL(expectedText, static_cast<Utf8CharRow*>(pCharRow)->GetText());
                break;
            }
            default:
                VERIFY_IS_TRUE(false);
            }
        }
    }

    TEST_METHOD(TestIterators)
    {
        std::vector<ICharRow*> rows = { &ucs2CharRow, &utf8CharRow };
        std::vector<DbcsAttribute> attrs(rowWidth, DbcsAttribute::Attribute::Trailing);

        for (ICharRow* const pCharRow : rows)
        {
            // fill cells with data
            SetCellData(pCharRow, attrs);

            // make sure data received from iterators matches data written
            switch (pCharRow->GetSupportedEncoding())
            {
            case ICharRow::SupportedEncoding::Ucs2:
            {
                size_t index = 0;
                const Ucs2CharRow& charRow = *static_cast<const Ucs2CharRow* const>(pCharRow);
                for (Ucs2CharRow::const_iterator it = charRow.cbegin(); it != charRow.cend(); ++it)
                {
                    VERIFY_ARE_EQUAL(ucs2Text[index], it->first);
                    VERIFY_ARE_EQUAL(attrs[index], it->second);
                    ++index;
                }
                break;
            }
            case ICharRow::SupportedEncoding::Utf8:
            {
                size_t index = 0;
                const Utf8CharRow& charRow = *static_cast<const Utf8CharRow* const>(pCharRow);
                for (Utf8CharRow::const_iterator it = charRow.cbegin(); it != charRow.cend(); ++it)
                {
                    VERIFY_ARE_EQUAL(utf8Text[index], it->first);
                    VERIFY_ARE_EQUAL(attrs[index], it->second);
                    ++index;
                }
                break;
            }
            default:
                VERIFY_IS_TRUE(false);
            }
        }
    }
};
