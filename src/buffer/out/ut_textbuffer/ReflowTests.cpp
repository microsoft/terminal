// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../textBuffer.hpp"
#include "../../renderer/inc/DummyRenderer.hpp"
#include "../../types/inc/GlyphWidth.hpp"

#include <IDataSource.h>

template<>
class WEX::TestExecution::VerifyOutputTraits<wchar_t>
{
public:
    static WEX::Common::NoThrowString ToString(const wchar_t& wch)
    {
        return WEX::Common::NoThrowString().Format(L"'%c'", wch);
    }
};

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

namespace
{
    struct TestRow
    {
        std::wstring_view text;
        bool wrap;
    };

    struct TestBuffer
    {
        til::size size;
        std::vector<TestRow> rows;
        til::point cursor;
    };

    struct TestCase
    {
        std::wstring_view name;
        std::vector<TestBuffer> buffers;
    };

    static const TestCase testCases[] = {
        TestCase{
            L"No reflow required",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"AB    ", false },
                        { L"$     ", false },
                        { L"CD    ", false },
                        { L"EFG   ", false },
                        { L"      ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"AB   ", false },
                        { L"$    ", false },
                        { L"CD   ", false },
                        { L"EFG  ", false },
                        { L"     ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
                TestBuffer{
                    { 4, 5 },
                    {
                        { L"AB  ", false },
                        { L"$   ", false },
                        { L"CD  ", false },
                        { L"EFG ", false },
                        { L"    ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
            },
        },
        TestCase{
            L"SBCS, cursor remains in buffer, no circling, no original wrap",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"ABCDE", true },
                        { L"F    ", false },
                        { L"$    ", false },
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 0, 2 }, // cursor on $
                },
                TestBuffer{
                    { 6, 5 }, // grow width back to original
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
                TestBuffer{
                    { 7, 5 }, // grow width wider than original
                    {
                        { L"ABCDEF ", false },
                        { L"$      ", false },
                        { L"       ", false },
                        { L"       ", false },
                        { L"       ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
            },
        },
        TestCase{
            L"SBCS, cursor remains in buffer, no circling, with original wrap",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", true },
                        { L"G$    ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 1, 1 }, // cursor on $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"ABCDE", true },
                        { L"FG$  ", false },
                        { L"     ", false },
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 2, 1 }, // cursor on $
                },
                TestBuffer{
                    { 6, 5 }, // grow width back to original
                    {
                        { L"ABCDEF", true },
                        { L"G$    ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 1, 1 }, // cursor on $
                },
                TestBuffer{
                    { 7, 5 }, // grow width wider than original
                    {
                        { L"ABCDEFG", true },
                        { L"$      ", false },
                        { L"       ", false },
                        { L"       ", false },
                        { L"       ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
            },
        },
        TestCase{
            L"SBCS line padded with spaces (to wrap)",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"AB    ", true }, // AB    $     CD is one long wrapped line
                        { L"$     ", true },
                        { L"CD    ", false },
                        { L"EFG   ", false },
                        { L"      ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
                TestBuffer{
                    { 7, 5 }, // reduce width by 1
                    {
                        { L"AB    $", true },
                        { L"     CD", false }, // CD ends with a newline -> .wrap = false
                        { L"EFG    ", false },
                        { L"       ", false },
                        { L"       ", false },
                    },
                    { 6, 0 }, // cursor on $
                },
                TestBuffer{
                    { 8, 5 },
                    {
                        { L"AB    $ ", true },
                        { L"    CD  ", false },
                        { L"EFG     ", false },
                        { L"        ", false },
                        { L"        ", false },
                    },
                    { 6, 0 }, // cursor on $
                },
            },
        },
        TestCase{
            L"DBCS, cursor remains in buffer, no circling, with original wrap",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        //--0123456--
                        { L"カタカ", true }, // KA TA KA
                        { L"ナ$   ", false }, // NA
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 2, 1 }, // cursor on $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        //--012345--
                        { L"カタ ", true }, // KA TA [FORCED SPACER]
                        { L"カナ$", false }, // KA NA
                        { L"     ", false },
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 4, 1 }, // cursor on $
                },
                TestBuffer{
                    { 6, 5 }, // grow width back to original
                    {
                        //--0123456--
                        { L"カタカ", true }, // KA TA KA
                        { L"ナ$   ", false }, // NA
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 2, 1 }, // cursor on $
                },
                TestBuffer{
                    { 7, 5 }, // grow width wider than original (by one; no visible change!)
                    {
                        //--0123456--
                        { L"カタカ ", true }, // KA TA KA [FORCED SPACER]
                        { L"ナ$    ", false }, // NA
                        { L"       ", false },
                        { L"       ", false },
                        { L"       ", false },
                    },
                    { 2, 1 }, // cursor on $
                },
                TestBuffer{
                    { 8, 5 }, // grow width enough to fit second DBCS
                    {
                        //--01234567--
                        { L"カタカナ", true }, // KA TA KA NA
                        { L"$       ", false },
                        { L"        ", false },
                        { L"        ", false },
                        { L"        ", false },
                    },
                    { 0, 1 }, // cursor on $
                },
            },
        },
        TestCase{
            L"SBCS, cursor remains in buffer, with circling, no original wrap",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"GHIJKL", false },
                        { L"MNOPQR", false },
                        { L"STUVWX", false },
                    },
                    { 0, 1 }, // cursor on $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"$    ", false },
                        { L"GHIJK", true },
                        { L"L    ", false },
                        { L"MNOPQ", true },
                        { L"R    ", false },
                    },
                    { 0, 0 },
                },
                TestBuffer{
                    { 6, 5 }, // going back to 6,5, the data lost has been destroyed
                    {
                        { L"$     ", false },
                        { L"GHIJKL", false },
                        { L"MNOPQR", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 0, 0 },
                },
            },
        },
        TestCase{
            // The cursor is not found during character insertion.
            // Instead, it is found off the right edge of the text. This triggers
            // a separate cursor found codepath in the original algorithm.
            L"SBCS, cursor off rightmost char in non-wrapped line",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 1, 1 }, // cursor *after* $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"ABCDE", true },
                        { L"F    ", false },
                        { L"$    ", false },
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 1, 2 }, // cursor follows space after $ to next line
                },
            },
        },
        TestCase{
            L"SBCS, cursor off rightmost char in wrapped line, which is then pushed off bottom",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", true },
                        { L"GHIJKL", true },
                        { L"MNOPQR", true },
                        { L"STUVWX", true },
                        { L"YZ0 $ ", false },
                    },
                    { 5, 4 }, // cursor *after* $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"FGHIJ", true },
                        { L"KLMNO", true },
                        { L"PQRST", true },
                        { L"UVWXY", true },
                        { L"Z0 $ ", false },
                    },
                    { 4, 4 }, // cursor follows space after $ to newly introduced bottom line
                },
            },
        },
        TestCase{
            L"SBCS, cursor off in space to far right of text (end of buffer content)",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 5, 1 }, // The cursor is 5 columns to the right of the $ (last column).
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"ABCDE", true },
                        { L"F    ", false },
                        // The reflow implementation marks a wrapped cursor as a forced row-wrap (= the row is padded with whitespace), so that when
                        // the buffer is enlarged again, we restore the original cursor position correctly. That's why it says .cursor={5,1} below.
                        { L"$    ", true },
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 0, 3 }, // $ is now at 0,2 and the cursor used to be 5 columns to the right. -> 0,3
                },
                TestBuffer{
                    { 6, 5 }, // grow back to original size
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 5, 1 },
                },
            },
        },
        TestCase{
            L"SBCS, cursor off in space to far right of text (middle of buffer content)",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"BLAH  ", false },
                        { L"BLAH  ", false },
                        { L"      ", false },
                    },
                    { 5, 1 }, // The cursor is 5 columns to the right of the $ (last column).
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"F    ", false },
                        // The reflow implementation pads the row with the cursor with whitespace.
                        // Search for "REFLOW_JANK_CURSOR_WRAP" to find the corresponding code.
                        { L"$    ", true },
                        { L"     ", false },
                        { L"BLAH ", false },
                        { L"BLAH ", false },
                    },
                    { 0, 2 },
                },
                TestBuffer{
                    { 6, 5 }, // grow back to original size
                    {
                        { L"F     ", false },
                        { L"$     ", false },
                        { L"BLAH  ", false },
                        { L"BLAH  ", false },
                        { L"      ", false },
                    },
                    { 5, 1 },
                },
            },
        },
        TestCase{
            // Shrinking the buffer this much forces a multi-line wrap before the cursor
            L"SBCS, cursor off in space to far right of text (end of buffer content), aggressive shrink",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 5, 1 }, // The cursor is 5 columns to the right of the $ (last column).
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"CD", true },
                        { L"EF", false },
                        { L"$ ", true },
                        { L"  ", true },
                        { L"  ", false },
                    },
                    { 1, 4 },
                },
            },
        },
        TestCase{
            L"SBCS, cursor off in space to far right of text (end of buffer content), fully wrapped, aggressive shrink",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", true },
                        { L"$     ", true },
                        { L"      ", true },
                        { L"      ", true },
                        { L"      ", true },
                    },
                    { 5, 1 }, // cursor in space far after $
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                    },
                    { 1, 0 },
                },
            },
        },
        TestCase{
            L"SBCS, cursor off in space to far right of text (middle of buffer content), fully wrapped, aggressive shrink",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", true },
                        { L"$     ", true },
                        { L"      ", true },
                        { L"      ", true },
                        { L"     Q", true },
                    },
                    { 5, 1 }, // cursor in space far after $
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                    },
                    { 1, 0 },
                },
            },
        },
        TestCase{
            L"SBCS, cursor off in space to far right of text (middle of buffer content), partially wrapped, aggressive shrink",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"      ", false },
                        { L"      ", true },
                        { L"     Q", true },
                    },
                    { 5, 1 }, // cursor in space far after $
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"  ", false },
                        { L"  ", false },
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                    },
                    { 1, 0 },
                },
            },
        },
        TestCase{
            // This triggers the cursor being walked forward w/ newlines to maintain
            // distance from the last char in the buffer
            L"SBCS, cursor at end of buffer; otherwise, same as previous test",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"     Q", true },
                        { L"      ", true },
                        { L"      ", true },
                    },
                    { 5, 4 }, // cursor at end of buffer
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", false },
                    },
                    { 1, 4 },
                },
            },
        },
    };

