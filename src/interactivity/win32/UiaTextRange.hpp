/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- UiaTextRange.hpp

Abstract:
- This module provides UI Automation access to the text of the console
  window to support both automation tests and accessibility (screen
  reading) applications.

Author(s):
- Austin Diviness (AustDi)     2017
--*/

#pragma once

#include "precomp.h"

#include "../inc/IConsoleWindow.hpp"
#include "../types/inc/viewport.hpp"
#include "../../buffer/out/cursor.h"

#include <deque>
#include <tuple>

#ifdef UNIT_TESTING
class UiaTextRangeTests;
#endif

// The UiaTextRange deals with several data structures that have
// similar semantics. In order to keep the information from these data
// structures separated, each structure has its own naming for a
// row.
//
// There is the generic Row, which does not know which data structure
// the row came from.
//
// There is the ViewportRow, which is a 0-indexed row value from the
// viewport. The top row of the viewport is at 0, rows below the top
// row increase in value and rows above the top row get increasingly
// negative.
//
// ScreenInfoRow is a row from the screen info data structure. They
// start at 0 at the top of screen info buffer. Their positions do not
// change but their associated row in the text buffer does change each
// time a new line is written.
//
// TextBufferRow is a row from the text buffer. It is not a ROW
// struct, but rather the index of a row. This is also 0-indexed. A
// TextBufferRow with a value of 0 does not necessarily refer to the
// top row of the console.

typedef int Row;
typedef int ViewportRow;
typedef unsigned int ScreenInfoRow;
typedef unsigned int TextBufferRow;

typedef unsigned long long IdType;

// A Column is a row agnostic value that refers to the column an
// endpoint is equivalent to. It is 0-indexed.
typedef unsigned int Column;

// an endpoint is a char location in the text buffer. endpoint 0 is
// the first char of the 0th row in the text buffer row array.
typedef unsigned int Endpoint;

constexpr IdType InvalidId = 0;

namespace Microsoft::Console::Interactivity::Win32
{
    class UiaTextRange final : public ITextRangeProvider
    {
    private:
        static IdType id;

    protected:
        // indicates which direction a movement operation
        // is going
        enum class MovementDirection
        {
            Forward,
            Backward
        };

        // valid increment amounts for forward and
        // backward movement
        enum class MovementIncrement
        {
            Forward = 1,
            Backward = -1
        };

        // common information used by the variety of
        // movement operations
        struct MoveState
        {
            // screen/column position of _start
            ScreenInfoRow StartScreenInfoRow;
            Column StartColumn;
            // screen/column position of _end
            ScreenInfoRow EndScreenInfoRow;
            Column EndColumn;
            // last row in the direction being moved
            ScreenInfoRow LimitingRow;
            // first column in the direction being moved
            Column FirstColumnInRow;
            // last column in the direction being moved
            Column LastColumnInRow;
            // increment amount
            MovementIncrement Increment;
            // direction moving
            MovementDirection Direction;

            MoveState(const UiaTextRange& range,
                      const MovementDirection direction);

        private:
            MoveState(const ScreenInfoRow startScreenInfoRow,
                      const Column startColumn,
                      const ScreenInfoRow endScreenInfoRow,
                      const Column endColumn,
                      const ScreenInfoRow limitingRow,
                      const Column firstColumnInRow,
                      const Column lastColumnInRow,
                      const MovementIncrement increment,
                      const MovementDirection direction);

#ifdef UNIT_TESTING
            friend class ::UiaTextRangeTests;
#endif
        };

    public:
        static std::deque<UiaTextRange*> GetSelectionRanges(_In_ IRawElementProviderSimple* pProvider);

        // degenerate range
        static UiaTextRange* Create(_In_ IRawElementProviderSimple* const pProvider);

        // degenerate range at cursor position
        static UiaTextRange* Create(_In_ IRawElementProviderSimple* const pProvider,
                                    const Cursor& cursor);

        // specific endpoint range
        static UiaTextRange* Create(_In_ IRawElementProviderSimple* const pProvider,
                                    const Endpoint start,
                                    const Endpoint end,
                                    const bool degenerate);

        // range from a UiaPoint
        static UiaTextRange* Create(_In_ IRawElementProviderSimple* const pProvider,
                                    const UiaPoint point);

        ~UiaTextRange();

        const IdType GetId() const;
        const Endpoint GetStart() const;
        const Endpoint GetEnd() const;
        const bool IsDegenerate() const;

        // IUnknown methods
        IFACEMETHODIMP_(ULONG)
        AddRef();
        IFACEMETHODIMP_(ULONG)
        Release();
        IFACEMETHODIMP QueryInterface(_In_ REFIID riid,
                                      _COM_Outptr_result_maybenull_ void** ppInterface);

