// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../textBuffer.hpp"
#include "../../renderer/inc/DummyRenderer.hpp"
#include "../../types/inc/Utf16Parser.hpp"
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

    static constexpr auto true_due_to_exact_wrap_bug{ true };

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
                    { 0, 1 } // cursor on $
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
                    { 0, 1 } // cursor on $
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
                    { 0, 1 } // cursor on $
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
                    { 0, 1 } // cursor on $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"ABCDE", true },
                        { L"F$   ", false }, // [BUG] EXACT WRAP. $ should be alone on next line.
                        { L"     ", false },
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 1, 1 } // cursor on $
                },
                TestBuffer{
                    { 6, 5 }, // grow width back to original
                    {
                        { L"ABCDEF", true_due_to_exact_wrap_bug },
                        { L"$     ", false },
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 0, 1 } // cursor on $
                },
                TestBuffer{
                    { 7, 5 }, // grow width wider than original
                    {
                        { L"ABCDEF$", true_due_to_exact_wrap_bug }, // EXACT WRAP BUG: $ should be alone on next line
                        { L"       ", false },
                        { L"       ", false },
                        { L"       ", false },
                        { L"       ", false },
                    },
                    { 6, 0 } // cursor on $
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
                    { 1, 1 } // cursor on $
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
                    { 2, 1 } // cursor on $
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
                    { 1, 1 } // cursor on $
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
                    { 0, 1 } // cursor on $
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
                    { 0, 1 } // cursor on $
                },
                TestBuffer{
                    { 7, 5 }, // reduce width by 1
                    {
                        { L"AB    $", true },
                        { L"     CD", true_due_to_exact_wrap_bug },
                        { L"       ", false },
                        { L"EFG    ", false },
                        { L"       ", false },
                    },
                    { 6, 0 } // cursor on $
                },
                TestBuffer{
                    { 8, 5 },
                    {
                        { L"AB    $ ", true },
                        { L"    CD  ", false }, // Goes to false because we hit the end of ..CD
                        { L"EFG     ", false }, // [BUG] EFG moves up due to exact wrap bug above
                        { L"        ", false },
                        { L"        ", false },
                    },
                    { 6, 0 } // cursor on $
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
                    { 2, 1 } // cursor on $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        //--012345--
                        { L"カタ ", true }, // KA TA [FORCED SPACER]
                        { L"カナ$", true_due_to_exact_wrap_bug }, // KA NA
                        { L"     ", false },
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 4, 1 } // cursor on $
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
                    { 2, 1 } // cursor on $
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
                    { 2, 1 } // cursor on $
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
                    { 0, 1 } // cursor on $
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
                    { 0, 1 } // cursor on $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"F$   ", false },
                        { L"GHIJK", true }, // [BUG] We should see GHIJK\n L\n MNOPQ\n R\n
                        { L"LMNOP", true }, // The wrapping here is irregular
                        { L"QRSTU", true },
                        { L"VWX  ", false },
                    },
                    { 1, 1 } // [BUG] cursor moves to 1,1 instead of sticking with the $
                },
                TestBuffer{
                    { 6, 5 }, // going back to 6,5, the data lost has been destroyed
                    {
                        //{ L"F$    ", false }, // [BUG] this line is erroneously destroyed too!
                        { L"GHIJKL", true },
                        { L"MNOPQR", true },
                        { L"STUVWX", true },
                        { L"      ", false },
                        { L"      ", false }, // [BUG] this line is added
                    },
                    { 1, 1 }, // [BUG] cursor does not follow [H], it sticks at 1,1
                },
                TestBuffer{
                    { 7, 5 }, // a number of errors are carried forward from the previous buffer
                    {
                        { L"GHIJKLM", true },
                        { L"NOPQRST", true },
                        { L"UVWX   ", false }, // [BUG] This line loses wrap for some reason
                        { L"       ", false },
                        { L"       ", false },
                    },
                    { 0, 1 }, // [BUG] The cursor moves to 0, 1 now, sticking with the [N] from before
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
                    { 1, 1 } // cursor *after* $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"ABCDE", true },
                        { L"F$   ", false }, // [BUG] EXACT WRAP. $ should be alone on next line.
                        { L"     ", false },
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 2, 1 } // cursor follows space after $ to next line
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
                    { 5, 4 } // cursor *after* $
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
                    { 4, 4 } // cursor follows space after $ to newly introduced bottom line
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
                        //       v cursor
                        { L"$     ", false },
                        //       ^ cursor
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 5, 1 } // cursor in space far after $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"ABCDE", true },
                        { L"F$   ", true }, // [BUG] This line is marked wrapped, and I do not know why
                        //   v cursor
                        { L"     ", false },
                        //   ^ cursor
                        { L"     ", false },
                        { L"     ", false },
                    },
                    { 1, 2 } // cursor stays same linear distance from $
                },
                TestBuffer{
                    { 6, 5 }, // grow back to original size
                    {
                        { L"ABCDEF", true_due_to_exact_wrap_bug },
                        //      v cursor [BUG] cursor does not retain linear distance from $
                        { L"$     ", false },
                        //      ^ cursor
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 4, 1 } // cursor stays same linear distance from $
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
                        //       v cursor
                        { L"$     ", false },
                        //       ^ cursor
                        { L"BLAH  ", false },
                        { L"BLAH  ", false },
                        { L"      ", false },
                    },
                    { 5, 1 } // cursor in space far after $
                },
                TestBuffer{
                    { 5, 5 }, // reduce width by 1
                    {
                        { L"ABCDE", true },
                        { L"F$   ", false },
                        { L"BLAH ", false },
                        { L"BLAH ", true }, // [BUG] this line wraps, no idea why
                        //  v cursor [BUG] cursor erroneously moved to end of all content
                        { L"     ", false },
                        //  ^ cursor
                    },
                    { 0, 4 } },
                TestBuffer{
                    { 6, 5 }, // grow back to original size
                    {
                        { L"ABCDEF", true },
                        { L"$     ", false },
                        { L"BLAH  ", false },
                        //       v cursor [BUG] cursor is pulled up to previous line because it was marked wrapped
                        { L"BLAH  ", false },
                        //       ^ cursor
                        { L"      ", false },
                    },
                    { 5, 3 } },
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
                        //       v cursor
                        { L"$     ", false },
                        //       ^ cursor
                        { L"      ", false },
                        { L"      ", false },
                        { L"      ", false },
                    },
                    { 5, 1 } // cursor in space far after $
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"CD", true },
                        { L"EF", true },
                        { L"$ ", true },
                        { L"  ", true },
                        //   v cursor
                        { L"  ", false },
                        //   ^ cursor
                    },
                    { 1, 4 } },
            },
        },
        TestCase{
            L"SBCS, cursor off in space to far right of text (end of buffer content), fully wrapped, aggressive shrink",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", true },
                        //       v cursor
                        { L"$     ", true },
                        //       ^ cursor
                        { L"      ", true },
                        { L"      ", true },
                        { L"      ", true },
                    },
                    { 5, 1 } // cursor in space far after $
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"EF", true },
                        { L"$ ", true },
                        { L"  ", true },
                        { L"  ", true },
                        //   v cursor [BUG] cursor does not maintain linear distance from $
                        { L"  ", false },
                        //   ^ cursor
                    },
                    { 1, 4 } },
            },
        },
        TestCase{
            L"SBCS, cursor off in space to far right of text (middle of buffer content), fully wrapped, aggressive shrink",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", true },
                        //       v cursor
                        { L"$     ", true },
                        //       ^ cursor
                        { L"      ", true },
                        { L"      ", true },
                        { L"     Q", true },
                    },
                    { 5, 1 } // cursor in space far after $
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                        { L" Q", true },
                        //   v cursor [BUG] cursor jumps to end of world
                        { L"  ", false }, // POTENTIAL [BUG] a whole new blank line is added for the cursor
                        //   ^ cursor
                    },
                    { 1, 4 } },
            },
        },
        TestCase{
            L"SBCS, cursor off in space to far right of text (middle of buffer content), partially wrapped, aggressive shrink",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        //       v cursor
                        { L"$     ", false },
                        //       ^ cursor
                        { L"      ", false },
                        { L"      ", true },
                        { L"     Q", true },
                    },
                    { 5, 1 } // cursor in space far after $
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"  ", true },
                        { L"  ", true },
                        { L"  ", true },
                        { L" Q", true },
                        //  v  cursor [BUG] cursor jumps to different place than fully wrapped case
                        { L"  ", false },
                        //  ^  cursor
                    },
                    { 0, 4 } },
            },
        },
        TestCase{
            // This triggers the cursor being walked forward w/ newlines to maintain
            // distance from the last char in the buffer
            L"SBCS, cursor at end of buffer, otherwise same as previous test",
            {
                TestBuffer{
                    { 6, 5 },
                    {
                        { L"ABCDEF", false },
                        { L"$     ", false },
                        { L"     Q", true },
                        { L"      ", true },
                        //       v cursor
                        { L"      ", true },
                        //       ^ cursor
                    },
                    { 5, 4 } // cursor at end of buffer
                },
                TestBuffer{
                    { 2, 5 }, // reduce width aggressively
                    {
                        { L"  ", true },
                        { L"  ", true },
                        { L" Q", true },
                        { L"  ", false },
                        //  v  cursor [BUG] cursor loses linear distance from Q; is this important?
                        { L"  ", false },
                        //  ^  cursor
                    },
                    { 0, 4 } },
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
            if (_index < std::extent<decltype(testCases)>::value)
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
        auto buffer = std::make_unique<TextBuffer>(testBuffer.size, TextAttribute{ 0x7 }, 0, false, renderer);

        til::CoordType i{};
        for (const auto& testRow : testBuffer.rows)
        {
            auto& row{ buffer->GetRowByOffset(i) };

            auto& charRow{ row.GetCharRow() };
            row.SetWrapForced(testRow.wrap);

            til::CoordType j{};
            for (auto it{ charRow.begin() }; it != charRow.end(); ++it)
            {
                // Yes, we're about to manually create a buffer. It is unpleasant.
                const auto ch{ til::at(testRow.text, j) };
                it->Char() = ch;
                if (IsGlyphFullWidth(ch))
                {
                    it->DbcsAttr().SetLeading();
                    it++;
                    it->Char() = ch;
                    it->DbcsAttr().SetTrailing();
                }
                else
                {
                    it->DbcsAttr().SetSingle();
                }
                j++;
            }
            i++;
        }

        buffer->GetCursor().SetPosition(testBuffer.cursor);
        return buffer;
    }

    static std::unique_ptr<TextBuffer> _textBufferByReflowingTextBuffer(TextBuffer& originalBuffer, const til::size newSize)
    {
        auto buffer = std::make_unique<TextBuffer>(newSize, TextAttribute{ 0x7 }, 0, false, renderer);
        TextBuffer::Reflow(originalBuffer, *buffer, std::nullopt, std::nullopt);
        return buffer;
    }

    static void _compareTextBufferAgainstTestBuffer(const TextBuffer& buffer, const TestBuffer& testBuffer)
    {
        VERIFY_ARE_EQUAL(testBuffer.cursor, buffer.GetCursor().GetPosition());
        VERIFY_ARE_EQUAL(testBuffer.size, buffer.GetSize().Dimensions());

        til::CoordType i{};
        for (const auto& testRow : testBuffer.rows)
        {
            NoThrowString indexString;
            const auto& row{ buffer.GetRowByOffset(i) };

            const auto& charRow{ row.GetCharRow() };

            indexString.Format(L"[Row %d]", i);
            VERIFY_ARE_EQUAL(testRow.wrap, row.WasWrapForced(), indexString);

            til::CoordType j{};
            for (auto it{ charRow.begin() }; it != charRow.end(); ++it)
            {
                indexString.Format(L"[Cell %d, %d; Text line index %d]", it - charRow.begin(), i, j);
                // Yes, we're about to manually create a buffer. It is unpleasant.
                const auto ch{ til::at(testRow.text, j) };
                if (IsGlyphFullWidth(ch))
                {
                    // Char is full width in test buffer, so
                    // ensure that real buffer is LEAD, TRAIL (ch)
                    VERIFY_IS_TRUE(it->DbcsAttr().IsLeading(), indexString);
                    VERIFY_ARE_EQUAL(ch, it->Char(), indexString);

                    it++;
                    VERIFY_IS_TRUE(it->DbcsAttr().IsTrailing(), indexString);
                }
                else
                {
                    VERIFY_IS_TRUE(it->DbcsAttr().IsSingle(), indexString);
                }

                VERIFY_ARE_EQUAL(ch, it->Char(), indexString);
                j++;
            }
            i++;
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
            Log::Comment(NoThrowString().Format(L"[%zu.%zu] Resizing to %dx%d", i, bufferIndex, testBuffer.size.X, testBuffer.size.Y));

            auto newBuffer{ _textBufferByReflowingTextBuffer(*textBuffer, testBuffer.size) };

            // All future operations are based on the new buffer
            std::swap(textBuffer, newBuffer);

            _compareTextBufferAgainstTestBuffer(*textBuffer, testBuffer);
        }
    }
};

DummyRenderer ReflowTests::renderer{};
