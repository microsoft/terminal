/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- UiaTextRangeBase.hpp

Abstract:
- This module provides UI Automation access to the text of the console
  window to support both automation tests and accessibility (screen
  reading) applications.
- ConHost and Windows Terminal must implement their own virtual functions separately.

Author(s):
- Austin Diviness (AustDi)     2017
- Carlos Zamora   (CaZamor)    2019
--*/

#pragma once

#include "precomp.h"

#include "inc/viewport.hpp"
#include "../buffer/out/textBuffer.hpp"
#include "IUiaData.h"

#include <deque>
#include <tuple>
#include <wrl/implements.h>

#ifdef UNIT_TESTING
class UiaTextRangeTests;
#endif

// The UiaTextRangeBase deals with several data structures that have
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

namespace Microsoft::Console::Types
{
    class UiaTextRangeBase : public WRL::RuntimeClass<WRL::RuntimeClassFlags<WRL::ClassicCom | WRL::InhibitFtmBase>, ITextRangeProvider>
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

            MoveState(IUiaData* pData,
                      const UiaTextRangeBase& range,
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
                      const MovementDirection direction) noexcept;

#ifdef UNIT_TESTING
            friend class ::UiaTextRangeTests;
#endif
        };

    public:
        // degenerate range
        HRESULT RuntimeClassInitialize(_In_ IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider) noexcept;

        // degenerate range at cursor position
        HRESULT RuntimeClassInitialize(_In_ IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const Cursor& cursor) noexcept;

        // specific endpoint range
        HRESULT RuntimeClassInitialize(_In_ IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       const Endpoint start,
                                       const Endpoint end,
                                       const bool degenerate) noexcept;

        HRESULT RuntimeClassInitialize(const UiaTextRangeBase& a) noexcept;

        UiaTextRangeBase(UiaTextRangeBase&&) = default;
        UiaTextRangeBase& operator=(const UiaTextRangeBase&) = default;
        UiaTextRangeBase& operator=(UiaTextRangeBase&&) = default;
        ~UiaTextRangeBase() = default;

        const IdType GetId() const noexcept;
        const Endpoint GetStart() const noexcept;
        const Endpoint GetEnd() const noexcept;
        const bool IsDegenerate() const noexcept;

        // TODO GitHub #605:
        // only used for UiaData::FindText. Remove after Search added properly
        void SetRangeValues(const Endpoint start, const Endpoint end, const bool isDegenerate) noexcept;

        // ITextRangeProvider methods
        virtual IFACEMETHODIMP Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) = 0;
        IFACEMETHODIMP Compare(_In_opt_ ITextRangeProvider* pRange, _Out_ BOOL* pRetVal) noexcept override;
        IFACEMETHODIMP CompareEndpoints(_In_ TextPatternRangeEndpoint endpoint,
                                        _In_ ITextRangeProvider* pTargetRange,
                                        _In_ TextPatternRangeEndpoint targetEndpoint,
                                        _Out_ int* pRetVal) noexcept override;
        IFACEMETHODIMP ExpandToEnclosingUnit(_In_ TextUnit unit) override;
        IFACEMETHODIMP FindAttribute(_In_ TEXTATTRIBUTEID textAttributeId,
                                     _In_ VARIANT val,
                                     _In_ BOOL searchBackward,
                                     _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) noexcept override;
        virtual IFACEMETHODIMP FindText(_In_ BSTR text,
                                        _In_ BOOL searchBackward,
                                        _In_ BOOL ignoreCase,
                                        _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) = 0;
        IFACEMETHODIMP GetAttributeValue(_In_ TEXTATTRIBUTEID textAttributeId,
                                         _Out_ VARIANT* pRetVal) noexcept override;
        IFACEMETHODIMP GetBoundingRectangles(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal) override;
        IFACEMETHODIMP GetEnclosingElement(_Outptr_result_maybenull_ IRawElementProviderSimple** ppRetVal) override;
        IFACEMETHODIMP GetText(_In_ int maxLength,
                               _Out_ BSTR* pRetVal) override;
        IFACEMETHODIMP Move(_In_ TextUnit unit,
                            _In_ int count,
                            _Out_ int* pRetVal) override;
        IFACEMETHODIMP MoveEndpointByUnit(_In_ TextPatternRangeEndpoint endpoint,
                                          _In_ TextUnit unit,
                                          _In_ int count,
                                          _Out_ int* pRetVal) override;
        IFACEMETHODIMP MoveEndpointByRange(_In_ TextPatternRangeEndpoint endpoint,
                                           _In_ ITextRangeProvider* pTargetRange,
                                           _In_ TextPatternRangeEndpoint targetEndpoint) override;
        IFACEMETHODIMP Select() override;
        IFACEMETHODIMP AddToSelection() noexcept override;
        IFACEMETHODIMP RemoveFromSelection() noexcept override;
        IFACEMETHODIMP ScrollIntoView(_In_ BOOL alignToTop) override;
        IFACEMETHODIMP GetChildren(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal) noexcept override;

    protected:
        UiaTextRangeBase() = default;