#pragma region TAEF hookup for the test case array above
    struct ArrayIndexTaefAdapterRow : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom | Microsoft::WRL::InhibitFtmBase>, IDataRow>
    {
        HRESULT RuntimeClassInitialize(const size_t index)
        {
            _index = index;
            return S_OK;
        }

        STDMETHODIMP GetTestData(BSTR /*pszName*/, SAFEARRAY** ppData) override
        {
            const auto indexString{ wil::str_printf<std::wstring>(L"%zu", _index) };
            auto safeArray{ SafeArrayCreateVector(VT_BSTR, 0, 1) };
            LONG index{ 0 };
            auto indexBstr{ wil::make_bstr(indexString.c_str()) };
            (void)SafeArrayPutElement(safeArray, &index, indexBstr.release());
            *ppData = safeArray;
            return S_OK;
        }

        STDMETHODIMP GetMetadataNames(SAFEARRAY** ppMetadataNames) override
        {
            *ppMetadataNames = nullptr;
            return S_FALSE;
        }

        STDMETHODIMP GetMetadata(BSTR /*pszName*/, SAFEARRAY** ppData) override
        {
            *ppData = nullptr;
            return S_FALSE;
        }

        STDMETHODIMP GetName(BSTR* ppszRowName) override
        {
            *ppszRowName = nullptr;
            return S_FALSE;
        }

    private:
        size_t _index;
    };

    struct ArrayIndexTaefAdapterSource : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom | Microsoft::WRL::InhibitFtmBase>, IDataSource>
    {
        STDMETHODIMP Advance(IDataRow** ppDataRow) override
        {
            if (_index < std::extent_v<decltype(testCases)>)
            {
                Microsoft::WRL::MakeAndInitialize<ArrayIndexTaefAdapterRow>(ppDataRow, _index++);
            }
            else
            {
                *ppDataRow = nullptr;
            }
            return S_OK;
        }

        STDMETHODIMP Reset() override
        {
            _index = 0;
            return S_OK;
        }

        STDMETHODIMP GetTestDataNames(SAFEARRAY** names) override
        {
            auto safeArray{ SafeArrayCreateVector(VT_BSTR, 0, 1) };
            LONG index{ 0 };
            auto dataNameBstr{ wil::make_bstr(L"index") };
            (void)SafeArrayPutElement(safeArray, &index, dataNameBstr.release());
            *names = safeArray;
            return S_OK;
        }

        STDMETHODIMP GetTestDataType(BSTR /*name*/, BSTR* type) override
        {
            *type = nullptr;
            return S_OK;
        }

    private:
        size_t _index{ 0 };
    };
