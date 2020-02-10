// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "WexTestClass.h"
#include "..\..\inc\consoletaeftemplates.hpp"
#include "CommonState.hpp"

#include "uiaTextRange.hpp"
#include "../../../buffer/out/textBuffer.hpp"

using namespace WEX::Common;
using namespace WEX::Logging;
using namespace WEX::TestExecution;

using namespace Microsoft::WRL;

using namespace Microsoft::Console::Interactivity::Win32;

// UiaTextRange takes an object that implements
// IRawElementProviderSimple as a constructor argument. Making a real
// one would involve setting up the window which we don't want to do
// for unit tests so instead we'll use this one. We don't care about
// it not doing anything for its implementation because it is not used
// during the unit tests below.

class DummyElementProvider final : public IRawElementProviderSimple
{
public:
    // IUnknown methods
    IFACEMETHODIMP_(ULONG)
    AddRef()
    {
        return 1;
    }

    IFACEMETHODIMP_(ULONG)
    Release()
    {
        return 1;
    }
    IFACEMETHODIMP QueryInterface(_In_ REFIID /*riid*/,
                                  _COM_Outptr_result_maybenull_ void** /*ppInterface*/)
    {
        return E_NOTIMPL;
    };

    // IRawElementProviderSimple methods
    IFACEMETHODIMP get_ProviderOptions(_Out_ ProviderOptions* /*pOptions*/)
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP GetPatternProvider(_In_ PATTERNID /*iid*/,
                                      _COM_Outptr_result_maybenull_ IUnknown** /*ppInterface*/)
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP GetPropertyValue(_In_ PROPERTYID /*idProp*/,
                                    _Out_ VARIANT* /*pVariant*/)
    {
        return E_NOTIMPL;
    }

