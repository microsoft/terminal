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

        // set up default range
        COORD coord = { 0, 0 };
        Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&_range,
                                                        _pUiaData,
                                                        &_dummyProvider,
                                                        coord,
                                                        coord);

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
        Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&degenerate,
                                                        _pUiaData,
                                                        &_dummyProvider,
                                                        origin,
                                                        origin);
        VERIFY_IS_TRUE(degenerate->IsDegenerate());
        VERIFY_ARE_EQUAL(degenerate->_start, degenerate->_end);

        // make a non-degenerate range and verify that it reports as such
        const COORD end = { origin.X + 1, origin.Y };
        Microsoft::WRL::ComPtr<UiaTextRange> notDegenerate;
        Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&notDegenerate,
                                                        _pUiaData,
                                                        &_dummyProvider,
                                                        origin,
                                                        end);
        VERIFY_IS_FALSE(notDegenerate->IsDegenerate());
        VERIFY_ARE_NOT_EQUAL(notDegenerate->_start, notDegenerate->_end);
    }

    TEST_METHOD(CanMoveByCharacter)
    {
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        struct ExpectedResult
        {
            int moveAmt;
            COORD start;
            COORD end;
        };

        struct Test
        {
            std::wstring comment;
            COORD start;
            COORD end;
            int moveAmt;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
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

            Test{
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

            Test{
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

            Test{
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

            Test{
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

            Test{
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

            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end);
            utr->Move(TextUnit::TextUnit_Character, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveByWord_EmptyBuffer)
    {
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        struct ExpectedResult
        {
            int moveAmt;
            COORD start;
            COORD end;
        };

        struct Test
        {
            std::wstring comment;
            COORD start;
            COORD end;
            int moveAmt;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
                L"can't move backward from (0, 0)",
                { 0, 0 },
                { 0, 2 },
                -1,
                {
                    0,
                    {0, 0},
                    {0, 2}
                }
            },

            Test{
                L"can move backward within a row",
                { 1, 0 },
                { 2, 0 },
                -1,
                {
                    0,
                    { 1, 0 },
                    { 2, 0 },
                }
            },

            Test{
                L"can move forward in a row",
                { 1, 0 },
                { 10, 0 },
                5,
                {
                    5,
                    {0, 4},
                    {0, 5}
                }
            },

            Test{
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

            Test{
                L"can move to a new row when necessary when moving forward",
                { lastColumnIndex, 0 },
                { lastColumnIndex, 0 },
                5,
                {
                    5,
                    { 0, 5 },
                    { 0, 6}
                }
            },

            Test{
                L"can move to a new row when necessary when moving backward",
                { 0, 1 },
                { 5, 1 },
                -5,
                {
                    0,
                    { 0, 1 },
                    { 5, 1 }
                }
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (const auto& test : testData)
        {
            Log::Comment(test.comment.data());
            int amountMoved;

            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end);
            utr->Move(TextUnit::TextUnit_Word, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveByWord_NonEmptyBuffer)
    {
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        const std::wstring_view text[] = {
            L"word other",
            L"  more words"
        };

        for (auto i = 0; i < 2; i++)
        {
            _pTextBuffer->WriteLine(text[i], { 0, gsl::narrow<SHORT>(i) });
        }

        struct ExpectedResult
        {
            int moveAmt;
            COORD start;
            COORD end;
        };

        struct Test
        {
            std::wstring comment;
            COORD start;
            COORD end;
            int moveAmt;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
                L"move backwards on the word by (0,0)",
                { 1, 0 },
                { 2, 0 },
                -1,
                {
                    -1,
                    { 0, 0 },
                    { 6, 0 }
                }
            }

                //L"get next word while on first word",
                //L"get next word twice while on first word",
                //L"move forward to next row with word",
                //L"move backwards to previous row with word",
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (const auto& test : testData)
        {
            Log::Comment(test.comment.data());
            int amountMoved;

            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end);
            utr->Move(TextUnit::TextUnit_Word, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveByLine)
    {
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        struct ExpectedResult
        {
            int moveAmt;
            COORD start;
            COORD end;
        };

        struct Test
        {
            std::wstring comment;
            COORD start;
            COORD end;
            int moveAmt;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
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

            Test{
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

            Test{
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

            Test{
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

            Test{
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

            Test{
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

            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end);
            utr->Move(TextUnit::TextUnit_Line, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitCharacter)
    {
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = static_cast<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        struct ExpectedResult
        {
            int moveAmt;
            COORD start;
            COORD end;
        };

        struct Test
        {
            std::wstring comment;
            COORD start;
            COORD end;
            int moveAmt;
            TextPatternRangeEndpoint endpoint;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
                L"can't move _start past the beginning of the document when _start is positioned at the beginning",
                {0, 0},
                {lastColumnIndex, 0},
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    0,
                    {0, 0},
                    {lastColumnIndex, 0}
                }
            },

            Test{
                L"can partially move _start to the begining of the document when it is closer than the move count requested",
                {3, 0},
                {lastColumnIndex, 0},
                -5,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    -3,
                    {0, 0},
                    {lastColumnIndex, 0}
                }
            },

            Test{
                L"can't move _end past the begining of the document",
                {0, 0},
                {4, 0},
                -5,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    -4,
                    {0, 0},
                    {0, 0}
                }
            },

            Test{
                L"_start follows _end when passed during movement",
                {5, 0},
                {10, 0},
                -7,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    -7,
                    {3, 0},
                    {3, 0}
                }
            },

            Test{
                L"can't move _end past the beginning of the document when _end is positioned at the end",
                {0, bottomRow},
                {0, bottomRow+1},
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    0,
                    {0, bottomRow},
                    {0, bottomRow+1},
                }
            },

            Test{
                L"can partially move _end to the end of the document when it is closer than the move count requested",
                {0, 0},
                {lastColumnIndex - 3, bottomRow},
                5,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    4,
                    {0, 0},
                    {0, bottomRow+1},
                }
            },

            Test{
                L"can't move _start past the end of the document",
                {lastColumnIndex - 4, bottomRow},
                {0, bottomRow+1},
                5,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    5,
                    {0, bottomRow+1},
                    {0, bottomRow+1},
                }
            },

            Test{
                L"_end follows _start when passed during movement",
                {5, 0},
                {10, 0},
                7,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
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

            //if (test.comment == L"can't move _end past the beginning of the document when _end is positioned at the end")
            //    DebugBreak();

            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end);
            utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Character, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    //TEST_METHOD(CanMoveEndpointByUnitWord)
    //{
    //    const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
    //    const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

    //    const std::wstring_view text[] = {
    //        L"word1  word2   word3",
    //        L"word4  word5   word6"
    //    };

    //    for (auto i = 0; i < 2; i++)
    //    {
    //        _pTextBuffer->WriteLine(text[i], { 0, gsl::narrow<SHORT>(i) });
    //    }

    //    // clang-format off
    //    const std::vector<std::tuple<std::wstring,
    //                                 UiaTextRange::MoveState,
    //                                 int, // amount to move
    //                                 int, // amount actually moved
    //                                 TextPatternRangeEndpoint, // endpoint to move
    //                                 Endpoint, // start
    //                                 Endpoint, // end
    //                                 bool // degenerate
    //                                 >> testData =
    //    {
    //        {
    //            L"can't move _start past the beginning of the document when _start is positioned at the beginning",
    //            {
    //                0, 0,
    //                0, lastColumnIndex,
    //                0,
    //                lastColumnIndex,
    //                0,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -1,
    //            0,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + 0,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"can partially move _start to the begining of the document when it is closer than the move count requested",
    //            {
    //                0, 15,
    //                0, lastColumnIndex,
    //                0,
    //                lastColumnIndex,
    //                0,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -5,
    //            -2,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + 0,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"can't move _end past the begining of the document",
    //            {
    //                0, 0,
    //                0, 2,
    //                0,
    //                lastColumnIndex,
    //                0,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -2,
    //            -1,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + 0,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + 0,
    //            false
    //        },

    //        {
    //            L"_start follows _end when passed during movement",
    //            {
    //                0 + 1, 0 + 2,
    //                0 + 1, 0 + 10,
    //                0,
    //                lastColumnIndex,
    //                0,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -4,
    //            -4,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + 6,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + 6,
    //            true
    //        },

    //        {
    //            L"can't move _end past the beginning of the document when _end is positioned at the end",
    //            {
    //                bottomRow, 0,
    //                bottomRow, lastColumnIndex,
    //                bottomRow,
    //                0,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            1,
    //            0,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + 0,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"can partially move _end to the end of the document when it is closer than the move count requested",
    //            {
    //                0, 0,
    //                bottomRow, lastColumnIndex - 3,
    //                bottomRow,
    //                0,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            5,
    //            1,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0) + 0,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"can't move _start past the end of the document",
    //            {
    //                bottomRow, lastColumnIndex - 4,
    //                bottomRow, lastColumnIndex,
    //                bottomRow,
    //                0,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            5,
    //            1,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"_end follows _start when passed during movement",
    //            {
    //                0, 0,
    //                0, 0 + 3,
    //                0,
    //                lastColumnIndex,
    //                0,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            2,
    //            2,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0)+15,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 0)+15,
    //            true
    //        },
    //    };
    //    // clang-format on

    //    for (auto data : testData)
    //    {
    //        Log::Comment(std::get<0>(data).c_str());
    //        std::tuple<Endpoint, Endpoint, bool> result;
    //        int amountMoved;
    //        result = UiaTextRange::_moveEndpointByUnitWord(_pUiaData,
    //                                                       std::get<2>(data),
    //                                                       std::get<4>(data),
    //                                                       std::get<1>(data),
    //                                                       L" ",
    //                                                       &amountMoved);

    //        VERIFY_ARE_EQUAL(std::get<3>(data), amountMoved);
    //        VERIFY_ARE_EQUAL(std::get<5>(data), std::get<0>(result));
    //        VERIFY_ARE_EQUAL(std::get<6>(data), std::get<1>(result));
    //        VERIFY_ARE_EQUAL(std::get<7>(data), std::get<2>(result));
    //    }
    //}

    TEST_METHOD(CanMoveEndpointByUnitLine)
    {
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        struct ExpectedResult
        {
            int moveAmt;
            COORD start;
            COORD end;
        };

        struct Test
        {
            std::wstring comment;
            COORD start;
            COORD end;
            int moveAmt;
            TextPatternRangeEndpoint endpoint;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
                L"can move _end forward without affecting _start",
                {0, 0},
                {lastColumnIndex, 0},
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                1,
                {0, 0},
                {0, 1}
            },

            Test{
                L"can move _end backward without affecting _start",
                {0, 1},
                {lastColumnIndex, 5},
                -2,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                -2,
                {0, 1},
                {0, 4}
            },

            Test{
                L"can move _start forward without affecting _end",
                {0, 1},
                {lastColumnIndex, 5},
                2,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                2,
                {0, 3},
                {lastColumnIndex, 5}
            },

            Test{
                L"can move _start backward without affecting _end",
                {0, 2},
                {lastColumnIndex, 5},
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                -1,
                {0, 1},
                {lastColumnIndex, 5}
            },

            Test{
                L"can move _start backwards when it's already on the top row",
                {lastColumnIndex, 0},
                {lastColumnIndex, 0},
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                -1,
                {0, 0},
                {lastColumnIndex, 0},
            },

            Test{
                L"can't move _start backwards when it's at the start of the document already",
                {0, 0},
                {lastColumnIndex, 0},
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                0,
                {0, 0},
                {lastColumnIndex, 0}
            },

            Test{
                L"can move _end forwards when it's on the bottom row",
                {0, 0},
                {lastColumnIndex - 3, bottomRow},
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                1,
                {0, 0},
                {0, bottomRow+1}
            },

            Test{
                L"can't move _end forwards when it's at the end of the document already",
                {0, 0},
                {0, bottomRow+1},
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                0,
                {0, 0},
                {0, bottomRow+1}
            },

            Test{
                L"moving _start forward when it's already on the bottom row creates a degenerate range at the document end",
                {0, bottomRow},
                {lastColumnIndex, bottomRow},
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                1,
                {0, bottomRow+1},
                {0, bottomRow+1}
            },

            Test{
                L"moving _end backward when it's already on the top row creates a degenerate range at the document start",
                {4, 0},
                {lastColumnIndex - 5, 0},
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
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

            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end);
            utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Line, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitDocument)
    {
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT bottomRow = gsl::narrow<SHORT>(_pTextBuffer->TotalRowCount() - 1);

        struct ExpectedResult
        {
            int moveAmt;
            COORD start;
            COORD end;
        };

        struct Test
        {
            std::wstring comment;
            COORD start;
            COORD end;
            int moveAmt;
            TextPatternRangeEndpoint endpoint;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData =
        {
            Test{
                L"can move _end forward to end of document without affecting _start",
                {0, 4},
                {0, 4},
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    1,
                    {0, 4},
                    {0, bottomRow+1}
                }
            },

            Test{
                L"can move _start backward to end of document without affect _end",
                {0, 4},
                {0, 4},
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    -1,
                    {0, 0},
                    {0, 4}
                }
            },

            Test{
                L"can't move _end forward when it's already at the end of the document",
                {3, 2},
                {0, bottomRow+1},
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    0,
                    {3, 2},
                    {0, bottomRow+1}
                }
            },

            Test{
                L"can't move _start backward when it's already at the start of the document",
                {0, 0},
                {5, 6},
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    0,
                    {0, 0},
                    {5, 6}
                }
            },

            Test{
                L"moving _end backward creates degenerate range at start of document",
                {5, 2},
                {5, 6},
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    -1,
                    {0, 0},
                    {0, 0}
                }
            },

            Test{
                L"moving _start forward creates degenerate range at end of document",
                {5, 2},
                {5, 6},
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
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

            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, test.start, test.end);
            utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Document, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }
};