#pragma endregion
}

extern "C" HRESULT __declspec(dllexport) __cdecl ReflowTestDataSource(IDataSource** ppDataSource, void*)
{
    auto source{ Microsoft::WRL::Make<ArrayIndexTaefAdapterSource>() };
    return source.CopyTo(ppDataSource);
}

class ReflowTests
{
    TEST_CLASS(ReflowTests);

    static DummyRenderer renderer;
    static std::unique_ptr<TextBuffer> _textBufferFromTestBuffer(const TestBuffer& testBuffer)
    {
        auto buffer = std::make_unique<TextBuffer>(testBuffer.size, TextAttribute{ 0x7 }, 0, false, &renderer);

        til::CoordType y = 0;
        for (const auto& testRow : testBuffer.rows)
        {
            auto& row{ buffer->GetMutableRowByOffset(y) };

            row.SetWrapForced(testRow.wrap);

            til::CoordType x = 0;
            for (const auto& ch : testRow.text)
            {
                const til::CoordType width = IsGlyphFullWidth(ch) ? 2 : 1;
                row.ReplaceCharacters(x, width, { &ch, 1 });
                x += width;
            }

            y++;
        }

        buffer->GetCursor().SetPosition(testBuffer.cursor);
        return buffer;
    }

    static std::unique_ptr<TextBuffer> _textBufferByReflowingTextBuffer(TextBuffer& originalBuffer, const til::size newSize)
    {
        auto buffer = std::make_unique<TextBuffer>(newSize, TextAttribute{ 0x7 }, 0, false, &renderer);
        TextBuffer::Reflow(originalBuffer, *buffer);
        return buffer;
    }

