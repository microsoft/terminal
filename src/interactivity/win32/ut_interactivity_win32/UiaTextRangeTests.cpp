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
                cell.Char() = L'a';
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

        // make a degenerate range and verify that it reports degenerate
        Microsoft::WRL::ComPtr<UiaTextRange> degenerate;
        Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&degenerate,
                                                        _pUiaData,
                                                        &_dummyProvider,
                                                        bufferSize.Origin(),
                                                        bufferSize.Origin());
        VERIFY_IS_TRUE(degenerate->IsDegenerate());
        VERIFY_ARE_EQUAL(degenerate->_start, degenerate->_end);

        // make a non-degenerate range and verify that it reports as such
        const COORD end = { bufferSize.Origin().X + 1, bufferSize.Origin().Y };
        Microsoft::WRL::ComPtr<UiaTextRange> notDegenerate;
        Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&notDegenerate,
                                                        _pUiaData,
                                                        &_dummyProvider,
                                                        bufferSize.Origin(),
                                                        end);
        VERIFY_IS_FALSE(notDegenerate->IsDegenerate());
        VERIFY_ARE_NOT_EQUAL(degenerate->_start, degenerate->_end);
    }

    TEST_METHOD(CanMoveByCharacter)
    {
        const SHORT firstColumnIndex = 0;
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT topRow = 0;
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
            UiaTextRange::MoveState moveState;
            int moveAmt;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
                L"can't move backward from (0, 0)",
                {
                    { 0, 0 },
                    { 0, 2 },
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                {
                    0,
                    {0,0},
                    {0,0}
                }
            },

            Test{
                L"can move backward within a row",
                {
                    { 0, 1 },
                    { 0, 2 },
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                {
                    -1,
                    {0,0},
                    {0,0}
                }
            },

            Test{
                L"can move forward in a row",
                {
                    { 2, 1 },
                    { 4, 5 },
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                {
                    5,
                    {2,6},
                    {2,6}
                }
            },

            Test{
                L"can't move past the last column in the last row",
                {
                    { bottomRow, lastColumnIndex },
                    { bottomRow, lastColumnIndex },
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                {
                    0,
                    {bottomRow, lastColumnIndex},
                    {bottomRow, lastColumnIndex}
                }
            },

            Test{
                L"can move to a new row when necessary when moving forward",
                {
                    { topRow, lastColumnIndex },
                    { topRow, lastColumnIndex },
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                {
                    5,
                    {topRow + 1, 4},
                    {topRow + 1, 4}
                }
            },

            Test{
                L"can move to a new row when necessary when moving backward",
                {
                    { topRow + 1, firstColumnIndex },
                    { topRow + 1, lastColumnIndex },
                    UiaTextRange::MovementDirection::Backward
                },
                -5,
                {
                    -5,
                    {topRow, lastColumnIndex - 4},
                    {topRow, lastColumnIndex - 4}
                }
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (const auto &test : testData)
        {
            Log::Comment(test.comment.data());
            int amountMoved;

            const auto moveState = test.moveState;
            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, moveState.start, moveState.end);
            utr->Move(TextUnit::TextUnit_Character, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    //TEST_METHOD(CanMoveByWord_EmptyBuffer)
    //{
    //    const Column firstColumnIndex = 0;
    //    const Column lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
    //    const ScreenInfoRow topRow = 0;
    //    const ScreenInfoRow bottomRow = _pTextBuffer->TotalRowCount() - 1;

    //    // clang-format off
    //    const std::vector<std::tuple<std::wstring,
    //                                 UiaTextRange::MoveState,
    //                                 int, // amount to move
    //                                 int, // amount actually moved
    //                                 Endpoint, // start
    //                                 Endpoint // end
    //                                 >> testData =
    //    {
    //        {
    //            L"can't move backward from (0, 0)",
    //            {
    //                0, 0,
    //                0, 2,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -1,
    //            0,
    //            0u,
    //            lastColumnIndex
    //        },

    //        {
    //            L"can move backward within a row",
    //            {
    //                0, 1,
    //                0, 2,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -1,
    //            -1,
    //            0u,
    //            lastColumnIndex
    //        },

    //        {
    //            L"can move forward in a row",
    //            {
    //                2, 1,
    //                4, 5,
    //                bottomRow,
    //                firstColumnIndex,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            5,
    //            5,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 8),
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 8) + lastColumnIndex
    //        },

    //        {
    //            L"can't move past the last column in the last row",
    //            {
    //                bottomRow, lastColumnIndex,
    //                bottomRow, lastColumnIndex,
    //                bottomRow,
    //                firstColumnIndex,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            5,
    //            0,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow),
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex
    //        },

    //        {
    //            L"can move to a new row when necessary when moving forward",
    //            {
    //                topRow, lastColumnIndex,
    //                topRow, lastColumnIndex,
    //                bottomRow,
    //                firstColumnIndex,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            5,
    //            5,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 5),
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 5) + lastColumnIndex
    //        },

    //        {
    //            L"can move to a new row when necessary when moving backward",
    //            {
    //                topRow + 1, firstColumnIndex,
    //                topRow + 1, lastColumnIndex,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -5,
    //            -2,
    //            0u,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex
    //        }
    //    };
    //    // clang-format on

    //    for (auto data : testData)
    //    {
    //        Log::Comment(std::get<0>(data).c_str());
    //        int amountMoved;
    //        std::pair<Endpoint, Endpoint> newEndpoints = UiaTextRange::_moveByWord(_pUiaData,
    //                                                                               std::get<2>(data),
    //                                                                               std::get<1>(data),
    //                                                                               L"",
    //                                                                               &amountMoved);

    //        VERIFY_ARE_EQUAL(std::get<3>(data), amountMoved);
    //        VERIFY_ARE_EQUAL(std::get<4>(data), newEndpoints.first);
    //        VERIFY_ARE_EQUAL(std::get<5>(data), newEndpoints.second);
    //    }
    //}

    //TEST_METHOD(CanMoveByWord_NonEmptyBuffer)
    //{
    //    const Column firstColumnIndex = 0;
    //    const Column lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
    //    const ScreenInfoRow topRow = 0;
    //    const ScreenInfoRow bottomRow = _pTextBuffer->TotalRowCount() - 1;

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
    //                                 Endpoint, // start
    //                                 Endpoint // end
    //                                 >> testData =
    //    {
    //        {
    //            L"move backwards on the word by (0,0)",
    //            {
    //                0, 1,
    //                0, 2,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -1,
    //            -1,
    //            0u,
    //            6
    //        },

    //        {
    //            L"get next word while on first word",
    //            {
    //                0, 0,
    //                0, 0,
    //                bottomRow,
    //                firstColumnIndex,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            1,
    //            1,
    //            0,
    //            6
    //        },

    //        {
    //            L"get next word twice while on first word",
    //            {
    //                0, 0,
    //                0, 0,
    //                bottomRow,
    //                firstColumnIndex,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            2,
    //            2,
    //            7,
    //            14
    //        },

    //        {
    //            L"move forward to next row with word",
    //            {
    //                topRow, lastColumnIndex,
    //                topRow, lastColumnIndex,
    //                bottomRow,
    //                firstColumnIndex,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            1,
    //            1,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 1),
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 1) + 6
    //        },

    //        {
    //            L"move backwards to previous row with word",
    //            {
    //                topRow + 1, firstColumnIndex,
    //                topRow + 1, lastColumnIndex,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -1,
    //            -1,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + 15,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex
    //        }
    //    };
    //    // clang-format on

    //    for (auto data : testData)
    //    {
    //        Log::Comment(std::get<0>(data).c_str());
    //        int amountMoved;
    //        std::pair<Endpoint, Endpoint> newEndpoints = UiaTextRange::_moveByWord(_pUiaData,
    //                                                                               std::get<2>(data),
    //                                                                               std::get<1>(data),
    //                                                                               L"",
    //                                                                               &amountMoved);

    //        VERIFY_ARE_EQUAL(std::get<3>(data), amountMoved);
    //        VERIFY_ARE_EQUAL(std::get<4>(data), newEndpoints.first);
    //        VERIFY_ARE_EQUAL(std::get<5>(data), newEndpoints.second);
    //    }
    //}

    TEST_METHOD(CanMoveByLine)
    {
        const SHORT firstColumnIndex = 0;
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT topRow = 0;
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
            UiaTextRange::MoveState moveState;
            int moveAmt;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
                L"can't move backward from top row",
                {
                    {topRow, firstColumnIndex},
                    {topRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -4,
                {
                    0,
                    {topRow, firstColumnIndex},
                    {topRow, lastColumnIndex}
                }
            },

            Test{
                L"can move forward from top row",
                {
                    {topRow, firstColumnIndex},
                    {topRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                4,
                {
                    4,
                    {topRow + 4, firstColumnIndex},
                    {topRow + 4, lastColumnIndex}
                }
            },

            Test{
                L"can't move forward from bottom row",
                {
                    {bottomRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                3,
                {
                    0,
                    {bottomRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex}
                }
            },

            Test{
                L"can move backward from bottom row",
                {
                    {bottomRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -3,
                {
                    -3,
                    {bottomRow - 3, firstColumnIndex},
                    {bottomRow - 3, lastColumnIndex}
                }
            },

            Test{
                L"can't move backward when part of the top row is in the range",
                {
                    {topRow, firstColumnIndex + 5},
                    {topRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                {
                    0,
                    {topRow, firstColumnIndex + 5},
                    {topRow, lastColumnIndex}
                }
            },

            Test{
                L"can't move forward when part of the bottom row is in the range",
                {
                    {bottomRow, firstColumnIndex},
                    {bottomRow, firstColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                {
                    0,
                    {bottomRow, firstColumnIndex},
                    {bottomRow, firstColumnIndex}
                }
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (auto test : testData)
        {
            Log::Comment(test.comment.c_str());
            int amountMoved;

            const auto moveState = test.moveState;
            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, moveState.start, moveState.end);
            utr->Move(TextUnit::TextUnit_Line, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitCharacter)
    {
        const SHORT firstColumnIndex = 0;
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT topRow = 0;
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
            UiaTextRange::MoveState moveState;
            int moveAmt;
            TextPatternRangeEndpoint endpoint;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
                L"can't move _start past the beginning of the document when _start is positioned at the beginning",
                {
                    {topRow, firstColumnIndex},
                    {topRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    0,
                    {topRow, firstColumnIndex},
                    {topRow, lastColumnIndex}
                }
            },

            Test{
                L"can partially move _start to the begining of the document when it is closer than the move count requested",
                {
                    {topRow, firstColumnIndex + 3},
                    {topRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -5,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    -3,
                    {topRow, firstColumnIndex},
                    {topRow, lastColumnIndex}
                }
            },

            Test{
                L"can't move _end past the begining of the document",
                {
                    {topRow, firstColumnIndex},
                    {topRow, firstColumnIndex + 4},
                    UiaTextRange::MovementDirection::Backward
                },
                -5,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    -4,
                    {topRow, firstColumnIndex},
                    {topRow, firstColumnIndex}
                }
            },

            Test{
                L"_start follows _end when passed during movement",
                {
                    {topRow, firstColumnIndex + 5},
                    {topRow, firstColumnIndex + 10},
                    UiaTextRange::MovementDirection::Backward
                },
                -7,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    -7,
                    {topRow, 3},
                    {topRow, 3}
                }
            },

            Test{
                L"can't move _end past the beginning of the document when _end is positioned at the end",
                {
                    {bottomRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    0,
                    {bottomRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex}
                }
            },

            Test{
                L"can partially move _end to the end of the document when it is closer than the move count requested",
                {
                    {topRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex - 3},
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    3,
                    {topRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex}
                }
            },

            Test{
                L"can't move _start past the end of the document",
                {
                    {bottomRow, lastColumnIndex - 4},
                    {bottomRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    4,
                    {bottomRow, lastColumnIndex},
                    {bottomRow, lastColumnIndex}
                }
            },

            Test{
                L"_end follows _start when passed during movement",
                {
                    {topRow, firstColumnIndex + 5},
                    {topRow, firstColumnIndex + 10},
                    UiaTextRange::MovementDirection::Forward
                },
                7,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    7,
                    {topRow, 12},
                    {topRow, 12}
                }
            },
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (auto test : testData)
        {
            Log::Comment(test.comment.c_str());
            int amountMoved;

            const auto moveState = test.moveState;
            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, moveState.start, moveState.end);
            utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Character, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    //TEST_METHOD(CanMoveEndpointByUnitWord)
    //{
    //    const Column firstColumnIndex = 0;
    //    const Column lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
    //    const ScreenInfoRow topRow = 0;
    //    const ScreenInfoRow bottomRow = _pTextBuffer->TotalRowCount() - 1;

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
    //                topRow, firstColumnIndex,
    //                topRow, lastColumnIndex,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -1,
    //            0,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"can partially move _start to the begining of the document when it is closer than the move count requested",
    //            {
    //                topRow, firstColumnIndex + 15,
    //                topRow, lastColumnIndex,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -5,
    //            -2,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"can't move _end past the begining of the document",
    //            {
    //                topRow, firstColumnIndex,
    //                topRow, firstColumnIndex + 2,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -2,
    //            -1,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
    //            false
    //        },

    //        {
    //            L"_start follows _end when passed during movement",
    //            {
    //                topRow + 1, firstColumnIndex + 2,
    //                topRow + 1, firstColumnIndex + 10,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Backward,
    //                UiaTextRange::MovementDirection::Backward
    //            },
    //            -4,
    //            -4,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + 6,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + 6,
    //            true
    //        },

    //        {
    //            L"can't move _end past the beginning of the document when _end is positioned at the end",
    //            {
    //                bottomRow, firstColumnIndex,
    //                bottomRow, lastColumnIndex,
    //                bottomRow,
    //                firstColumnIndex,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            1,
    //            0,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + firstColumnIndex,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"can partially move _end to the end of the document when it is closer than the move count requested",
    //            {
    //                topRow, firstColumnIndex,
    //                bottomRow, lastColumnIndex - 3,
    //                bottomRow,
    //                firstColumnIndex,
    //                lastColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            5,
    //            1,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
    //            false
    //        },

    //        {
    //            L"can't move _start past the end of the document",
    //            {
    //                bottomRow, lastColumnIndex - 4,
    //                bottomRow, lastColumnIndex,
    //                bottomRow,
    //                firstColumnIndex,
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
    //                topRow, firstColumnIndex,
    //                topRow, firstColumnIndex + 3,
    //                topRow,
    //                lastColumnIndex,
    //                firstColumnIndex,
    //                UiaTextRange::MovementIncrement::Forward,
    //                UiaTextRange::MovementDirection::Forward
    //            },
    //            2,
    //            2,
    //            TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow)+15,
    //            UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow)+15,
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
        const SHORT firstColumnIndex = 0;
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT topRow = 0;
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
            UiaTextRange::MoveState moveState;
            int moveAmt;
            TextPatternRangeEndpoint endpoint;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData
        {
            Test{
                L"can move _end forward without affecting _start",
                {
                    {topRow, firstColumnIndex},
                    {topRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                1,
                {topRow, firstColumnIndex},
                {topRow, lastColumnIndex}
            },

            Test{
                L"can move _end backward without affecting _start",
                {
                    {topRow + 1, firstColumnIndex},
                    {topRow + 5, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -2,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                -2,
                {topRow + 1, firstColumnIndex},
                {topRow + 3, lastColumnIndex}
            },

            Test{
                L"can move _start forward without affecting _end",
                {
                    {topRow + 1, firstColumnIndex},
                    {topRow + 5, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                2,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                2,
                {topRow + 3, firstColumnIndex},
                {topRow + 5, lastColumnIndex}
            },

            Test{
                L"can move _start backward without affecting _end",
                {
                    {topRow + 2, firstColumnIndex},
                    {topRow + 5, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                -1,
                {topRow + 1, firstColumnIndex},
                {topRow + 5, lastColumnIndex}
            },

            Test{
                L"can move _start backwards when it's already on the top row",
                {
                    {topRow, lastColumnIndex},
                    {topRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                -1,
                {topRow, firstColumnIndex},
                {topRow, lastColumnIndex}
            },

            Test{
                L"can't move _start backwards when it's at the start of the document already",
                {
                    {topRow, firstColumnIndex},
                    {topRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                0,
                {topRow, firstColumnIndex},
                {topRow, lastColumnIndex}
            },

            Test{
                L"can move _end forwards when it's on the bottom row",
                {
                    {topRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex - 3},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                1,
                {topRow, firstColumnIndex},
                {bottomRow, lastColumnIndex}
            },

            Test{
                L"can't move _end forwards when it's at the end of the document already",
                {
                    {topRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                0,
                {topRow, firstColumnIndex},
                {bottomRow, lastColumnIndex}
            },

            Test{
                L"moving _start forward when it's already on the bottom row creates a degenerate range at the document end",
                {
                    {bottomRow, firstColumnIndex},
                    {bottomRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                1,
                {bottomRow, lastColumnIndex},
                {bottomRow, lastColumnIndex}
            },

            Test{
                L"moving _end backward when it's already on the top row creates a degenerate range at the document start",
                {
                    {topRow, firstColumnIndex + 4},
                    {topRow, lastColumnIndex - 5},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                -1,
                {topRow, firstColumnIndex},
                {topRow, firstColumnIndex}
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (auto test : testData)
        {
            Log::Comment(test.comment.c_str());
            int amountMoved;

            const auto moveState = test.moveState;
            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, moveState.start, moveState.end);
            utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Line, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitDocument)
    {
        const SHORT firstColumnIndex = 0;
        const SHORT lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const SHORT topRow = 0;
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
            UiaTextRange::MoveState moveState;
            int moveAmt;
            TextPatternRangeEndpoint endpoint;
            ExpectedResult expected;
        };

        // clang-format off
        const std::vector<Test> testData =
        {
            Test{
                L"can move _end forward to end of document without affecting _start",
                {
                    {topRow, firstColumnIndex + 4},
                    {topRow, firstColumnIndex + 4},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    1,
                    {topRow, firstColumnIndex + 4},
                    {bottomRow, lastColumnIndex}
                }
            },

            Test{
                L"can move _start backward to end of document without affect _end",
                {
                    {topRow, firstColumnIndex + 4},
                    {topRow, firstColumnIndex + 4},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    -1,
                    {topRow, firstColumnIndex},
                    {topRow, 4}
                }
            },

            Test{
                L"can't move _end forward when it's already at the end of the document",
                {
                    {topRow + 3, firstColumnIndex + 2},
                    {bottomRow, lastColumnIndex},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    0,
                    {topRow + 3, firstColumnIndex + 2},
                    {bottomRow, lastColumnIndex}
                }
            },

            Test{
                L"can't move _start backward when it's already at the start of the document",
                {
                    {topRow, firstColumnIndex},
                    {topRow + 5, firstColumnIndex + 6},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    0,
                    {topRow, firstColumnIndex},
                    {topRow + 5, 6}
                }
            },

            Test{
                L"moving _end backward creates degenerate range at start of document",
                {
                    {topRow + 5, firstColumnIndex + 2},
                    {topRow + 5, firstColumnIndex + 6},
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                {
                    -1,
                    {topRow, firstColumnIndex},
                    {topRow, firstColumnIndex}
                }
            },

            Test{
                L"moving _start forward creates degenerate range at end of document",
                {
                    {topRow + 5, firstColumnIndex + 2},
                    {topRow + 5, firstColumnIndex + 6},
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                {
                    1,
                    {bottomRow, lastColumnIndex},
                    {bottomRow, lastColumnIndex}
                }
            }
        };
        // clang-format on

        Microsoft::WRL::ComPtr<UiaTextRange> utr;
        for (auto test : testData)
        {
            Log::Comment(test.comment.c_str());
            int amountMoved;

            const auto moveState = test.moveState;
            Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&utr, _pUiaData, &_dummyProvider, moveState.start, moveState.end);
            utr->MoveEndpointByUnit(test.endpoint, TextUnit::TextUnit_Document, test.moveAmt, &amountMoved);

            VERIFY_ARE_EQUAL(test.expected.moveAmt, amountMoved);
            VERIFY_ARE_EQUAL(test.expected.start, utr->_start);
            VERIFY_ARE_EQUAL(test.expected.end, utr->_end);
        }
    }
};
