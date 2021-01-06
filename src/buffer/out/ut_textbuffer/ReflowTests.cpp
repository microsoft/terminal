// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"

#include "../textBuffer.hpp"
#include "../../renderer/inc/DummyRenderTarget.hpp"
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
        COORD size;
        std::vector<TestRow> rows;
        COORD cursor;
    };

    struct TestCase
    {
        std::wstring_view name;
        std::vector<TestBuffer> buffers;
    };

    static constexpr auto true_due_to_exact_wrap_bug{ true };

    static const TestCase testCases[] = {
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
                        { L"ABCDE", true_due_to_exact_wrap_bug },
                        { L"F$   ", false }, // EXACT WRAP BUG. $ should be alone on next line.
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
    };

#pragma region TAEF hookup for the test case array above
    struct TaefDataRow : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom | Microsoft::WRL::InhibitFtmBase>, IDataRow>
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

    struct TaefDataSource : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom | Microsoft::WRL::InhibitFtmBase>, IDataSource>
    {
        STDMETHODIMP Advance(IDataRow** ppDataRow) override
        {
            if (_index < std::extent<decltype(testCases)>::value)
            {
                Microsoft::WRL::MakeAndInitialize<TaefDataRow>(ppDataRow, _index++);
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
    auto source{ Microsoft::WRL::Make<TaefDataSource>() };
    return source.CopyTo(ppDataSource);
}

class ReflowTests
{
    TEST_CLASS(ReflowTests);

    static DummyRenderTarget target;
    static std::unique_ptr<TextBuffer> _textBufferFromTestBuffer(const TestBuffer& testBuffer)
    {
        auto buffer = std::make_unique<TextBuffer>(testBuffer.size, TextAttribute{ 0x7 }, 0, target);

        size_t i{};
        for (const auto& testRow : testBuffer.rows)
        {
            auto& row{ buffer->GetRowByOffset(i) };

            auto& charRow{ row.GetCharRow() };
            charRow.SetWrapForced(testRow.wrap);

            size_t j{};
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

    static std::unique_ptr<TextBuffer> _textBufferByReflowingTextBuffer(TextBuffer& originalBuffer, const COORD newSize)
    {
        auto buffer = std::make_unique<TextBuffer>(newSize, TextAttribute{ 0x7 }, 0, target);
        TextBuffer::Reflow(originalBuffer, *buffer, std::nullopt, std::nullopt);
        return buffer;
    }

    static void _compareTextBufferAgainstTestBuffer(const TextBuffer& buffer, const TestBuffer& testBuffer)
    {
        VERIFY_ARE_EQUAL(testBuffer.cursor, buffer.GetCursor().GetPosition());
        VERIFY_ARE_EQUAL(testBuffer.size, buffer.GetSize().Dimensions());

        size_t i{};
        for (const auto& testRow : testBuffer.rows)
        {
            const auto& row{ buffer.GetRowByOffset(i) };

            const auto& charRow{ row.GetCharRow() };

            VERIFY_ARE_EQUAL(testRow.wrap, charRow.WasWrapForced());

            size_t j{};
            NoThrowString indexString;
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

                // casts below are so WEX knows how to log them
                //VERIFY_ARE_EQUAL((unsigned int)ch, (unsigned int)it->Char(), indexString);
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
        TestData::TryGetValue(L"index", i); // index is produced by the TaefDataSource above
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

DummyRenderTarget ReflowTests::target{};