    static void _compareTextBufferAgainstTestBuffer(const TextBuffer& buffer, const TestBuffer& testBuffer)
    {
        VERIFY_ARE_EQUAL(testBuffer.cursor, buffer.GetCursor().GetPosition());
        VERIFY_ARE_EQUAL(testBuffer.size, buffer.GetSize().Dimensions());

        til::CoordType y = 0;
        for (const auto& testRow : testBuffer.rows)
        {
            NoThrowString indexString;
            const auto& row{ buffer.GetRowByOffset(y) };

            indexString.Format(L"[Row %d]", y);
            VERIFY_ARE_EQUAL(testRow.wrap, row.WasWrapForced(), indexString);

            til::CoordType x = 0;
            til::CoordType j = 0;
            for (const auto& ch : testRow.text)
            {
                indexString.Format(L"[Cell %d, %d; Text line index %d]", x, y, j);

                if (IsGlyphFullWidth(ch))
                {
                    // Char is full width in test buffer, so
                    // ensure that real buffer is LEAD, TRAIL (ch)
                    VERIFY_ARE_EQUAL(row.DbcsAttrAt(x), DbcsAttribute::Leading, indexString);
                    VERIFY_ARE_EQUAL(ch, row.GlyphAt(x).front(), indexString);
                    ++x;

                    VERIFY_ARE_EQUAL(row.DbcsAttrAt(x), DbcsAttribute::Trailing, indexString);
                    VERIFY_ARE_EQUAL(ch, row.GlyphAt(x).front(), indexString);
                    ++x;
                }
                else
                {
                    VERIFY_ARE_EQUAL(row.DbcsAttrAt(x), DbcsAttribute::Single, indexString);
                    VERIFY_ARE_EQUAL(ch, row.GlyphAt(x).front(), indexString);
                    ++x;
                }

                j++;
            }

            y++;
        }
    }

    TEST_METHOD(TestReflowCases)
    {
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"DataSource", L"Export:ReflowTestDataSource")
        END_TEST_METHOD_PROPERTIES()

        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        WEX::TestExecution::SetVerifyOutput verifyOutputScope{ WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures };

        unsigned int i{};
        TestData::TryGetValue(L"index", i); // index is produced by the ArrayIndexTaefAdapterSource above
        const auto& testCase{ testCases[i] };
        Log::Comment(NoThrowString().Format(L"[%zu.0] Test case \"%.*s\"", i, testCase.name.size(), testCase.name.data()));

        // Create initial text buffer from Buffer 0
        auto textBuffer{ _textBufferFromTestBuffer(testCase.buffers.front()) };
        for (size_t bufferIndex{ 1 }; bufferIndex < testCase.buffers.size(); ++bufferIndex)
        {
            const auto& testBuffer{ til::at(testCase.buffers, bufferIndex) };
            Log::Comment(NoThrowString().Format(L"[%zu.%zu] Resizing to %dx%d", i, bufferIndex, testBuffer.size.width, testBuffer.size.height));

            auto newBuffer{ _textBufferByReflowingTextBuffer(*textBuffer, testBuffer.size) };

            // All future operations are based on the new buffer
            std::swap(textBuffer, newBuffer);

            _compareTextBufferAgainstTestBuffer(*textBuffer, testBuffer);
        }
    }
};

DummyRenderer ReflowTests::renderer{};