    IFACEMETHODIMP get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** /*ppProvider*/)
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
    IUiaData* _pUiaData;

    struct ExpectedResult
    {
        int moveAmt;
        COORD start;
        COORD end;
    };

    struct MoveTest
    {
        std::wstring comment;
        COORD start;
        COORD end;
        int moveAmt;
        ExpectedResult expected;
    };

    struct MoveEndpointTest
    {
        std::wstring comment;
        COORD start;
        COORD end;
        int moveAmt;
        TextPatternRangeEndpoint endpoint;
        ExpectedResult expected;
    };

    TEST_METHOD_SETUP(MethodSetup)
    {
        CONSOLE_INFORMATION& gci = Microsoft::Console::Interactivity::ServiceLocator::LocateGlobals().getConsoleInformation();
        // set up common state
        _state = new CommonState();
        _state->PrepareGlobalFont();
        _state->PrepareGlobalScreenBuffer();
        _state->PrepareNewTextBufferInfo();

        // set up pointers
        _pScreenInfo = &gci.GetActiveOutputBuffer();
        _pTextBuffer = &_pScreenInfo->GetTextBuffer();
        _pUiaData = &gci.renderData;

        // fill text buffer with text
        for (UINT i = 0; i < _pTextBuffer->TotalRowCount(); ++i)
        {
            ROW& row = _pTextBuffer->GetRowByOffset(i);
            auto& charRow = row.GetCharRow();
            for (auto& cell : charRow)
            {
                cell.Char() = L' ';
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
        const auto bufferSize = _pTextBuffer->GetSize();
        const auto origin = bufferSize.Origin();

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
        const COORD end = { origin.X + 1, origin.Y };
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
        const auto bufferSize = _pTextBuffer->GetSize();
        const auto origin = bufferSize.Origin();

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
        const COORD end = { origin.X + 2, origin.Y };
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
        const auto bufferSize = _pTextBuffer->GetSize();
        const auto origin = bufferSize.Origin();

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
        const COORD end = { origin.X + 2, origin.Y };
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
        for (UINT i = 0; i < _pTextBuffer->TotalRowCount(); ++i)
        {
            ROW& row = _pTextBuffer->GetRowByOffset(i);
            auto& charRow = row.GetCharRow();
            for (size_t j = 0; j < charRow.size(); ++j)
            {
                // every 5th cell is a space, otherwise a letter
                // this is used to simulate words
                CharRowCellReference cell = charRow.GlyphAt(j);
                if (j % 5 == 0)
                {
                    cell = L" ";
                }
                else
                {
                    cell = L"x";
                }
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

        auto toString = [&](TextUnit unit) {
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
                return L"Document";
            default:
                throw E_INVALIDARG;
            }
        };

        struct TextUnitBoundaries
        {
            COORD start;
            COORD end;
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
        auto verifyExpansion = [&](TextUnit textUnit, COORD utrStart, COORD utrEnd) {
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
                const COORD end = { boundaries.start.X + 1, boundaries.start.Y };
                verifyExpansion(textUnit, boundaries.start, end);
            }

            // Test 3
            Log::Comment(NoThrowString().Format(L"%s - Test 3", toString(textUnit)));
            verifyExpansion(textUnit, boundaries.start, boundaries.end);

            // Test 4 (impossible for TextUnit_Character and TextUnit_Document)
            if (textUnit != TextUnit_Character && textUnit != TextUnit_Document)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 4", toString(textUnit)));
                const COORD end = { boundaries.end.X + 1, boundaries.end.Y };
                verifyExpansion(textUnit, boundaries.start, end);
            }

            // Test 5 (impossible for TextUnit_Character)
            if (textUnit != TextUnit_Character)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 5", toString(textUnit)));
                const COORD start = { boundaries.start.X + 1, boundaries.start.Y };
                verifyExpansion(textUnit, start, start);
            }

            // Test 6 (impossible for TextUnit_Character)
            if (textUnit != TextUnit_Character)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 6", toString(textUnit)));
                const COORD start = { boundaries.start.X + 1, boundaries.start.Y };
                const COORD end = { start.X + 1, start.Y };
                verifyExpansion(textUnit, start, end);
            }

            // Test 7 (impossible for TextUnit_Character)
            if (textUnit != TextUnit_Character)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 7", toString(textUnit)));
                const COORD start = { boundaries.start.X + 1, boundaries.start.Y };
                verifyExpansion(textUnit, start, boundaries.end);
            }

            // Test 8 (impossible for TextUnit_Character and TextUnit_Document)
            if (textUnit != TextUnit_Character && textUnit != TextUnit_Document)
            {
                Log::Comment(NoThrowString().Format(L"%s - Test 8", toString(textUnit)));
                const COORD start = { boundaries.start.X + 1, boundaries.start.Y };
                const COORD end = { boundaries.end.X + 1, boundaries.end.Y };
                verifyExpansion(textUnit, start, end);
            }
        }
    }

    TEST_METHOD(MoveEndpointByRange)
    {
        const COORD start{ 0, 1 };
        const COORD end{ 1, 2 };
        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        THROW_IF_FAILED(Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr,
                                                                        _pUiaData,
                                                                        &_dummyProvider,
                                                                        start,
                                                                        end));

        const auto bufferSize = _pTextBuffer->GetSize();
        const auto origin = bufferSize.Origin();
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
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        // clang-format off
        const std::vector<MoveTest> testData
        {
            MoveTest{
                L"can't move backward from (0, 0)",
                { 0, 0 },
                { 2, 0 },
                -1,
                {
                    0,
                    {0,0},
                    {2,0}
                }
            },

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
                L"can't move past the last column in the last row",
                { lastColumnIndex, bottomRow },
                { lastColumnIndex, bottomRow },
                5,
                {
                    0,
                    { lastColumnIndex, bottomRow },
                    { lastColumnIndex, bottomRow },
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
                    {5 , 0 + 1}
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
            utr->Move(TextUnit::TextUnit_Character, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveByLine)
    {
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        // clang-format off
        const std::vector<MoveTest> testData
        {
            MoveTest{
                L"can't move backward from top row",
                {0, 0},
                {0, lastColumnIndex},
                -4,
                {
                    0,
                    {0, 0},
                    {0, lastColumnIndex}
                }
            },

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
                L"can't move forward from bottom row",
                {0, bottomRow},
                {lastColumnIndex, bottomRow},
                3,
                {
                    0,
                    {0, bottomRow},
                    {lastColumnIndex, bottomRow},
                }
            },

            MoveTest{
                L"can move backward from bottom row",
                {0, bottomRow},
                {lastColumnIndex, bottomRow},
                -3,
                {
                    -3,
                    {0, bottomRow - 3},
                    {0, bottomRow - 2}
                }
            },

            MoveTest{
                L"can't move backward when part of the top row is in the range",
                {5, 0},
                {lastColumnIndex, 0},
                -1,
                {
                    0,
                    {5, 0},
                    {lastColumnIndex, 0},
                }
            },

            MoveTest{
                L"can't move forward when part of the bottom row is in the range",
                {0, bottomRow},
                {0, bottomRow},
                1,
                {
                    0,
                    {0, bottomRow},
                    {0, bottomRow}
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
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = static_cast<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        // clang-format off
        const std::vector<MoveEndpointTest> testData
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
                {0, bottomRow},
                {0, bottomRow+1},
                1,
                TextPatternRangeEndpoint_End,
                {
                    0,
                    {0, bottomRow},
                    {0, bottomRow+1},
                }
            },

            MoveEndpointTest{
                L"can partially move _end to the end of the document when it is closer than the move count requested",
                {0, 0},
                {lastColumnIndex - 3, bottomRow},
                5,
                TextPatternRangeEndpoint_End,
                {
                    4,
                    {0, 0},
                    {0, bottomRow+1},
                }
            },

            MoveEndpointTest{
                L"can't move _start past the end of the document",
                {lastColumnIndex - 4, bottomRow},
                {0, bottomRow+1},
                5,
                TextPatternRangeEndpoint_Start,
                {
                    5,
                    {0, bottomRow+1},
                    {0, bottomRow+1},
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
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

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
                L"can move _end forwards when it's on the bottom row",
                {0, 0},
                {lastColumnIndex - 3, bottomRow},
                1,
                TextPatternRangeEndpoint_End,
                1,
                {0, 0},
                {0, bottomRow+1}
            },

            MoveEndpointTest{
                L"can't move _end forwards when it's at the end of the document already",
                {0, 0},
                {0, bottomRow+1},
                1,
                TextPatternRangeEndpoint_End,
                0,
                {0, 0},
                {0, bottomRow+1}
            },

            MoveEndpointTest{
                L"moving _start forward when it's already on the bottom row creates a degenerate range at the document end",
                {0, bottomRow},
                {lastColumnIndex, bottomRow},
                1,
                TextPatternRangeEndpoint_Start,
                1,
                {0, bottomRow+1},
                {0, bottomRow+1}
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
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

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
                    {0, bottomRow+1}
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
                L"can't move _end forward when it's already at the end of the document",
                {3, 2},
                {0, bottomRow+1},
                1,
                TextPatternRangeEndpoint_End,
                {
                    0,
                    {3, 2},
                    {0, bottomRow+1}
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
                    {0, bottomRow+1},
                    {0, bottomRow+1}
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
};