#if _DEBUG
        void _outputRowConversions(IUiaData* pData);
        void _outputObjectState();
#endif
        IUiaData* _pData;

        IRawElementProviderSimple* _pProvider;

        virtual void _ChangeViewport(const SMALL_RECT NewWindow) = 0;
        virtual void _TranslatePointToScreen(LPPOINT clientPoint) const = 0;
        virtual void _TranslatePointFromScreen(LPPOINT screenPoint) const = 0;

        void Initialize(_In_ const UiaPoint point);

        // used to debug objects passed back and forth
        // between the provider and the client
        IdType _id;

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

        RECT _getTerminalRect() const;

        static const COORD _getScreenBufferCoords(gsl::not_null<IUiaData*> pData);
        virtual const COORD _getScreenFontSize() const;

        static const unsigned int _getTotalRows(gsl::not_null<IUiaData*> pData) noexcept;
        static const unsigned int _getRowWidth(gsl::not_null<IUiaData*> pData);

        static const unsigned int _getFirstScreenInfoRowIndex() noexcept;
        static const unsigned int _getLastScreenInfoRowIndex(gsl::not_null<IUiaData*> pData) noexcept;

        static const Column _getFirstColumnIndex() noexcept;
        static const Column _getLastColumnIndex(gsl::not_null<IUiaData*> pData);

        const unsigned int _rowCountInRange(gsl::not_null<IUiaData*> pData) const;

        static const TextBufferRow _endpointToTextBufferRow(gsl::not_null<IUiaData*> pData,
                                                            const Endpoint endpoint);
        static const ScreenInfoRow _textBufferRowToScreenInfoRow(gsl::not_null<IUiaData*> pData,
                                                                 const TextBufferRow row) noexcept;

        static const TextBufferRow _screenInfoRowToTextBufferRow(gsl::not_null<IUiaData*> pData,
                                                                 const ScreenInfoRow row) noexcept;
        static const Endpoint _textBufferRowToEndpoint(gsl::not_null<IUiaData*> pData, const TextBufferRow row);

        static const ScreenInfoRow _endpointToScreenInfoRow(gsl::not_null<IUiaData*> pData,
                                                            const Endpoint endpoint);
        static const Endpoint _screenInfoRowToEndpoint(gsl::not_null<IUiaData*> pData,
                                                       const ScreenInfoRow row);

        static COORD _endpointToCoord(gsl::not_null<IUiaData*> pData,
                                      const Endpoint endpoint);
        static Endpoint _coordToEndpoint(gsl::not_null<IUiaData*> pData,
                                         const COORD coord);

        static const Column _endpointToColumn(gsl::not_null<IUiaData*> pData,
                                              const Endpoint endpoint);

        static const Row _normalizeRow(gsl::not_null<IUiaData*> pData, const Row row) noexcept;

        static const ViewportRow _screenInfoRowToViewportRow(gsl::not_null<IUiaData*> pData,
                                                             const ScreenInfoRow row) noexcept;
        // Routine Description:
        // - Converts a ScreenInfoRow to a ViewportRow.
        // Arguments:
        // - row - the ScreenInfoRow to convert
        // - viewport - the viewport to use for the conversion
        // Return Value:
        // - the equivalent ViewportRow.
        static constexpr const ViewportRow _screenInfoRowToViewportRow(const ScreenInfoRow row,
                                                                       const SMALL_RECT viewport) noexcept
        {
            return row - viewport.Top;
        }

        static const bool _isScreenInfoRowInViewport(gsl::not_null<IUiaData*> pData,
                                                     const ScreenInfoRow row) noexcept;
        static const bool _isScreenInfoRowInViewport(const ScreenInfoRow row,
                                                     const SMALL_RECT viewport) noexcept;

        static const unsigned int _getViewportHeight(const SMALL_RECT viewport) noexcept;
        static const unsigned int _getViewportWidth(const SMALL_RECT viewport) noexcept;

        void _addScreenInfoRowBoundaries(gsl::not_null<IUiaData*> pData,
                                         const ScreenInfoRow screenInfoRow,
                                         _Inout_ std::vector<double>& coords) const;

        static const int _compareScreenCoords(gsl::not_null<IUiaData*> pData,
                                              const ScreenInfoRow rowA,
                                              const Column colA,
                                              const ScreenInfoRow rowB,
                                              const Column colB);

        static std::pair<Endpoint, Endpoint> _moveByCharacter(gsl::not_null<IUiaData*> pData,
                                                              const int moveCount,
                                                              const MoveState moveState,
                                                              _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::pair<Endpoint, Endpoint> _moveByCharacterForward(gsl::not_null<IUiaData*> pData,
                                                                     const int moveCount,
                                                                     const MoveState moveState,
                                                                     _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::pair<Endpoint, Endpoint> _moveByCharacterBackward(gsl::not_null<IUiaData*> pData,
                                                                      const int moveCount,
                                                                      const MoveState moveState,
                                                                      _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::pair<Endpoint, Endpoint> _moveByLine(gsl::not_null<IUiaData*> pData,
                                                         const int moveCount,
                                                         const MoveState moveState,
                                                         _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::pair<Endpoint, Endpoint> _moveByDocument(gsl::not_null<IUiaData*> pData,
                                                             const int moveCount,
                                                             const MoveState moveState,
                                                             _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitCharacter(gsl::not_null<IUiaData*> pData,
                                     const int moveCount,
                                     const TextPatternRangeEndpoint endpoint,
                                     const MoveState moveState,
                                     _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitCharacterForward(gsl::not_null<IUiaData*> pData,
                                            const int moveCount,
                                            const TextPatternRangeEndpoint endpoint,
                                            const MoveState moveState,
                                            _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitCharacterBackward(gsl::not_null<IUiaData*> pData,
                                             const int moveCount,
                                             const TextPatternRangeEndpoint endpoint,
                                             const MoveState moveState,
                                             _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitLine(gsl::not_null<IUiaData*> pData,
                                const int moveCount,
                                const TextPatternRangeEndpoint endpoint,
                                const MoveState moveState,
                                _Out_ gsl::not_null<int*> const pAmountMoved);

        static std::tuple<Endpoint, Endpoint, bool>
        _moveEndpointByUnitDocument(gsl::not_null<IUiaData*> pData,
                                    const int moveCount,
                                    const TextPatternRangeEndpoint endpoint,
                                    const MoveState moveState,
                                    _Out_ gsl::not_null<int*> const pAmountMoved);

#ifdef UNIT_TESTING
        friend class ::UiaTextRangeTests;
#endif
    };

    namespace UiaTextRangeBaseTracing
    {
        enum class ApiCall
        {
            Constructor,
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
