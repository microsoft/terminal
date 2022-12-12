// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "../../inc/consoletaeftemplates.hpp"
#include "CommonState.hpp"

#include "uiaTextRange.hpp"
#include "../types/ScreenInfoUiaProviderBase.h"
#include "../../../buffer/out/textBuffer.hpp"
#include "../types/UiaTracing.h"

#include <IDataSource.h>

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::WRL;

using namespace Microsoft::Console::Interactivity::Win32;

static constexpr til::point point_offset_by_char(const til::point start, const til::rect& bounds, til::CoordType amt)
{
    auto pos_x = start.x;
    auto pos_y = start.y;
    while (amt != 0)
    {
        if (amt > 0)
        {
            if (pos_x == bounds.left && pos_y == bounds.bottom)
            {
                // end exclusive --> can't move any more
                break;
            }
            else if (pos_x == bounds.right - 1)
            {
                // right boundary --> wrap
                pos_x = bounds.left;
                ++pos_y;
            }
            else
            {
                // standard move
                ++pos_x;
            }
            --amt;
        }
        else
        {
            if (pos_x == bounds.left && pos_y == bounds.top)
            {
                // origin --> can't move any more
                break;
            }
            else if (pos_x == bounds.left)
            {
                // left boundary --> wrap
                pos_x = bounds.right - 1;
                --pos_y;
            }
            else
            {
                // standard move
                --pos_x;
            }
            ++amt;
        }
    }
    return { pos_x, pos_y };
}

static constexpr til::point point_offset_by_line(const til::point start, const til::rect& bounds, til::CoordType amt)
{
    // X = left boundary for UIA
    auto pos_x = bounds.left;
    auto pos_y = start.y;
    while (amt != 0)
    {
        if (amt > 0)
        {
            if (pos_y == bounds.bottom + 1)
            {
                break;
            }
            else
            {
                ++pos_y;
            }
            --amt;
        }
        else
        {
            if (pos_y == bounds.top)
            {
                break;
            }
            else
            {
                --pos_y;
            }
            ++amt;
        }
    }
    return { pos_x, pos_y };
}

// IMPORTANT: reference this _after_ defining point_offset_by_XXX. We need it for some definitions
#include "GeneratedUiaTextRangeMovementTests.g.cpp"

namespace
{
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
            *ppszRowName = wil::make_bstr(s_movementTests[_index].name.data()).release();
            return S_OK;
        }

    private:
        size_t _index;
    };

    struct ArrayIndexTaefAdapterSource : public Microsoft::WRL::RuntimeClass<Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom | Microsoft::WRL::InhibitFtmBase>, IDataSource>
    {
        STDMETHODIMP Advance(IDataRow** ppDataRow) override
        {
            if (_index < s_movementTests.size())
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

extern "C" HRESULT __declspec(dllexport) __cdecl GeneratedMovementTestDataSource(IDataSource** ppDataSource, void*)
{
    auto source{ Microsoft::WRL::Make<ArrayIndexTaefAdapterSource>() };
    return source.CopyTo(ppDataSource);
}

// UiaTextRange takes an object that implements
// IRawElementProviderSimple as a constructor argument. Making a real
// one would involve setting up the window which we don't want to do
// for unit tests so instead we'll use this one. We don't care about
// it not doing anything for its implementation because it is not used
// during the unit tests below.
class DummyElementProvider final : public ScreenInfoUiaProviderBase
{
public:
    IFACEMETHODIMP Navigate(_In_ NavigateDirection /*direction*/,
                            _COM_Outptr_result_maybenull_ IRawElementProviderFragment** /*ppProvider*/) override
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP get_BoundingRectangle(_Out_ UiaRect* /*pRect*/) override
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** /*ppProvider*/) override
    {
        return E_NOTIMPL;
    }

    void ChangeViewport(const til::inclusive_rect& /*NewWindow*/)
    {
        return;
    }

    HRESULT GetSelectionRange(_In_ IRawElementProviderSimple* /*pProvider*/, const std::wstring_view /*wordDelimiters*/, _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** /*ppUtr*/) override
    {
        return E_NOTIMPL;
    }

    // degenerate range
    HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const /*pProvider*/, const std::wstring_view /*wordDelimiters*/, _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** /*ppUtr*/) override
    {
        return E_NOTIMPL;
    }

    // degenerate range at cursor position
    HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const /*pProvider*/,
                            const Cursor& /*cursor*/,
                            const std::wstring_view /*wordDelimiters*/,
                            _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** /*ppUtr*/) override
    {
        return E_NOTIMPL;
    }

    // specific endpoint range
    HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const /*pProvider*/,
                            const til::point /*start*/,
                            const til::point /*end*/,
                            const std::wstring_view /*wordDelimiters*/,
                            _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** /*ppUtr*/) override
    {
        return E_NOTIMPL;
    }

    // range from a UiaPoint
    HRESULT CreateTextRange(_In_ IRawElementProviderSimple* const /*pProvider*/,
                            const UiaPoint /*point*/,
                            const std::wstring_view /*wordDelimiters*/,
                            _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** /*ppUtr*/) override
    {
        return E_NOTIMPL;
    }
};

class UiaTextRangeTests
{
    TEST_CLASS(UiaTextRangeTests);

    CommonState* _state;
    DummyElementProvider _dummyProvider;
    SCREEN_INFORMATION* _pScreenInfo;
    TextBuffer* _pTextBuffer;
    UiaTextRange* _range;
    Microsoft::Console::Render::IRenderData* _pUiaData;

    struct ExpectedResult
    {
        int moveAmt;
        til::point start;
        til::point end;
    };

    struct MoveTest
    {
        std::wstring comment;
        til::point start;
        til::point end;
        int moveAmt;
        ExpectedResult expected;
    };

    struct MoveEndpointTest
    {
        std::wstring comment;
        til::point start;
        til::point end;
        int moveAmt;
        TextPatternRangeEndpoint endpoint;
        ExpectedResult expected;
    };

    struct ScrollTest
    {
        std::wstring comment;
        til::CoordType yPos;
    };

    static constexpr const wchar_t* toString(TextUnit unit) noexcept
    {
        // if a format is not supported, it goes to the next largest text unit
        switch (unit)
        {
        case TextUnit_Character:
            return L"Character";
        case TextUnit_Format:
        case TextUnit_Word:
            return L"Word";
        case TextUnit_Line:
            return L"Line";
        case TextUnit_Paragraph:
        case TextUnit_Page:
        case TextUnit_Document:
        default:
            return L"Document";
        }
    };

    TEST_METHOD_SETUP(MethodSetup)
    {
        auto& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        // set up common state
        _state = new CommonState();
        _state->PrepareGlobalFont();
        _state->PrepareGlobalScreenBuffer();
        _state->PrepareNewTextBufferInfo();

        // set up pointers
        _pScreenInfo = &gci.GetActiveOutputBuffer();
        _pTextBuffer = &_pScreenInfo->GetTextBuffer();
        _pUiaData = &gci.renderData;

        // GH#6986: document end now limits the navigation to be
        // within the document end bounds _as opposed to_ the buffer bounds.
        // As a result, let's populate the buffer partially to define a document end.
        // Additionally, add spaces to create "words" in the buffer.

        // LOAD BEARING: make sure we fill it halfway so that we can reuse most of
        // the variables from the generated tests.

        // fill first half of text buffer with text
        for (auto i = 0; i < _pTextBuffer->TotalRowCount() / 2; ++i)
        {
            const std::wstring_view glyph{ i % 2 == 0 ? L" " : L"X" };
            auto& row = _pTextBuffer->GetRowByOffset(i);
            const auto width = row.size();

            for (uint16_t x = 0; x < width; ++x)
            {
                row.ReplaceCharacters(x, 1, glyph);
            }
        }

        return true;
    }