        // ITextRangeProvider methods
        IFACEMETHODIMP Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal);
        IFACEMETHODIMP Compare(_In_opt_ ITextRangeProvider* pRange, _Out_ BOOL* pRetVal);
        IFACEMETHODIMP CompareEndpoints(_In_ TextPatternRangeEndpoint endpoint,
                                        _In_ ITextRangeProvider* pTargetRange,
                                        _In_ TextPatternRangeEndpoint targetEndpoint,
                                        _Out_ int* pRetVal);
        IFACEMETHODIMP ExpandToEnclosingUnit(_In_ TextUnit unit);
        IFACEMETHODIMP FindAttribute(_In_ TEXTATTRIBUTEID textAttributeId,
                                     _In_ VARIANT val,
                                     _In_ BOOL searchBackward,
                                     _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal);
        IFACEMETHODIMP FindText(_In_ BSTR text,
                                _In_ BOOL searchBackward,
                                _In_ BOOL ignoreCase,
                                _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal);
        IFACEMETHODIMP GetAttributeValue(_In_ TEXTATTRIBUTEID textAttributeId,
                                         _Out_ VARIANT* pRetVal);
        IFACEMETHODIMP GetBoundingRectangles(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal);
        IFACEMETHODIMP GetEnclosingElement(_Outptr_result_maybenull_ IRawElementProviderSimple** ppRetVal);
        IFACEMETHODIMP GetText(_In_ int maxLength,
                               _Out_ BSTR* pRetVal);
        IFACEMETHODIMP Move(_In_ TextUnit unit,
                            _In_ int count,
                            _Out_ int* pRetVal);
        IFACEMETHODIMP MoveEndpointByUnit(_In_ TextPatternRangeEndpoint endpoint,
                                          _In_ TextUnit unit,
                                          _In_ int count,
                                          _Out_ int* pRetVal);
        IFACEMETHODIMP MoveEndpointByRange(_In_ TextPatternRangeEndpoint endpoint,
                                           _In_ ITextRangeProvider* pTargetRange,
                                           _In_ TextPatternRangeEndpoint targetEndpoint);
        IFACEMETHODIMP Select();
        IFACEMETHODIMP AddToSelection();
        IFACEMETHODIMP RemoveFromSelection();
        IFACEMETHODIMP ScrollIntoView(_In_ BOOL alignToTop);
        IFACEMETHODIMP GetChildren(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal);

    protected:
#if _DEBUG
        void _outputRowConversions();
        void _outputObjectState();
#endif

        IRawElementProviderSimple* const _pProvider;

    private:
        // degenerate range
        UiaTextRange(_In_ IRawElementProviderSimple* const pProvider);

        // degenerate range at cursor position
        UiaTextRange(_In_ IRawElementProviderSimple* const pProvider,
                     const Cursor& cursor);

        // specific endpoint range
        UiaTextRange(_In_ IRawElementProviderSimple* const pProvider,
                     const Endpoint start,
                     const Endpoint end,
                     const bool degenerate);

        // range from a UiaPoint
        UiaTextRange(_In_ IRawElementProviderSimple* const pProvider,
                     const UiaPoint point);

        UiaTextRange(const UiaTextRange& a);

        // used to debug objects passed back and forth
        // between the provider and the client
        IdType _id;

        // Ref counter for COM object
        ULONG _cRefs;

        // measure units in the form [_start, _end]. _start
        // may be a bigger number than _end if the range
        // wraps around the end of the text buffer.
        //
        // In this scenario, _start <= _end
        // 0 ............... N (text buffer line indices)
        //      s-----e        (_start to _end)
        //
        // In this scenario, _start >= end
        // 0 ............... N (text buffer line indices)
        //   ---e     s-----   (_start to _end)
        //
        Endpoint _start;
        Endpoint _end;

        // The msdn documentation (and hence this class) talks a bunch about a
        // degenerate range. A range is degenerate if it contains
        // no text (both the start and end endpoints are the same). Note that
        // a degenerate range may have a position in the text. We indicate a
        // degenerate range internally with a bool. If a range is degenerate
        // then both endpoints will contain the same value.
        bool _degenerate;

        static const Microsoft::Console::Types::Viewport& _getViewport();
        static HWND _getWindowHandle();
        static IConsoleWindow* const _getIConsoleWindow();
        static SCREEN_INFORMATION& _getScreenInfo();
        static TextBuffer& _getTextBuffer();
        static const COORD _getScreenBufferCoords();

        static const unsigned int _getTotalRows();
        static const unsigned int _getRowWidth();

        static const unsigned int _getFirstScreenInfoRowIndex();
        static const unsigned int _getLastScreenInfoRowIndex();

        static const Column _getFirstColumnIndex();
        static const Column _getLastColumnIndex();

        const unsigned int _rowCountInRange() const;

        static const TextBufferRow _endpointToTextBufferRow(const Endpoint endpoint);
        static const ScreenInfoRow _textBufferRowToScreenInfoRow(const TextBufferRow row);

        static const TextBufferRow _screenInfoRowToTextBufferRow(const ScreenInfoRow row);
        static const Endpoint _textBufferRowToEndpoint(const TextBufferRow row);

        static const ScreenInfoRow _endpointToScreenInfoRow(const Endpoint endpoint);
        static const Endpoint _screenInfoRowToEndpoint(const ScreenInfoRow row);

        static COORD _endpointToCoord(const Endpoint endpoint);
        static Endpoint _coordToEndpoint(const COORD coord);

        static const Column _endpointToColumn(const Endpoint endpoint);

