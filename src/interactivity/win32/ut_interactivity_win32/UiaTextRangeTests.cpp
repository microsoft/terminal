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
        Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&_range,
                                                        _pUiaData,
                                                        &_dummyProvider,
                                                        0,
                                                        0,
                                                        false);

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

    const size_t _getRowWidth() const
    {
        const CharRow& charRow = _pTextBuffer->_GetFirstRow().GetCharRow();
        return charRow.MeasureRight() - charRow.MeasureLeft();
    }

    TEST_METHOD(DegenerateRangesDetected)
    {
        // make a degenerate range and verify that it reports degenerate
        Microsoft::WRL::ComPtr<UiaTextRange> degenerate;
        Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&degenerate,
                                                        _pUiaData,
                                                        &_dummyProvider,
                                                        20,
                                                        19,
                                                        true);
        VERIFY_IS_TRUE(degenerate->IsDegenerate());
        VERIFY_ARE_EQUAL(0u, degenerate->_rowCountInRange(_pUiaData));
        VERIFY_ARE_EQUAL(degenerate->_start, degenerate->_end);

        // make a non-degenerate range and verify that it reports as such
        Microsoft::WRL::ComPtr<UiaTextRange> notDegenerate1;
        Microsoft::WRL::MakeAndInitialize<UiaTextRange>(&notDegenerate1,
                                                        _pUiaData,
                                                        &_dummyProvider,
                                                        20,
                                                        20,
                                                        false);
        VERIFY_IS_FALSE(notDegenerate1->IsDegenerate());
        VERIFY_ARE_EQUAL(1u, notDegenerate1->_rowCountInRange(_pUiaData));
    }

    TEST_METHOD(CanCheckIfScreenInfoRowIsInViewport)
    {
        // check a viewport that's one line tall
        SMALL_RECT viewport;
        viewport.Top = 0;
        viewport.Bottom = 0;

        VERIFY_IS_TRUE(_range->_isScreenInfoRowInViewport(0, viewport));
        VERIFY_IS_FALSE(_range->_isScreenInfoRowInViewport(1, viewport));

        // check a slightly larger viewport
        viewport.Bottom = 5;
        for (auto i = 0; i <= viewport.Bottom; ++i)
        {
            VERIFY_IS_TRUE(_range->_isScreenInfoRowInViewport(i, viewport),
                           NoThrowString().Format(L"%d should be in viewport", i));
        }
        VERIFY_IS_FALSE(_range->_isScreenInfoRowInViewport(viewport.Bottom + 1, viewport));
    }

    TEST_METHOD(CanTranslateScreenInfoRowToViewport)
    {
        const int totalRows = _pTextBuffer->TotalRowCount();

        SMALL_RECT viewport;
        viewport.Top = 0;
        viewport.Bottom = 10;

        std::vector<std::pair<int, int>> viewportSizes = {
            { 0, 10 }, // viewport at top
            { 2, 10 }, // shifted viewport
            { totalRows - 5, totalRows + 3 } // viewport with 0th row
        };

        for (auto it = viewportSizes.begin(); it != viewportSizes.end(); ++it)
        {
            viewport.Top = static_cast<SHORT>(it->first);
            viewport.Bottom = static_cast<SHORT>(it->second);
            for (int i = viewport.Top; _range->_isScreenInfoRowInViewport(i, viewport); ++i)
            {
                VERIFY_ARE_EQUAL(i - viewport.Top, _range->_screenInfoRowToViewportRow(i, viewport));
            }
        }

        // ScreenInfoRows that are above the viewport return a
        // negative value
        viewport.Top = 5;
        viewport.Bottom = 10;

        VERIFY_ARE_EQUAL(-1, _range->_screenInfoRowToViewportRow(4, viewport));
        VERIFY_ARE_EQUAL(-2, _range->_screenInfoRowToViewportRow(3, viewport));
    }

    TEST_METHOD(CanTranslateEndpointToTextBufferRow)
    {
        const auto rowWidth = _getRowWidth();
        for (auto i = 0; i < 300; ++i)
        {
            VERIFY_ARE_EQUAL(i / rowWidth, _range->_endpointToTextBufferRow(_pUiaData, i));
        }
    }

    TEST_METHOD(CanTranslateTextBufferRowToEndpoint)
    {
        const auto rowWidth = _getRowWidth();
        for (unsigned int i = 0; i < 5; ++i)
        {
            VERIFY_ARE_EQUAL(i * rowWidth, _range->_textBufferRowToEndpoint(_pUiaData, i));
            // make sure that the translation is reversible
            VERIFY_ARE_EQUAL(i, _range->_endpointToTextBufferRow(_pUiaData, _range->_textBufferRowToEndpoint(_pUiaData, i)));
        }
    }

    TEST_METHOD(CanTranslateTextBufferRowToScreenInfoRow)
    {
        const auto rowWidth = _getRowWidth();
        for (unsigned int i = 0; i < 5; ++i)
        {
            VERIFY_ARE_EQUAL(i, _range->_textBufferRowToScreenInfoRow(_pUiaData, _range->_screenInfoRowToTextBufferRow(_pUiaData, i)));
        }
    }

    TEST_METHOD(CanTranslateEndpointToColumn)
    {
        const auto rowWidth = _getRowWidth();
        for (auto i = 0; i < 300; ++i)
        {
            const auto column = i % rowWidth;
            VERIFY_ARE_EQUAL(column, _range->_endpointToColumn(_pUiaData, i));
        }
    }

    TEST_METHOD(CanGetTotalRows)
    {
        const auto totalRows = _pTextBuffer->TotalRowCount();
        VERIFY_ARE_EQUAL(totalRows,
                         _range->_getTotalRows(_pUiaData));
    }

    TEST_METHOD(CanGetRowWidth)
    {
        const auto rowWidth = _getRowWidth();
        VERIFY_ARE_EQUAL(rowWidth, _range->_getRowWidth(_pUiaData));
    }

    TEST_METHOD(CanNormalizeRow)
    {
        const int totalRows = _pTextBuffer->TotalRowCount();
        std::vector<std::pair<unsigned int, unsigned int>> rowMappings = {
            { 0, 0 },
            { totalRows / 2, totalRows / 2 },
            { totalRows - 1, totalRows - 1 },
            { totalRows, 0 },
            { totalRows + 1, 1 },
            { -1, totalRows - 1 }
        };

        for (auto it = rowMappings.begin(); it != rowMappings.end(); ++it)
        {
            VERIFY_ARE_EQUAL(static_cast<int>(it->second), _range->_normalizeRow(_pUiaData, it->first));
        }
    }

    TEST_METHOD(CanGetViewportHeight)
    {
        SMALL_RECT viewport;
        viewport.Top = 0;
        viewport.Bottom = 0;

        // Viewports are inclusive, so Top == Bottom really means 1 row
        VERIFY_ARE_EQUAL(1u, _range->_getViewportHeight(viewport));

        // make the viewport 10 rows tall
        viewport.Top = 3;
        viewport.Bottom = 12;
        VERIFY_ARE_EQUAL(10u, _range->_getViewportHeight(viewport));
    }

    TEST_METHOD(CanGetViewportWidth)
    {
        SMALL_RECT viewport;
        viewport.Left = 0;
        viewport.Right = 0;

        // Viewports are inclusive, Left == Right is really 1 column
        VERIFY_ARE_EQUAL(1u, _range->_getViewportWidth(viewport));

        // test a more normal size
        viewport.Right = 300;
        VERIFY_ARE_EQUAL(viewport.Right + 1u, _range->_getViewportWidth(viewport));
    }

    TEST_METHOD(CanCompareScreenCoords)
    {
        const std::vector<std::tuple<ScreenInfoRow, Column, ScreenInfoRow, Column, int>> testData = {
            { 0, 0, 0, 0, 0 },
            { 5, 0, 5, 0, 0 },
            { 2, 3, 2, 3, 0 },
            { 0, 6, 0, 6, 0 },
            { 1, 5, 2, 5, -1 },
            { 5, 4, 7, 3, -1 },
            { 3, 4, 3, 5, -1 },
            { 2, 0, 1, 9, 1 },
            { 4, 5, 4, 3, 1 }
        };

        for (auto data : testData)
        {
            VERIFY_ARE_EQUAL(std::get<4>(data),
                             UiaTextRange::_compareScreenCoords(_pUiaData,
                                                                std::get<0>(data),
                                                                std::get<1>(data),
                                                                std::get<2>(data),
                                                                std::get<3>(data)));
        }
    }

    TEST_METHOD(CanMoveByCharacter)
    {
        const Column firstColumnIndex = 0;
        const Column lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const ScreenInfoRow topRow = 0;
        const ScreenInfoRow bottomRow = _pTextBuffer->TotalRowCount() - 1;

        // clang-format off
        const std::vector<std::tuple<std::wstring,
                                     UiaTextRange::MoveState,
                                     int, // amount to move
                                     int, // amount actually moved
                                     Endpoint, // start
                                     Endpoint // end
                                     >> testData =
        {
            {
                L"can't move backward from (0, 0)",
                {
                    0, 0,
                    0, 2,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                0,
                0u,
                0u
            },

            {
                L"can move backward within a row",
                {
                    0, 1,
                    0, 2,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                -1,
                0u,
                0u
            },

            {
                L"can move forward in a row",
                {
                    2, 1,
                    4, 5,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                5,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 2) + 6,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, 2) + 6
            },

            {
                L"can't move past the last column in the last row",
                {
                    bottomRow, lastColumnIndex,
                    bottomRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                0,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex
            },

            {
                L"can move to a new row when necessary when moving forward",
                {
                    topRow, lastColumnIndex,
                    topRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                5,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 1) + 4,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 1) + 4
            },

            {
                L"can move to a new row when necessary when moving backward",
                {
                    topRow + 1, firstColumnIndex,
                    topRow + 1, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -5,
                -5,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + (lastColumnIndex - 4),
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + (lastColumnIndex - 4)
            }
        };
        // clang-format on

        for (auto data : testData)
        {
            Log::Comment(std::get<0>(data).c_str());
            int amountMoved;
            std::pair<Endpoint, Endpoint> newEndpoints = UiaTextRange::_moveByCharacter(_pUiaData,
                                                                                        std::get<2>(data),
                                                                                        std::get<1>(data),
                                                                                        &amountMoved);

            VERIFY_ARE_EQUAL(std::get<3>(data), amountMoved);
            VERIFY_ARE_EQUAL(std::get<4>(data), newEndpoints.first);
            VERIFY_ARE_EQUAL(std::get<5>(data), newEndpoints.second);
        }
    }

    TEST_METHOD(CanMoveByLine)
    {
        const Column firstColumnIndex = 0;
        const Column lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const ScreenInfoRow topRow = 0;
        const ScreenInfoRow bottomRow = _pTextBuffer->TotalRowCount() - 1;

        // clang-format off
        const std::vector<std::tuple<std::wstring,
                                     UiaTextRange::MoveState,
                                     int, // amount to move
                                     int, // amount actually moved
                                     Endpoint, // start
                                     Endpoint // end
                                     >> testData =
        {
            {
                L"can't move backward from top row",
                {
                    topRow, firstColumnIndex,
                    topRow, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -4,
                0,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex
            },

            {
                L"can move forward from top row",
                {
                    topRow, firstColumnIndex,
                    topRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                4,
                4,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 4) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 4) + lastColumnIndex
            },

            {
                L"can't move forward from bottom row",
                {
                    bottomRow, firstColumnIndex,
                    bottomRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                3,
                0,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex
            },

            {
                L"can move backward from bottom row",
                {
                    bottomRow, firstColumnIndex,
                    bottomRow, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -3,
                -3,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow - 3) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow - 3) + lastColumnIndex
            },

            {
                L"can't move backward when part of the top row is in the range",
                {
                    topRow, firstColumnIndex + 5,
                    topRow, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                0,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex + 5,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex
            },

            {
                L"can't move forward when part of the bottom row is in the range",
                {
                    bottomRow, firstColumnIndex,
                    bottomRow, firstColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                0,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + firstColumnIndex
            }
        };
        // clang-format on

        for (auto data : testData)
        {
            Log::Comment(std::get<0>(data).c_str());
            int amountMoved;
            std::pair<Endpoint, Endpoint> newEndpoints = UiaTextRange::_moveByLine(_pUiaData,
                                                                                   std::get<2>(data),
                                                                                   std::get<1>(data),
                                                                                   &amountMoved);

            VERIFY_ARE_EQUAL(std::get<3>(data), amountMoved);
            VERIFY_ARE_EQUAL(std::get<4>(data), newEndpoints.first);
            VERIFY_ARE_EQUAL(std::get<5>(data), newEndpoints.second);
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitCharacter)
    {
        const Column firstColumnIndex = 0;
        const Column lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const ScreenInfoRow topRow = 0;
        const ScreenInfoRow bottomRow = _pTextBuffer->TotalRowCount() - 1;

        // clang-format off
        const std::vector<std::tuple<std::wstring,
                                     UiaTextRange::MoveState,
                                     int, // amount to move
                                     int, // amount actually moved
                                     TextPatternRangeEndpoint, // endpoint to move
                                     Endpoint, // start
                                     Endpoint, // end
                                     bool // degenerate
                                     >> testData =
        {
            {
                L"can't move _start past the beginning of the document when _start is positioned at the beginning",
                {
                    topRow, firstColumnIndex,
                    topRow, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                0,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex,
                false
            },

            {
                L"can partially move _start to the begining of the document when it is closer than the move count requested",
                {
                    topRow, firstColumnIndex + 3,
                    topRow, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -5,
                -3,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex,
                false
            },

            {
                L"can't move _end past the begining of the document",
                {
                    topRow, firstColumnIndex,
                    topRow, firstColumnIndex + 4,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -5,
                -4,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                false
            },

            {
                L"_start follows _end when passed during movement",
                {
                    topRow, firstColumnIndex + 5,
                    topRow, firstColumnIndex + 10,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -7,
                -7,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + 3,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + 3,
                true
            },

            {
                L"can't move _end past the beginning of the document when _end is positioned at the end",
                {
                    bottomRow, firstColumnIndex,
                    bottomRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                0,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                false
            },

            {
                L"can partially move _end to the end of the document when it is closer than the move count requested",
                {
                    topRow, firstColumnIndex,
                    bottomRow, lastColumnIndex - 3,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                3,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                false
            },

            {
                L"can't move _start past the end of the document",
                {
                    bottomRow, lastColumnIndex - 4,
                    bottomRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                5,
                4,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                false
            },

            {
                L"_end follows _start when passed during movement",
                {
                    topRow, firstColumnIndex + 5,
                    topRow, firstColumnIndex + 10,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                7,
                7,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + 12,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + 12,
                true
            },
        };
        // clang-format on

        for (auto data : testData)
        {
            Log::Comment(std::get<0>(data).c_str());
            std::tuple<Endpoint, Endpoint, bool> result;
            int amountMoved;
            result = UiaTextRange::_moveEndpointByUnitCharacter(_pUiaData,
                                                                std::get<2>(data),
                                                                std::get<4>(data),
                                                                std::get<1>(data),
                                                                &amountMoved);

            VERIFY_ARE_EQUAL(std::get<3>(data), amountMoved);
            VERIFY_ARE_EQUAL(std::get<5>(data), std::get<0>(result));
            VERIFY_ARE_EQUAL(std::get<6>(data), std::get<1>(result));
            VERIFY_ARE_EQUAL(std::get<7>(data), std::get<2>(result));
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitLine)
    {
        const Column firstColumnIndex = 0;
        const Column lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const ScreenInfoRow topRow = 0;
        const ScreenInfoRow bottomRow = _pTextBuffer->TotalRowCount() - 1;

        // clang-format off
        const std::vector<std::tuple<std::wstring,
                                     UiaTextRange::MoveState,
                                     int, // amount to move
                                     int, // amount actually moved
                                     TextPatternRangeEndpoint, // endpoint to move
                                     Endpoint, // start
                                     Endpoint, // end
                                     bool // degenerate
                                     >> testData =
        {
            {
                L"can move _end forward without affecting _start",
                {
                    topRow, firstColumnIndex,
                    topRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 1) + lastColumnIndex,
                false
            },

            {
                L"can move _end backward without affecting _start",
                {
                    topRow + 1, firstColumnIndex,
                    topRow + 5, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -2,
                -2,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 1) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 3) + lastColumnIndex,
                false
            },

            {
                L"can move _start forward without affecting _end",
                {
                    topRow + 1, firstColumnIndex,
                    topRow + 5, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                2,
                2,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 3) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 5) + lastColumnIndex,
                false
            },

            {
                L"can move _start backward without affecting _end",
                {
                    topRow + 2, firstColumnIndex,
                    topRow + 5, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 1) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 5) + lastColumnIndex,
                false
            },

            {
                L"can move _start backwards when it's already on the top row",
                {
                    topRow, lastColumnIndex,
                    topRow, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex,
                false
            },

            {
                L"can't move _start backwards when it's at the start of the document already",
                {
                    topRow, firstColumnIndex,
                    topRow, lastColumnIndex,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                0,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + lastColumnIndex,
                false
            },

            {
                L"can move _end forwards when it's on the bottom row",
                {
                    topRow, firstColumnIndex,
                    bottomRow, lastColumnIndex - 3,
                    bottomRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                false
            },

            {
                L"can't move _end forwards when it's at the end of the document already",
                {
                    topRow, firstColumnIndex,
                    bottomRow, lastColumnIndex,
                    bottomRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                0,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                false
            },

            {
                L"moving _start forward when it's already on the bottom row creates a degenerate range at the document end",
                {
                    bottomRow, firstColumnIndex,
                    bottomRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                true
            },

            {
                L"moving _end backward when it's already on the top row creates a degenerate range at the document start",
                {
                    topRow, firstColumnIndex + 4,
                    topRow, lastColumnIndex - 5,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                true
            }
        };
        // clang-format on

        for (auto data : testData)
        {
            Log::Comment(std::get<0>(data).c_str());
            std::tuple<Endpoint, Endpoint, bool> result;
            int amountMoved;
            result = UiaTextRange::_moveEndpointByUnitLine(_pUiaData,
                                                           std::get<2>(data),
                                                           std::get<4>(data),
                                                           std::get<1>(data),
                                                           &amountMoved);

            VERIFY_ARE_EQUAL(std::get<3>(data), amountMoved);
            VERIFY_ARE_EQUAL(std::get<5>(data), std::get<0>(result));
            VERIFY_ARE_EQUAL(std::get<6>(data), std::get<1>(result));
            VERIFY_ARE_EQUAL(std::get<7>(data), std::get<2>(result));
        }
    }

    TEST_METHOD(CanMoveEndpointByUnitDocument)
    {
        const Column firstColumnIndex = 0;
        const Column lastColumnIndex = _pScreenInfo->GetBufferSize().Width() - 1;
        const ScreenInfoRow topRow = 0;
        const ScreenInfoRow bottomRow = _pTextBuffer->TotalRowCount() - 1;

        // clang-format off
        const std::vector<std::tuple<std::wstring,
                                     UiaTextRange::MoveState,
                                     int, // amount to move
                                     int, // amount actually moved
                                     TextPatternRangeEndpoint, // endpoint to move
                                     Endpoint, // start
                                     Endpoint, // end
                                     bool // degenerate
                                     >> testData =
        {
            {
                L"can move _end forward to end of document without affecting _start",
                {
                    topRow, firstColumnIndex + 4,
                    topRow, firstColumnIndex + 4,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex + 4,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                false
            },

            {
                L"can move _start backward to end of document without affect _end",
                {
                    topRow, firstColumnIndex + 4,
                    topRow, firstColumnIndex + 4,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + 4,
                false
            },

            {
                L"can't move _end forward when it's already at the end of the document",
                {
                    topRow + 3, firstColumnIndex + 2,
                    bottomRow, lastColumnIndex,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                0,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 3) + firstColumnIndex + 2,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                false
            },

            {
                L"can't move _start backward when it's already at the start of the document",
                {
                    topRow, firstColumnIndex,
                    topRow + 5, firstColumnIndex + 6,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                0,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow + 5) + 6,
                false
            },

            {
                L"moving _end backward creates degenerate range at start of document",
                {
                    topRow + 5, firstColumnIndex + 2,
                    topRow + 5, firstColumnIndex + 6,
                    topRow,
                    lastColumnIndex,
                    firstColumnIndex,
                    UiaTextRange::MovementIncrement::Backward,
                    UiaTextRange::MovementDirection::Backward
                },
                -1,
                -1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_End,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, topRow) + firstColumnIndex,
                true
            },

            {
                L"moving _start forward creates degenerate range at end of document",
                {
                    topRow + 5, firstColumnIndex + 2,
                    topRow + 5, firstColumnIndex + 6,
                    bottomRow,
                    firstColumnIndex,
                    lastColumnIndex,
                    UiaTextRange::MovementIncrement::Forward,
                    UiaTextRange::MovementDirection::Forward
                },
                1,
                1,
                TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                UiaTextRange::_screenInfoRowToEndpoint(_pUiaData, bottomRow) + lastColumnIndex,
                true
            }
        };
        // clang-format on

        for (auto data : testData)
        {
            Log::Comment(std::get<0>(data).c_str());
            std::tuple<Endpoint, Endpoint, bool> result;
            int amountMoved;
            result = UiaTextRange::_moveEndpointByUnitDocument(_pUiaData,
                                                               std::get<2>(data),
                                                               std::get<4>(data),
                                                               std::get<1>(data),
                                                               &amountMoved);

            VERIFY_ARE_EQUAL(std::get<3>(data), amountMoved);
            VERIFY_ARE_EQUAL(std::get<5>(data), std::get<0>(result));
            VERIFY_ARE_EQUAL(std::get<6>(data), std::get<1>(result));
            VERIFY_ARE_EQUAL(std::get<7>(data), std::get<2>(result));
        }
    }
};