    TEST_METHOD_CLEANUP(MethodCleanup)
    {
        _state->CleanupNewTextBufferInfo();
        _state->CleanupGlobalScreenBuffer();
        _state->CleanupGlobalFont();
        delete _state;
        delete _range;

        _pScreenInfo = nullptr;
        _pTextBuffer = nullptr;
        _pUiaData = nullptr;
        return true;
    }

    TEST_METHOD(DegenerateRangesDetected)
    {
        // make a degenerate range and verify that it reports degenerate
        Microsoft::WRL::ComPtr<UiaTextRange> degenerate;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&degenerate,
                                                                        _pUiaData,
                                                                        &_dummyProvider,
                                                                        origin,
                                                                        origin));
        VERIFY_IS_TRUE(degenerate->IsDegenerate());
        VERIFY_ARE_EQUAL(degenerate->_start, degenerate->_end);

        // make a non-degenerate range and verify that it reports as such
        const auto end{ point_offset_by_char(origin, bufferSize, 1) };
        Microsoft::WRL::ComPtr<UiaTextRange> notDegenerate;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&notDegenerate,
                                                                        _pUiaData,
                                                                        &_dummyProvider,
                                                                        origin,
                                                                        end));
        VERIFY_IS_FALSE(notDegenerate->IsDegenerate());
        VERIFY_ARE_NOT_EQUAL(notDegenerate->_start, notDegenerate->_end);
    }

    TEST_METHOD(CompareRange)
    {
        Microsoft::WRL::ComPtr<UiaTextRange> utr1;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr1,
                                                                        _pUiaData,
                                                                        &_dummyProvider,
                                                                        origin,
                                                                        origin));

        // utr2 initialized to have the same start/end as utr1
        Microsoft::WRL::ComPtr<ITextRangeProvider> utr2;
        THROW_IF_FAILED(utr1->Clone(&utr2));

        BOOL comparison;
        Log::Comment(L"_start and _end should match");
        THROW_IF_FAILED(utr1->Compare(utr2.Get(), &comparison));
        VERIFY_IS_TRUE(comparison);

        // utr2 redefined to have different end from utr1
        const auto end{ point_offset_by_char(origin, bufferSize, 2) };
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr2,
                                                                        _pUiaData,
                                                                        &_dummyProvider,
                                                                        origin,
                                                                        end));

        Log::Comment(L"_end is different");
        THROW_IF_FAILED(utr1->Compare(utr2.Get(), &comparison));
        VERIFY_IS_FALSE(comparison);
    }

    TEST_METHOD(CompareEndpoints)
    {
        Microsoft::WRL::ComPtr<UiaTextRange> utr1;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr1,
                                                                        _pUiaData,
                                                                        &_dummyProvider,
                                                                        origin,
                                                                        origin));

        Microsoft::WRL::ComPtr<ITextRangeProvider> utr2;
        THROW_IF_FAILED(utr1->Clone(&utr2));

        int comparison;
        Log::Comment(L"For a degenerate range, comparing _start and _end should return 0");
        VERIFY_IS_TRUE(utr1->IsDegenerate());
        THROW_IF_FAILED(utr1->CompareEndpoints(TextPatternRangeEndpoint_Start, utr1.Get(), TextPatternRangeEndpoint_End, &comparison));

        Log::Comment(L"_start and _end should match");
        THROW_IF_FAILED(utr1->CompareEndpoints(TextPatternRangeEndpoint_Start, utr2.Get(), TextPatternRangeEndpoint_Start, &comparison));
        VERIFY_IS_TRUE(comparison == 0);
        THROW_IF_FAILED(utr1->CompareEndpoints(TextPatternRangeEndpoint_End, utr2.Get(), TextPatternRangeEndpoint_End, &comparison));
        VERIFY_IS_TRUE(comparison == 0);

        // utr2 redefined to have different end from utr1
        const auto end{ point_offset_by_char(origin, bufferSize, 2) };
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr2,
                                                                        _pUiaData,
                                                                        &_dummyProvider,
                                                                        origin,
                                                                        end));

        Log::Comment(L"_start should match");
        THROW_IF_FAILED(utr1->CompareEndpoints(TextPatternRangeEndpoint_Start, utr2.Get(), TextPatternRangeEndpoint_Start, &comparison));
        VERIFY_IS_TRUE(comparison == 0);

        Log::Comment(L"_start and end should be 2 units apart. Sign depends on order of comparison.");
        THROW_IF_FAILED(utr1->CompareEndpoints(TextPatternRangeEndpoint_End, utr2.Get(), TextPatternRangeEndpoint_End, &comparison));
        VERIFY_IS_TRUE(comparison == -2);
        THROW_IF_FAILED(utr2->CompareEndpoints(TextPatternRangeEndpoint_End, utr1.Get(), TextPatternRangeEndpoint_End, &comparison));
        VERIFY_IS_TRUE(comparison == 2);
    }

    TEST_METHOD(ExpandToEnclosingUnit)
    {
        // Let's start by filling the text buffer with something useful:
        for (auto i = 0; i < _pTextBuffer->TotalRowCount(); ++i)
        {
            auto& row = _pTextBuffer->GetRowByOffset(i);
            const auto width = row.size();

            for (uint16_t x = 0; x < width; ++x)
            {
                const std::wstring_view glyph{ x % 5 == 0 ? L" " : L"x" };
                row.ReplaceCharacters(x, 1, glyph);
            }
        }

        // According to https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-implementingtextandtextrange#manipulating-a-text-range-by-text-unit
        // there are 9 examples of how ExpandToEnclosingUnit should behave. See the diagram there for reference.
        // Some of the relevant text has been copied below...
        // 1-2) If the text range starts at the beginning of a text unit
        //      and ends at the beginning of, or before, the next text unit
        //      boundary, the ending endpoint is moved to the next text unit boundary
        // 3-4) If the text range starts at the beginning of a text unit
        //      and ends at, or after, the next unit boundary,
        //      the ending endpoint stays or is moved backward to
        //      the next unit boundary after the starting endpoint
        // NOTE: If there is more than one text unit boundary between
        //       the starting and ending endpoints, the ending endpoint
        //       is moved backward to the next unit boundary after
        //       the starting endpoint, resulting in a text range that is
        //       one text unit in length.
        // 5-8) If the text range starts in a middle of the text unit,
        //      the starting endpoint is moved backward to the beginning
        //      of the text unit, and the ending endpoint is moved forward
        //      or backward, as necessary, to the next unit boundary
        //      after the starting endpoint
        // 9) (same as 1) If the text range starts and ends at the beginning of
        //     a text unit boundary, the ending endpoint is moved to the next text unit boundary

        // We will abstract these tests so that we can define the beginning and end of a text unit boundary,
        // based on the text unit we are testing
        constexpr TextUnit supportedUnits[] = { TextUnit_Character, TextUnit_Word, TextUnit_Line, TextUnit_Document };

        struct TextUnitBoundaries
        {
            til::point start;
            til::point end;
        };

        const std::map<TextUnit, TextUnitBoundaries> textUnitBoundaries = {
            { TextUnit_Character,
              TextUnitBoundaries{
                  { 0, 0 },
                  { 1, 0 } } },
            { TextUnit_Word,
              TextUnitBoundaries{
                  { 1, 0 },
                  { 6, 0 } } },
            { TextUnit_Line,
              TextUnitBoundaries{
                  { 0, 0 },
                  { 0, 1 } } },
            { TextUnit_Document,
              TextUnitBoundaries{
                  { 0, 0 },
                  _pTextBuffer->GetSize().EndExclusive() } }
        };

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        auto verifyExpansion = [&](TextUnit textUnit, til::point utrStart, til::point utrEnd) {
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr,
                                                                            _pUiaData,
                                                                            &_dummyProvider,
                                                                            utrStart,
                                                                            utrEnd));
            THROW_IF_FAILED(utr->ExpandToEnclosingUnit(textUnit));

            const auto boundaries = textUnitBoundaries.at(textUnit);
            VERIFY_ARE_EQUAL(utr->GetEndpoint(TextPatternRangeEndpoint_Start), boundaries.start);
            VERIFY_ARE_EQUAL(utr->GetEndpoint(TextPatternRangeEndpoint_End), boundaries.end);
        };

        for (auto textUnit : supportedUnits)
        {
            const auto boundaries = textUnitBoundaries.at(textUnit);

            // Test 1
            Log::Comment(NoThrowString().Format(L"%s - Test 1", toString(textUnit)));
            verifyExpansion(textUnit, boundaries.start, boundaries.start);

            // Test 2 (impossible for TextUnit_Character)
            if (textUnit != TextUnit_Character)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 2", toString(textUnit)));
                const til::point end = { boundaries.start.x + 1, boundaries.start.y };
                verifyExpansion(textUnit, boundaries.start, end);
            }

            // Test 3
            Log::Comment(NoThrowString().Format(L"%s - Test 3", toString(textUnit)));
            verifyExpansion(textUnit, boundaries.start, boundaries.end);

            // Test 4 (impossible for TextUnit_Character and TextUnit_Document)
            if (textUnit != TextUnit_Character && textUnit != TextUnit_Document)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 4", toString(textUnit)));
                const til::point end = { boundaries.end.x + 1, boundaries.end.y };
                verifyExpansion(textUnit, boundaries.start, end);
            }

            // Test 5 (impossible for TextUnit_Character)
            if (textUnit != TextUnit_Character)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 5", toString(textUnit)));
                const til::point start = { boundaries.start.x + 1, boundaries.start.y };
                verifyExpansion(textUnit, start, start);
            }

            // Test 6 (impossible for TextUnit_Character)
            if (textUnit != TextUnit_Character)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 6", toString(textUnit)));
                const til::point start = { boundaries.start.x + 1, boundaries.start.y };
                const til::point end = { start.x + 1, start.y };
                verifyExpansion(textUnit, start, end);
            }

            // Test 7 (impossible for TextUnit_Character)
            if (textUnit != TextUnit_Character)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 7", toString(textUnit)));
                const til::point start = { boundaries.start.x + 1, boundaries.start.y };
                verifyExpansion(textUnit, start, boundaries.end);
            }

            // Test 8 (impossible for TextUnit_Character and TextUnit_Document)
            if (textUnit != TextUnit_Character && textUnit != TextUnit_Document)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 8", toString(textUnit)));
                const til::point start = { boundaries.start.x + 1, boundaries.start.y };
                const til::point end = { boundaries.end.x + 1, boundaries.end.y };
                verifyExpansion(textUnit, start, end);
            }
        }
    }

    TEST_METHOD(MoveEndpointByRange)
    {
        const til::point start{ 0, 1 };
        const til::point end{ 1, 2 };
        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr,
                                                                        _pUiaData,
                                                                        &_dummyProvider,
                                                                        start,
                                                                        end));

        Microsoft::WRL::ComPtr<UiaTextRange> target;

        auto resetTargetUTR = [&]() {
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&target,
                                                                            _pUiaData,
                                                                            &_dummyProvider,
                                                                            origin,
                                                                            origin));
        };

        Log::Comment(L"Move target's end to utr1's start");
        {
            resetTargetUTR();
            THROW_IF_FAILED(target->MoveEndpointByRange(TextPatternRangeEndpoint_End,
                                                        utr.Get(),
                                                        TextPatternRangeEndpoint_Start));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_Start), origin);
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_End), utr->GetEndpoint(TextPatternRangeEndpoint_Start));
        }

        Log::Comment(L"Move target's start/end to utr1's start/end respectively");
        {
            resetTargetUTR();
            THROW_IF_FAILED(target->MoveEndpointByRange(TextPatternRangeEndpoint_End,
                                                        utr.Get(),
                                                        TextPatternRangeEndpoint_End));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_Start), origin);
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_End), utr->GetEndpoint(TextPatternRangeEndpoint_End));

            THROW_IF_FAILED(target->MoveEndpointByRange(TextPatternRangeEndpoint_Start,
                                                        utr.Get(),
                                                        TextPatternRangeEndpoint_Start));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_Start), utr->GetEndpoint(TextPatternRangeEndpoint_Start));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_End), utr->GetEndpoint(TextPatternRangeEndpoint_End));
        }

        Log::Comment(L"(Clone utr1) Collapse onto itself");
        {
            // Move start to end
            ComPtr<ITextRangeProvider> temp;
            THROW_IF_FAILED(utr->Clone(&temp));
            target = static_cast<UiaTextRange*>(temp.Get());
            THROW_IF_FAILED(target->MoveEndpointByRange(TextPatternRangeEndpoint_Start,
                                                        target.Get(),
                                                        TextPatternRangeEndpoint_End));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_Start), utr->GetEndpoint(TextPatternRangeEndpoint_End));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_End), utr->GetEndpoint(TextPatternRangeEndpoint_End));

            // Move end to start
            THROW_IF_FAILED(utr->Clone(&temp));
            target = static_cast<UiaTextRange*>(temp.Get());
            THROW_IF_FAILED(target->MoveEndpointByRange(TextPatternRangeEndpoint_End,
                                                        target.Get(),
                                                        TextPatternRangeEndpoint_Start));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_Start), utr->GetEndpoint(TextPatternRangeEndpoint_Start));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_End), utr->GetEndpoint(TextPatternRangeEndpoint_Start));
        }

        Log::Comment(L"Cross endpoints (force degenerate range)");
        {
            // move start past end
            resetTargetUTR();
            THROW_IF_FAILED(target->MoveEndpointByRange(TextPatternRangeEndpoint_Start,
                                                        utr.Get(),
                                                        TextPatternRangeEndpoint_End));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_Start), utr->GetEndpoint(TextPatternRangeEndpoint_End));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_End), utr->GetEndpoint(TextPatternRangeEndpoint_End));
            VERIFY_IS_TRUE(target->IsDegenerate());

            // move end past start
            THROW_IF_FAILED(target->MoveEndpointByRange(TextPatternRangeEndpoint_End,
                                                        utr.Get(),
                                                        TextPatternRangeEndpoint_Start));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_Start), utr->GetEndpoint(TextPatternRangeEndpoint_Start));
            VERIFY_ARE_EQUAL(target->GetEndpoint(TextPatternRangeEndpoint_End), utr->GetEndpoint(TextPatternRangeEndpoint_Start));
            VERIFY_IS_TRUE(target->IsDegenerate());
        }
    }

    TEST_METHOD(CanMoveByCharacter)
    {
        const auto lastColumnIndex = _pScreenInfo->GetBufferSize().RightInclusive();

        // GH#6986: This is used as the "end of the buffer" to help screen readers run faster
        //          instead of parsing through thousands of empty lines of text.
        const til::point documentEnd{ _pTextBuffer->GetSize().Left(), _pTextBuffer->GetLastNonSpaceCharacter().y + 1 };

        // clang-format off
        const std::vector<MoveTest> testData
        {
            MoveTest{
                L"can move backward within a row",
                { 1, 0 },
                { 2, 0 },
                -1,
                {
                    -1,
                    {0, 0},
                    {1, 0}
                }
            },

            MoveTest{
                L"can move forward in a row",
                { 1, 2 },
                { 5, 4 },
                5,
                {
                    5,
                    {6,2},
                    {7,2}
                }
            },

            MoveTest{
                L"can't move past the end of the 'document'",
                documentEnd,
                documentEnd,
                5,
                {
                    0,
                    documentEnd,
                    documentEnd,
                }
            },

            MoveTest{
                L"can move to a new row when necessary when moving forward",
                { lastColumnIndex, 0 },
                { lastColumnIndex, 0 },
                5,
                {
                    5,
                    {4 , 0 + 1},
                    {4 , 0 + 1}
                }
            },

            MoveTest{
                L"can move to a new row when necessary when moving backward",
                { 0, 0 + 1 },
                { lastColumnIndex, 0 + 1 },
                -5,
                {
                    -5,
                    {lastColumnIndex - 4, 0},
                    {lastColumnIndex - 3, 0}
                }
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (const auto& test : testData)
        {
            Log::Comment(test.comment.data());
            int amountMoved;

            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end));
            THROW_IF_FAILED(utr->Move(TextUnit::TextUnit_Character, test.moveAmt, &amountMoved));

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveByLine)
    {
        const auto lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;

        // GH#6986: This is used as the "end of the buffer" to help screen readers run faster
        //          instead of parsing through thousands of empty lines of text.
        const til::point documentEnd{ _pTextBuffer->GetSize().Left(), _pTextBuffer->GetLastNonSpaceCharacter().y + 1 };

        // clang-format off
        const std::vector<MoveTest> testData
        {
            MoveTest{
                L"can move forward from top row",
                {0, 0},
                {0, lastColumnIndex},
                4,
                {
                    4,
                    {0, 4},
                    {0, 5}
                }
            },

            MoveTest{
                L"can't move past the end of the 'document'",
                documentEnd,
                documentEnd,
                5,
                {
                    0,
                    documentEnd,
                    documentEnd,
                }
            },

            MoveTest{
                L"can move backward from bottom row",
                {0, documentEnd.y},
                {lastColumnIndex, documentEnd.y},
                -3,
                {
                    -3,
                    {0, documentEnd.y - 3},
                    {0, documentEnd.y - 3}
                }
            },

            MoveTest{
                L"can move to a new row when necessary when moving forward",
                { lastColumnIndex, 0 },
                { lastColumnIndex, 0 },
                5,
                {
                    5,
                    {0, 0 + 5},
                    {0, 0 + 5}
                }
            },

            MoveTest{
                L"can move to a new row when necessary when moving backward",
                { 0, 7 },
                { 0, 7 },
                -5,
                {
                    -5,
                    {0, 7 - 5},
                    {0, 7 - 5}
                }
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (const auto& test : testData)
        {
            Log::Comment(test.comment.data());
            int amountMoved;

            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end));
            THROW_IF_FAILED(utr->Move(TextUnit::TextUnit_Line, test.moveAmt, &amountMoved));

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitCharacter)
    {
        const auto lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;

        // GH#6986: This is used as the "end of the buffer" to help screen readers run faster
        //          instead of parsing through thousands of empty lines of text.
        const til::point documentEnd{ _pTextBuffer->GetSize().RightInclusive(), _pTextBuffer->GetLastNonSpaceCharacter().y };

        // clang-format off
        const std::array testData
        {
            MoveEndpointTest{
                L"can't move _start past the beginning of the document when _start is positioned at the beginning",
                {0, 0},
                {lastColumnIndex, 0},
                -1,
                TextPatternRangeEndpoint_Start,
                {
                    0,
                    {0, 0},
                    {lastColumnIndex, 0}
                }
            },

            MoveEndpointTest{
                L"can partially move _start to the beginning of the document when it is closer than the move count requested",
                {3, 0},
                {lastColumnIndex, 0},
                -5,
                TextPatternRangeEndpoint_Start,
                {
                    -3,
                    {0, 0},
                    {lastColumnIndex, 0}
                }
            },

            MoveEndpointTest{
                L"can't move _end past the beginning of the document",
                {0, 0},
                {4, 0},
                -5,
                TextPatternRangeEndpoint_End,
                {
                    -4,
                    {0, 0},
                    {0, 0}
                }
            },

            MoveEndpointTest{
                L"can't move _end past the end of the document",
                {0, 0},
                documentEnd,
                5,
                TextPatternRangeEndpoint_End,
                {
                    1,
                    {0,0},
                    {0, documentEnd.y + 1}
                }
            },

            MoveEndpointTest{
                L"_start follows _end when passed during movement",
                {5, 0},
                {10, 0},
                -7,
                TextPatternRangeEndpoint_End,
                {
                    -7,
                    {3, 0},
                    {3, 0}
                }
            },

            MoveEndpointTest{
                L"can't move _end past the beginning of the document when _end is positioned at the end",
                {0, documentEnd.y},
                {0, documentEnd.y + 1},
                1,
                TextPatternRangeEndpoint_End,
                {
                    0,
                    {0, documentEnd.y},
                    {0, documentEnd.y + 1},
                }
            },

            MoveEndpointTest{
                L"can partially move _end to the end of the document when it is closer than the move count requested",
                {0, 0},
                {lastColumnIndex - 3, documentEnd.y},
                5,
                TextPatternRangeEndpoint_End,
                {
                    4,
                    {0, 0},
                    {0, documentEnd.y + 1},
                }
            },

            MoveEndpointTest{
                L"can't move _start past the end of the document",
                {lastColumnIndex - 4, documentEnd.y},
                {0, documentEnd.y + 1},
                5,
                TextPatternRangeEndpoint_Start,
                {
                    5,
                    {0, documentEnd.y + 1},
                    {0, documentEnd.y + 1},
                }
            },

            MoveEndpointTest{
                L"_end follows _start when passed during movement",
                {5, 0},
                {10, 0},
                7,
                TextPatternRangeEndpoint_Start,
                {
                    7,
                    {12, 0},
                    {12, 0}
                }
            },
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (const auto& test : testData)
        {
            Log::Comment(test.comment.data());
            int amountMoved;

            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end));
            THROW_IF_FAILED(utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Character, test.moveAmt, &amountMoved));

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitLine)
    {
        const auto lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const auto bottomRow = _pTextBuffer->TotalRowCount() - 1;

        // GH#6986: This is used as the "end of the buffer" to help screen readers run faster
        //          instead of parsing through thousands of empty lines of text.
        const til::point documentEnd{ _pTextBuffer->GetSize().Left(), _pTextBuffer->GetLastNonSpaceCharacter().y + 1 };

        // clang-format off
        const std::vector<MoveEndpointTest> testData
        {
            MoveEndpointTest{
                L"can move _end forward without affecting _start",
                {0, 0},
                {lastColumnIndex, 0},
                1,
                TextPatternRangeEndpoint_End,
                1,
                {0, 0},
                {0, 1}
            },

            MoveEndpointTest{
                L"can move _end backward without affecting _start",
                {0, 1},
                {lastColumnIndex, 5},
                -2,
                TextPatternRangeEndpoint_End,
                -2,
                {0, 1},
                {0, 4}
            },

            MoveEndpointTest{
                L"can move _start forward without affecting _end",
                {0, 1},
                {lastColumnIndex, 5},
                2,
                TextPatternRangeEndpoint_Start,
                2,
                {0, 3},
                {lastColumnIndex, 5}
            },

            MoveEndpointTest{
                L"can move _start backward without affecting _end",
                {0, 2},
                {lastColumnIndex, 5},
                -1,
                TextPatternRangeEndpoint_Start,
                -1,
                {0, 1},
                {lastColumnIndex, 5}
            },

            MoveEndpointTest{
                L"can move _start backwards when it's already on the top row",
                {lastColumnIndex, 0},
                {lastColumnIndex, 0},
                -1,
                TextPatternRangeEndpoint_Start,
                -1,
                {0, 0},
                {lastColumnIndex, 0},
            },

            MoveEndpointTest{
                L"can't move _start backwards when it's at the start of the document already",
                {0, 0},
                {lastColumnIndex, 0},
                -1,
                TextPatternRangeEndpoint_Start,
                0,
                {0, 0},
                {lastColumnIndex, 0}
            },

            MoveEndpointTest{
                L"can't move _end forwards when it's on the bottom row (past doc end)",
                {0, 0},
                {lastColumnIndex - 3, bottomRow},
                1,
                TextPatternRangeEndpoint_End,
                0,
                {0, 0},
                documentEnd
            },

            MoveEndpointTest{
                L"can't move _end forwards when it's at the end of the buffer already (past doc end)",
                {0, 0},
                {0, bottomRow + 1},
                1,
                TextPatternRangeEndpoint_End,
                0,
                {0, 0},
                documentEnd
            },

            MoveEndpointTest{
                L"moving _start forward when it's already on the bottom row (past doc end) creates a degenerate range at the document end",
                {0, bottomRow},
                {lastColumnIndex, bottomRow},
                1,
                TextPatternRangeEndpoint_Start,
                0,
                documentEnd,
                documentEnd
            },

            MoveEndpointTest{
                L"moving _end backward when it's already on the top row creates a degenerate range at the document start",
                {4, 0},
                {lastColumnIndex - 5, 0},
                -1,
                TextPatternRangeEndpoint_End,
                -1,
                {0, 0},
                {0, 0}
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (const auto& test : testData)
        {
            Log::Comment(test.comment.data());
            int amountMoved;

            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end));
            THROW_IF_FAILED(utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Line, test.moveAmt, &amountMoved));

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitDocument)
    {
        const auto bottomRow = _pTextBuffer->TotalRowCount() - 1;

        // GH#6986: This is used as the "end of the buffer" to help screen readers run faster
        //          instead of parsing through thousands of empty lines of text.
        const til::point documentEnd{ _pTextBuffer->GetSize().Left(), _pTextBuffer->GetLastNonSpaceCharacter().y + 1 };

        // clang-format off
        const std::vector<MoveEndpointTest> testData =
        {
            MoveEndpointTest{
                L"can move _end forward to end of document without affecting _start",
                {0, 4},
                {0, 4},
                1,
                TextPatternRangeEndpoint_End,
                {
                    1,
                    {0, 4},
                    documentEnd
                }
            },

            MoveEndpointTest{
                L"can move _start backward to end of document without affect _end",
                {0, 4},
                {0, 4},
                -1,
                TextPatternRangeEndpoint_Start,
                {
                    -1,
                    {0, 0},
                    {0, 4}
                }
            },

            MoveEndpointTest{
                L"can't move _end forward when it's already at the end of the buffer (past doc end)",
                {3, 2},
                {0, bottomRow + 1},
                1,
                TextPatternRangeEndpoint_End,
                {
                    0,
                    {3, 2},
                    documentEnd
                }
            },

            MoveEndpointTest{
                L"can't move _start backward when it's already at the start of the document",
                {0, 0},
                {5, 6},
                -1,
                TextPatternRangeEndpoint_Start,
                {
                    0,
                    {0, 0},
                    {5, 6}
                }
            },

            MoveEndpointTest{
                L"moving _end backward creates degenerate range at start of document",
                {5, 2},
                {5, 6},
                -1,
                TextPatternRangeEndpoint_End,
                {
                    -1,
                    {0, 0},
                    {0, 0}
                }
            },

            MoveEndpointTest{
                L"moving _start forward creates degenerate range at end of document",
                {5, 2},
                {5, 6},
                1,
                TextPatternRangeEndpoint_Start,
                {
                    1,
                    documentEnd,
                    documentEnd
                }
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (auto test : testData)
        {
            Log::Comment(test.comment.c_str());
            int amountMoved;

            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end));
            THROW_IF_FAILED(utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Document, test.moveAmt, &amountMoved));

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(ExpansionAtExclusiveEnd)
    {
        // GH#7664: When attempting to expand to an enclosing unit
        // at the end exclusive, the UTR should refuse to move past
        // the end.
        const auto lastNonspaceCharPos{ _pTextBuffer->GetLastNonSpaceCharacter() };
        const til::point documentEnd{ 0, lastNonspaceCharPos.y + 1 };

        // Iterate over each TextUnit. If we don't support
        // the given TextUnit, we're supposed to fallback
        // to the last one that was defined anyways.
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:textUnit", L"{0, 1, 2, 3, 4, 5, 6}")
        END_TEST_METHOD_PROPERTIES();

        int textUnit;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"textUnit", textUnit), L"Get textUnit variant");

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        Log::Comment(NoThrowString().Format(L"%s", toString(static_cast<TextUnit>(textUnit))));

        // Create a degenerate UTR at EndExclusive
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, bufferEnd, endExclusive));
        THROW_IF_FAILED(utr->ExpandToEnclosingUnit(static_cast<TextUnit>(textUnit)));

        VERIFY_ARE_EQUAL(documentEnd, utr->_end);
    }

    TEST_METHOD(MovementAtExclusiveEnd)
    {
        // GH#7663: When attempting to move from end exclusive,
        // the UTR should refuse to move past the end.
        const auto endInclusive{ bufferEnd };

        // write "temp" at (2,2)
        _pTextBuffer->Reset();
        const til::point writeTarget{ 2, 2 };
        _pTextBuffer->Write({ L"temp" }, writeTarget);

        // GH#6986: This is used as the "end of the buffer" to help screen readers run faster
        //          instead of parsing through thousands of empty lines of text.
        const til::point documentEndInclusive{ bufferSize.right - 1, _pTextBuffer->GetLastNonSpaceCharacter().y };
        const til::point documentEndExclusive{ bufferSize.left, documentEndInclusive.y + 1 };

        const til::point lastLineStart{ bufferSize.left, documentEndInclusive.y };
        const auto secondToLastLinePos{ point_offset_by_line(lastLineStart, bufferSize, -1) };
        const til::point secondToLastCharacterPos{ documentEndInclusive.x - 1, documentEndInclusive.y };

        // Iterate over each TextUnit. If we don't support
        // the given TextUnit, we're supposed to fallback
        // to the last one that was defined anyways.
        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:textUnit", L"{0, 1, 2, 3, 4, 5, 6}")
            TEST_METHOD_PROPERTY(L"Data:degenerate", L"{false, true}")
            TEST_METHOD_PROPERTY(L"Data:atDocumentEnd", L"{false, true}")
        END_TEST_METHOD_PROPERTIES();

        int unit;
        bool degenerate;
        bool atDocumentEnd;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"textUnit", unit), L"Get TextUnit variant");
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"degenerate", degenerate), L"Get degenerate variant");
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"atDocumentEnd", atDocumentEnd), L"Get atDocumentEnd variant");
        auto textUnit{ static_cast<TextUnit>(unit) };

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        int moveAmt;
        Log::Comment(NoThrowString().Format(L"Forward by %s", toString(textUnit)));

        // Create an UTR at EndExclusive
        const auto utrEnd{ atDocumentEnd ? documentEndExclusive : endExclusive };
        if (degenerate)
        {
            // UTR: (exclusive, exclusive) range
            const auto utrStart{ atDocumentEnd ? documentEndExclusive : endExclusive };
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, utrStart, utrEnd));
        }
        else
        {
            // UTR: (inclusive, exclusive) range
            const auto utrStart{ atDocumentEnd ? documentEndInclusive : endInclusive };
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, utrStart, utrEnd));
        }
        THROW_IF_FAILED(utr->Move(textUnit, 1, &moveAmt));

        VERIFY_ARE_EQUAL(documentEndExclusive, utr->_end);
        VERIFY_ARE_EQUAL(0, moveAmt);

        // Verify expansion works properly
        Log::Comment(NoThrowString().Format(L"Expand by %s", toString(textUnit)));
        THROW_IF_FAILED(utr->ExpandToEnclosingUnit(textUnit));
        if (textUnit <= TextUnit::TextUnit_Character)
        {
            VERIFY_ARE_EQUAL(documentEndInclusive, utr->_start);
            VERIFY_ARE_EQUAL(documentEndExclusive, utr->_end);
        }
        else if (textUnit <= TextUnit::TextUnit_Word)
        {
            VERIFY_ARE_EQUAL(writeTarget, utr->_start);
            VERIFY_ARE_EQUAL(documentEndExclusive, utr->_end);
        }
        else if (textUnit <= TextUnit::TextUnit_Line)
        {
            VERIFY_ARE_EQUAL(lastLineStart, utr->_start);
            VERIFY_ARE_EQUAL(documentEndExclusive, utr->_end);
        }
        else // textUnit <= TextUnit::TextUnit_Document:
        {
            VERIFY_ARE_EQUAL(origin, utr->_start);
            VERIFY_ARE_EQUAL(documentEndExclusive, utr->_end);
        }

        // reset the UTR
        if (degenerate)
        {
            // UTR: (exclusive, exclusive) range
            const auto utrStart{ atDocumentEnd ? documentEndExclusive : endExclusive };
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, utrStart, utrEnd));
        }
        else
        {
            // UTR: (inclusive, exclusive) range
            const auto utrStart{ atDocumentEnd ? documentEndInclusive : endInclusive };
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, utrStart, utrEnd));
        }

        // Verify that moving backwards still works properly
        Log::Comment(NoThrowString().Format(L"Backwards by %s", toString(textUnit)));
        THROW_IF_FAILED(utr->Move(textUnit, -1, &moveAmt));

        // NOTE: If the range is degenerate, _start == _end before AND after the move.
        if (textUnit <= TextUnit::TextUnit_Character)
        {
            // Special case: _end will always be endInclusive, because...
            // -  degenerate --> it moves with _start to stay degenerate
            // - !degenerate --> it excludes the last char, to select the second to last char
            VERIFY_ARE_EQUAL(-1, moveAmt);
            VERIFY_ARE_EQUAL(degenerate || !atDocumentEnd ? documentEndInclusive : secondToLastCharacterPos, utr->_start);
            VERIFY_ARE_EQUAL(documentEndInclusive, utr->_end);
        }
        else if (textUnit <= TextUnit::TextUnit_Word)
        {
            VERIFY_ARE_EQUAL(-1, moveAmt);
            VERIFY_ARE_EQUAL(degenerate || !atDocumentEnd ? writeTarget : origin, utr->_start);
            VERIFY_ARE_EQUAL(writeTarget, utr->_end);
        }
        else if (textUnit <= TextUnit::TextUnit_Line)
        {
            VERIFY_ARE_EQUAL(-1, moveAmt);
            VERIFY_ARE_EQUAL(degenerate || !atDocumentEnd ? lastLineStart : secondToLastLinePos, utr->_start);
            VERIFY_ARE_EQUAL(lastLineStart, utr->_end);
        }
        else // textUnit <= TextUnit::TextUnit_Document:
        {
            VERIFY_ARE_EQUAL(degenerate || !atDocumentEnd ? -1 : 0, moveAmt);
            VERIFY_ARE_EQUAL(origin, utr->_start);
            VERIFY_ARE_EQUAL(degenerate || !atDocumentEnd ? origin : documentEndExclusive, utr->_end);
        }
    }

    TEST_METHOD(MoveToPreviousWord)
    {
        // See GH#7742 for more details.

        const auto originExclusive{ point_offset_by_char(origin, bufferSize, 1) };

        _pTextBuffer->Write({ L"My name is Carlos" }, origin);

        // Create degenerate UTR at origin
        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, origin, origin));

        // move forward by a word
        int moveAmt;
        THROW_IF_FAILED(utr->Move(TextUnit::TextUnit_Word, 1, &moveAmt));
        VERIFY_ARE_EQUAL(1, moveAmt);
        VERIFY_IS_TRUE(utr->IsDegenerate());

        // Expand by word
        BSTR text;
        THROW_IF_FAILED(utr->ExpandToEnclosingUnit(TextUnit::TextUnit_Word));
        THROW_IF_FAILED(utr->GetText(-1, &text));
        VERIFY_ARE_EQUAL(L"name ", std::wstring_view{ text });

        // Collapse utr (move end to start)
        const til::point expectedStart{ 3, 0 };
        THROW_IF_FAILED(utr->MoveEndpointByRange(TextPatternRangeEndpoint::TextPatternRangeEndpoint_End, utr.Get(), TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start));
        VERIFY_ARE_EQUAL(expectedStart, utr->_start);
        VERIFY_IS_TRUE(utr->IsDegenerate());

        // Move back by a word
        THROW_IF_FAILED(utr->Move(TextUnit::TextUnit_Word, -1, &moveAmt));
        VERIFY_ARE_EQUAL(-1, moveAmt);

        // Expand by character
        THROW_IF_FAILED(utr->ExpandToEnclosingUnit(TextUnit::TextUnit_Character));
        THROW_IF_FAILED(utr->GetText(-1, &text));
        VERIFY_ARE_EQUAL(L"M", std::wstring_view{ text });
    }

    TEST_METHOD(ScrollIntoView)
    {
        const auto viewportSize{ _pUiaData->GetViewport() };

        const std::vector<ScrollTest> testData{
            { L"Origin", bufferSize.top },
            { L"ViewportHeight From Top - 1", bufferSize.top + viewportSize.Height() - 1 },
            { L"ViewportHeight From Top", bufferSize.top + viewportSize.Height() },
            { L"ViewportHeight From Top + 1", bufferSize.top + viewportSize.Height() + 1 },
            { L"ViewportHeight From Bottom - 1", bufferSize.bottom - viewportSize.Height() - 2 },
            { L"ViewportHeight From Bottom", bufferSize.bottom - viewportSize.Height() - 1 },
            { L"ViewportHeight From Bottom + 1", bufferSize.bottom - viewportSize.Height() + 1 },

            // GH#7839: ExclusiveEnd is a nonexistent space,
            // so scrolling to it when !alignToTop used to crash
            { L"Exclusive End", bufferSize.bottom }
        };

        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"Data:alignToTop", L"{false, true}")
        END_TEST_METHOD_PROPERTIES();

        bool alignToTop;
        VERIFY_SUCCEEDED(TestData::TryGetValue(L"alignToTop", alignToTop), L"Get alignToTop variant");

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (const auto test : testData)
        {
            Log::Comment(test.comment.c_str());
            const til::point pos{ bufferSize.left, test.yPos };
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, pos, pos));
            VERIFY_SUCCEEDED(utr->ScrollIntoView(alignToTop));
        }
    }

    TEST_METHOD(GetAttributeValue)
    {
        Log::Comment(L"Check supported attributes");
        Microsoft::WRL::ComPtr<IUnknown> notSupportedVal;
        UiaGetReservedNotSupportedValue(&notSupportedVal);

        // Iterate over UIA's Text Attribute Identifiers
        // Validate that we know which ones are (not) supported
        // source: https://docs.microsoft.com/en-us/windows/win32/winauto/uiauto-textattribute-ids
        for (auto uiaAttributeId = UIA_AnimationStyleAttributeId; uiaAttributeId <= UIA_AfterParagraphSpacingAttributeId; ++uiaAttributeId)
        {
            Microsoft::WRL::ComPtr<UiaTextRange> utr;
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider));
            THROW_IF_FAILED(utr->ExpandToEnclosingUnit(TextUnit_Character));

            Log::Comment(NoThrowString().Format(L"Attribute ID: %d", uiaAttributeId));
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(uiaAttributeId, &result));

            switch (uiaAttributeId)
            {
            case UIA_FontNameAttributeId:
            {
                VERIFY_ARE_EQUAL(VT_BSTR, result.vt);
                break;
            }
            case UIA_BackgroundColorAttributeId:
            case UIA_FontWeightAttributeId:
            case UIA_ForegroundColorAttributeId:
            case UIA_StrikethroughStyleAttributeId:
            case UIA_UnderlineStyleAttributeId:
            {
                VERIFY_ARE_EQUAL(VT_I4, result.vt);
                break;
            }
            case UIA_IsItalicAttributeId:
            case UIA_IsReadOnlyAttributeId:
            {
                VERIFY_ARE_EQUAL(VT_BOOL, result.vt);
                break;
            }
            default:
            {
                // Expected: not supported
                VERIFY_ARE_EQUAL(VT_UNKNOWN, result.vt);
                VERIFY_ARE_EQUAL(notSupportedVal.Get(), result.punkVal);
                break;
            }
            }
        }

        // This is the text attribute we'll use to update the text buffer.
        // We'll modify it, then test if the UiaTextRange can extract/interpret the data properly.
        // updateBuffer() will write that text attribute to the first cell in the buffer.
        TextAttribute attr;
        auto updateBuffer = [&](TextAttribute outputAttr) {
            _pTextBuffer->Write({ outputAttr }, { 0, 0 });
        };

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider));
        THROW_IF_FAILED(utr->ExpandToEnclosingUnit(TextUnit_Character));
        {
            Log::Comment(L"Test Background");
            const auto rawBackgroundColor{ RGB(255, 0, 0) };
            attr.SetBackground(rawBackgroundColor);
            updateBuffer(attr);
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_BackgroundColorAttributeId, &result));

            const auto realBackgroundColor{ _pUiaData->GetAttributeColors(attr).second & 0x00ffffff };
            VERIFY_ARE_EQUAL(realBackgroundColor, static_cast<COLORREF>(result.lVal));
        }
        {
            Log::Comment(L"Test Font Weight");
            attr.SetIntense(true);
            updateBuffer(attr);
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_FontWeightAttributeId, &result));
            VERIFY_ARE_EQUAL(FW_BOLD, result.lVal);

            attr.SetIntense(false);
            updateBuffer(attr);
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_FontWeightAttributeId, &result));
            VERIFY_ARE_EQUAL(FW_NORMAL, result.lVal);
            ;
        }
        {
            Log::Comment(L"Test Foreground");
            const auto rawForegroundColor{ RGB(255, 0, 0) };
            attr.SetForeground(rawForegroundColor);
            updateBuffer(attr);
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_ForegroundColorAttributeId, &result));

            const auto realForegroundColor{ _pUiaData->GetAttributeColors(attr).first & 0x00ffffff };
            VERIFY_ARE_EQUAL(realForegroundColor, static_cast<COLORREF>(result.lVal));
        }
        {
            Log::Comment(L"Test Italic");
            attr.SetItalic(true);
            updateBuffer(attr);
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_IsItalicAttributeId, &result));
            VERIFY_IS_TRUE(result.boolVal);

            attr.SetItalic(false);
            updateBuffer(attr);
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_IsItalicAttributeId, &result));
            VERIFY_IS_FALSE(result.boolVal);
        }
        {
            Log::Comment(L"Test Strikethrough");
            attr.SetCrossedOut(true);
            updateBuffer(attr);
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_StrikethroughStyleAttributeId, &result));
            VERIFY_ARE_EQUAL(TextDecorationLineStyle_Single, result.lVal);

            attr.SetCrossedOut(false);
            updateBuffer(attr);
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_StrikethroughStyleAttributeId, &result));
            VERIFY_ARE_EQUAL(TextDecorationLineStyle_None, result.lVal);
        }
        {
            Log::Comment(L"Test Underline");

            // Single underline
            attr.SetUnderlined(true);
            updateBuffer(attr);
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_UnderlineStyleAttributeId, &result));
            VERIFY_ARE_EQUAL(TextDecorationLineStyle_Single, result.lVal);

            // Double underline (double supersedes single)
            attr.SetDoublyUnderlined(true);
            updateBuffer(attr);
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_UnderlineStyleAttributeId, &result));
            VERIFY_ARE_EQUAL(TextDecorationLineStyle_Double, result.lVal);

            // Double underline (double on its own)
            attr.SetUnderlined(false);
            updateBuffer(attr);
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_UnderlineStyleAttributeId, &result));
            VERIFY_ARE_EQUAL(TextDecorationLineStyle_Double, result.lVal);

            // No underline
            attr.SetDoublyUnderlined(false);
            updateBuffer(attr);
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_UnderlineStyleAttributeId, &result));
            VERIFY_ARE_EQUAL(TextDecorationLineStyle_None, result.lVal);
        }
        {
            Log::Comment(L"Test Font Name (special)");
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_FontNameAttributeId, &result));
            const std::wstring actualFontName{ result.bstrVal };
            const auto expectedFontName{ _pUiaData->GetFontInfo().GetFaceName() };
            VERIFY_ARE_EQUAL(expectedFontName, actualFontName);
        }
        {
            Log::Comment(L"Test Read Only (special)");
            VARIANT result;
            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_IsReadOnlyAttributeId, &result));
            VERIFY_IS_FALSE(result.boolVal);
        }
        {
            // "Mixed" is when the desired attribute value is inconsistent across the range.
            // We'll make our life easier by setting an attribute on a character,
            // but getting the attribute for the entire line.
            Log::Comment(L"Test Mixed");
            VARIANT result;
            THROW_IF_FAILED(utr->ExpandToEnclosingUnit(TextUnit_Line));

            // set first cell as underlined, but second cell as not underlined
            attr.SetUnderlined(true);
            _pTextBuffer->Write({ attr }, { 0, 0 });
            attr.SetUnderlined(false);
            _pTextBuffer->Write({ attr }, { 1, 0 });

            VERIFY_SUCCEEDED(utr->GetAttributeValue(UIA_UnderlineStyleAttributeId, &result));

            // Expected: mixed
            Microsoft::WRL::ComPtr<IUnknown> mixedVal;
            THROW_IF_FAILED(UiaGetReservedMixedAttributeValue(&mixedVal));
            VERIFY_ARE_EQUAL(VT_UNKNOWN, result.vt);
            VERIFY_ARE_EQUAL(mixedVal.Get(), result.punkVal);
        }
    }

    TEST_METHOD(FindAttribute)
    {
        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        const til::point startPos{ 0, 0 };
        const til::point endPos{ 0, 2 };
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, startPos, endPos));
        {
            Log::Comment(L"Test Font Name (special)");

            // Populate query with font name currently in use.
            const auto fontName{ _pUiaData->GetFontInfo().GetFaceName() };
            VARIANT var{};
            var.vt = VT_BSTR;
            var.bstrVal = SysAllocString(fontName.data());

            Microsoft::WRL::ComPtr<ITextRangeProvider> result;
            VERIFY_SUCCEEDED(utr->FindAttribute(UIA_FontNameAttributeId, var, false, result.GetAddressOf()));

            // Expecting the same text range endpoints
            BOOL isEqual;
            THROW_IF_FAILED(utr->Compare(result.Get(), &isEqual));
            VERIFY_IS_TRUE(isEqual);

            // Now perform the same test, but searching backwards
            Log::Comment(L"Test Font Name (special) - Backwards");
            Microsoft::WRL::ComPtr<ITextRangeProvider> resultBackwards;
            VERIFY_SUCCEEDED(utr->FindAttribute(UIA_FontNameAttributeId, var, true, resultBackwards.GetAddressOf()));

            // Expecting the same text range endpoints
            THROW_IF_FAILED(result->Compare(resultBackwards.Get(), &isEqual));
            VERIFY_IS_TRUE(isEqual);
        }
        {
            Log::Comment(L"Test Read Only (special)");

            VARIANT var{};
            var.vt = VT_BOOL;
            var.boolVal = false;

            Microsoft::WRL::ComPtr<ITextRangeProvider> result;
            VERIFY_SUCCEEDED(utr->FindAttribute(UIA_IsReadOnlyAttributeId, var, false, result.GetAddressOf()));

            // Expecting the same text range endpoints
            BOOL isEqual;
            THROW_IF_FAILED(utr->Compare(result.Get(), &isEqual));
            VERIFY_IS_TRUE(isEqual);

            // Now perform the same test, but searching backwards
            Log::Comment(L"Test Read Only (special) - Backwards");
            Microsoft::WRL::ComPtr<ITextRangeProvider> resultBackwards;
            VERIFY_SUCCEEDED(utr->FindAttribute(UIA_IsReadOnlyAttributeId, var, true, resultBackwards.GetAddressOf()));

            // Expecting the same text range endpoints
            THROW_IF_FAILED(result->Compare(resultBackwards.Get(), &isEqual));
            VERIFY_IS_TRUE(isEqual);
        }
        {
            Log::Comment(L"Test IsItalic (standard attribute)");

            // Since all of the other attributes operate very similarly,
            //  we're just going to pick one of them and test that.
            // The "GetAttribute" tests provide code coverage for
            //  retrieving an attribute verification function.
            // This test is intended to provide code coverage for
            //  finding a text range with the desired attribute.

            // Set up the buffer's attributes.
            TextAttribute italicAttr;
            italicAttr.SetItalic(true);
            auto iter{ _pUiaData->GetTextBuffer().GetCellDataAt(startPos) };
            for (auto i = 0; i < 5; ++i)
            {
                _pTextBuffer->Write({ L"X", italicAttr }, iter.Pos());
                ++iter;
            }

            // set the expected end (exclusive)
            const auto expectedEndPos{ iter.Pos() };

            VARIANT var{};
            var.vt = VT_BOOL;
            var.boolVal = true;

            Microsoft::WRL::ComPtr<ITextRangeProvider> result;
            THROW_IF_FAILED(utr->ExpandToEnclosingUnit(TextUnit_Document));
            VERIFY_SUCCEEDED(utr->FindAttribute(UIA_IsItalicAttributeId, var, false, result.GetAddressOf()));

            Microsoft::WRL::ComPtr<UiaTextRange> resultUtr{ static_cast<UiaTextRange*>(result.Get()) };
            VERIFY_ARE_EQUAL(startPos, resultUtr->_start);
            VERIFY_ARE_EQUAL(expectedEndPos, resultUtr->_end);

            // Now perform the same test, but searching backwards
            Log::Comment(L"Test IsItalic (standard attribute) - Backwards");
            Microsoft::WRL::ComPtr<ITextRangeProvider> resultBackwards;
            VERIFY_SUCCEEDED(utr->FindAttribute(UIA_IsItalicAttributeId, var, true, resultBackwards.GetAddressOf()));

            // Expecting the same text range endpoints
            BOOL isEqual;
            THROW_IF_FAILED(result->Compare(resultBackwards.Get(), &isEqual));
            VERIFY_IS_TRUE(isEqual);
        }
    }

    TEST_METHOD(BlockRange)
    {
        // This test replicates GH#7960.
        // It was caused by _blockRange being uninitialized, resulting in it occasionally being set to true.
        // Additionally, all of the ctors _except_ the copy ctor initialized it. So this would be more apparent
        // when calling Clone.
        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider));
        VERIFY_IS_FALSE(utr->_blockRange);

        Microsoft::WRL::ComPtr<ITextRangeProvider> clone1;
        THROW_IF_FAILED(utr->Clone(&clone1));

        auto cloneUtr1 = static_cast<UiaTextRange*>(clone1.Get());
        VERIFY_IS_FALSE(cloneUtr1->_blockRange);
        cloneUtr1->_blockRange = true;

        Microsoft::WRL::ComPtr<ITextRangeProvider> clone2;
        cloneUtr1->Clone(&clone2);
        auto cloneUtr2 = static_cast<UiaTextRange*>(clone2.Get());
        VERIFY_IS_TRUE(cloneUtr2->_blockRange);
    }

    TEST_METHOD(Movement)
    {
        // Helpful variables
        const auto firstChar{ point_offset_by_char(origin, bufferSize, 1) };
        const auto secondChar{ point_offset_by_char(origin, bufferSize, 2) };
        const auto fifthChar{ point_offset_by_char(origin, bufferSize, 5) };
        const auto sixthChar{ point_offset_by_char(origin, bufferSize, 6) };
        const til::point documentEnd{ bufferSize.left, bufferSize.height() / 2 + 1 };

        // Populate buffer
        //   Split the line into 5 segments alternating between "X" and whitespace
        //   _________________
        //   |XXX   XXX   XXX|
        //   |XXX   XXX   XXX|
        //   |XXX   XXX   XXX|
        //   |XXX   XXX   XXX|
        //   |_______________|
        {
            auto i = 0;
            auto iter{ _pTextBuffer->GetCellDataAt(origin) };
            const auto segment{ bufferSize.width() / 5 };
            while (iter.Pos() != documentEnd)
            {
                auto fill{ true };
                if (i % segment == 0)
                {
                    fill = !fill;
                }

                if (fill)
                {
                    _pTextBuffer->Write({ L"X" }, iter.Pos());
                }

                ++i;
                ++iter;
            }
        }

        // Define tests
        struct TestInput
        {
            TextUnit unit;
            int moveAmt;
            til::point start;
            til::point end;
        };

        struct TestOutput
        {
            int moveAmt;
            til::point start;
            til::point end;
        };

        struct MyTest
        {
            std::wstring name;
            TestInput input;
            TestOutput output;
        };

        const std::vector<MyTest> tests{
            MyTest{ L"Degenerate at origin", TestInput{ TextUnit_Character, -5, origin, origin }, TestOutput{ 0, origin, origin } }
        };
    }

    TEST_METHOD(GeneratedMovementTests)
    {
        // Populate the buffer with...
        // - 10 segments of alternating text
        // - up to half of the buffer (vertically)
        // It'll look something like this
        // +------------------------------+
        // |XXX   XXX   XXX   XXX   XXX   |
        // |XXX   XXX   XXX   XXX   XXX   |
        // |XXX   XXX   XXX   XXX   XXX   |
        // |XXX   XXX   XXX   XXX   XXX   |
        // |XXX   XXX   XXX   XXX   XXX   |
        // |                              |
        // |                              |
        // |                              |
        // |                              |
        // |                              |
        // +------------------------------+
        {
            auto i = 0;
            auto iter{ _pTextBuffer->GetCellDataAt(bufferSize.origin()) };
            const auto segment{ bufferSize.width() / 10 };
            auto fill{ true };
            while (iter.Pos() != docEnd)
            {
                if (iter.Pos().x == bufferSize.left)
                {
                    fill = true;
                }
                else if (i % segment == 0)
                {
                    fill = !fill;
                }

                _pTextBuffer->Write({ fill ? L"X" : L" " }, iter.Pos());

                ++i;
                ++iter;
            }
        }

        BEGIN_TEST_METHOD_PROPERTIES()
            TEST_METHOD_PROPERTY(L"DataSource", L"Export:GeneratedMovementTestDataSource")
        END_TEST_METHOD_PROPERTIES()

        WEX::TestExecution::DisableVerifyExceptions disableVerifyExceptions{};
        WEX::TestExecution::SetVerifyOutput verifyOutputScope{ WEX::TestExecution::VerifyOutputSettings::LogOnlyFailures };

        unsigned int i{};
        TestData::TryGetValue(L"index", i); // index is produced by the ArrayIndexTaefAdapterSource above
        const auto& testCase{ s_movementTests[i] };

        Log::Comment(NoThrowString().Format(L"[%zu.0] Test case \"%.*s\"", i, testCase.name.size(), testCase.name.data()));
        if (testCase.skip)
        {
            Log::Result(WEX::Logging::TestResults::Result::Skipped);
        }
        else
        {
            Microsoft::WRL::ComPtr<UiaTextRange> utr;
            int amountMoved;
            THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, testCase.input.start, testCase.input.end));
            THROW_IF_FAILED(utr->Move(testCase.input.unit, testCase.input.moveAmount, &amountMoved));

            VERIFY_ARE_EQUAL(testCase.expected.moveAmount, amountMoved);
            VERIFY_ARE_EQUAL(testCase.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(testCase.expected.end, utr->_end);
        }
    }
};