        static const Row _normalizeRow(const Row row);

        static const ViewportRow _screenInfoRowToViewportRow(const ScreenInfoRow row);
        static const ViewportRow _screenInfoRowToViewportRow(const ScreenInfoRow row,
                                                             const SMALL_RECT viewport);

        static const bool _isScreenInfoRowInViewport(const ScreenInfoRow row);
        static const bool _isScreenInfoRowInViewport(const ScreenInfoRow row,
                                                     const SMALL_RECT viewport);

        static const unsigned int _getViewportHeight(const SMALL_RECT viewport);
        static const unsigned int _getViewportWidth(const SMALL_RECT viewport);

        void _addScreenInfoRowBoundaries(const ScreenInfoRow screenInfoRow,
                                         _Inout_ std::vector<double>& coords) const;

        static const int _compareScreenCoords(const ScreenInfoRow rowA,
                                              const Column colA,
                                              const ScreenInfoRow rowB,
                                              const Column colB);

        static std::pair<Endpoint, Endpoint> _moveByCharacter(const int moveCount,
                                                              const MoveState moveState,
                                                              _Out_ int* const pAmountMoved);

        static std::pair<Endpoint, Endpoint> _moveByCharacterForward(const int moveCount,
                                                                     const MoveState moveState,
                                                                     _Out_ int* const pAmountMoved);

        static std::pair<Endpoint, Endpoint> _moveByCharacterBackward(const int moveCount,
                                                                      const MoveState moveState,
                                                                      _Out_ int* const pAmountMoved);

        static std::pair<Endpoint, Endpoint> _moveByLine(const int moveCount,
                                                         const MoveState moveState,
                                                         _Out_ int* const pAmountMoved);

        static std::pair<Endpoint, Endpoint> _moveByDocument(const int moveCount,
                                                             const MoveState moveState,
                                                             _Out_ int* const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitCharacter(const int moveCount,
                                     const TextPatternRangeEndpoint endpoint,
                                     const MoveState moveState,
                                     _Out_ int* const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitCharacterForward(const int moveCount,
                                            const TextPatternRangeEndpoint endpoint,
                                            const MoveState moveState,
                                            _Out_ int* const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitCharacterBackward(const int moveCount,
                                             const TextPatternRangeEndpoint endpoint,
                                             const MoveState moveState,
                                             _Out_ int* const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitLine(const int moveCount,
                                const TextPatternRangeEndpoint endpoint,
                                const MoveState moveState,
                                _Out_ int* const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitDocument(const int moveCount,
                                    const TextPatternRangeEndpoint endpoint,
                                    const MoveState moveState,
                                    _Out_ int* const pAmountMoved);

#ifdef UNIT_TESTING
        friend class ::UiaTextRangeTests;
#endif
    };

    namespace UiaTextRangeTracing
    {
        enum class ApiCall
        {
            Constructor,
            AddRef,
            Release,
            QueryInterface,
            Clone,
            Compare,
            CompareEndpoints,
            ExpandToEnclosingUnit,
            FindAttribute,
            FindText,
            GetAttributeValue,
            GetBoundingRectangles,
            GetEnclosingElement,
            GetText,
            Move,
            MoveEndpointByUnit,
            MoveEndpointByRange,
            Select,
            AddToSelection,
            RemoveFromSelection,
            ScrollIntoView,
            GetChildren
        };

        struct IApiMsg
        {
        };

        struct ApiMsgConstructor : public IApiMsg
        {
            IdType Id;
        };

        struct ApiMsgClone : public IApiMsg
        {
            IdType CloneId;
        };

        struct ApiMsgCompare : public IApiMsg
        {
            IdType OtherId;
            bool Equal;
        };

        struct ApiMsgCompareEndpoints : public IApiMsg
        {
            IdType OtherId;
            TextPatternRangeEndpoint Endpoint;
            TextPatternRangeEndpoint TargetEndpoint;
            int Result;
        };

        struct ApiMsgExpandToEnclosingUnit : public IApiMsg
        {
            TextUnit Unit;
            Endpoint OriginalStart;
            Endpoint OriginalEnd;
        };

        struct ApiMsgGetText : IApiMsg
        {
            const wchar_t* Text;
        };

        struct ApiMsgMove : IApiMsg
        {
            Endpoint OriginalStart;
            Endpoint OriginalEnd;
            TextUnit Unit;
            int RequestedCount;
            int MovedCount;
        };

        struct ApiMsgMoveEndpointByUnit : IApiMsg
        {
            Endpoint OriginalStart;
            Endpoint OriginalEnd;
            TextPatternRangeEndpoint Endpoint;
            TextUnit Unit;
            int RequestedCount;
            int MovedCount;
        };

        struct ApiMsgMoveEndpointByRange : IApiMsg
        {
            Endpoint OriginalStart;
            Endpoint OriginalEnd;
            TextPatternRangeEndpoint Endpoint;
            TextPatternRangeEndpoint TargetEndpoint;
            IdType OtherId;
        };

        struct ApiMsgScrollIntoView : IApiMsg
        {
            bool AlignToTop;
        };
    }
}
