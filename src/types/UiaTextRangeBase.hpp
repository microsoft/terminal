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
#include "unicode.hpp"

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

    public:
        // The default word delimiter for UiaTextRanges
        static constexpr std::wstring_view DefaultWordDelimiter{ &UNICODE_SPACE, 1 };

        // degenerate range
        HRESULT RuntimeClassInitialize(_In_ IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       _In_ std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept;

        // degenerate range at cursor position
        HRESULT RuntimeClassInitialize(_In_ IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       _In_ const Cursor& cursor,
                                       _In_ std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept;

        // specific endpoint range
        HRESULT RuntimeClassInitialize(_In_ IUiaData* pData,
                                       _In_ IRawElementProviderSimple* const pProvider,
                                       _In_ const COORD start,
                                       _In_ const COORD end,
                                       _In_ std::wstring_view wordDelimiters = DefaultWordDelimiter) noexcept;

        HRESULT RuntimeClassInitialize(const UiaTextRangeBase& a) noexcept;

        UiaTextRangeBase(UiaTextRangeBase&&) = default;
        UiaTextRangeBase& operator=(const UiaTextRangeBase&) = default;
        UiaTextRangeBase& operator=(UiaTextRangeBase&&) = default;
        ~UiaTextRangeBase() = default;

        const IdType GetId() const noexcept;
        const COORD GetEndpoint(TextPatternRangeEndpoint endpoint = TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start) const noexcept;
        bool SetEndpoint(TextPatternRangeEndpoint endpoint, const COORD val) noexcept;
        const bool IsDegenerate() const noexcept;

        void SetEndpoints(const COORD start, const COORD end) noexcept;

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
        void _outputObjectState();
#endif
        IUiaData* _pData;

        IRawElementProviderSimple* _pProvider;

        std::wstring _wordDelimiters;

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
        // NOTE: _start is inclusive, but _end is exclusive
        COORD _start;
        COORD _end;

        RECT _getTerminalRect() const;

        virtual const COORD _getScreenFontSize() const;
        const unsigned int _getViewportHeight(const SMALL_RECT viewport) noexcept;

        void _getBoundingRect(_In_ const COORD startAnchor, _In_ const COORD endAnchor, _Inout_ std::vector<double>& coords) const;

        void
        _moveEndpointByUnitCharacter(_In_ const int moveCount,
                                     _In_ const TextPatternRangeEndpoint endpoint,
                                     _Out_ gsl::not_null<int*> const pAmountMoved,
                                     _In_ const bool preventBufferEnd = false);

        void
        _moveEndpointByUnitWord(_In_ const int moveCount,
                                _In_ const TextPatternRangeEndpoint endpoint,
                                _In_ const std::wstring_view wordDelimiters,
                                _Out_ gsl::not_null<int*> const pAmountMoved,
                                _In_ const bool preventBufferEnd = false);

        void
        _moveEndpointByUnitLine(_In_ const int moveCount,
                                _In_ const TextPatternRangeEndpoint endpoint,
                                _Out_ gsl::not_null<int*> const pAmountMoved,
                                _In_ const bool preventBufferEnd = false);

        void
        _moveEndpointByUnitDocument(_In_ const int moveCount,
                                    _In_ const TextPatternRangeEndpoint endpoint,
                                    _Out_ gsl::not_null<int*> const pAmountMoved,
                                    _In_ const bool preventBufferEnd = false);

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
            COORD OriginalStart;
            COORD OriginalEnd;
        };

        struct ApiMsgGetText : IApiMsg
        {
            const wchar_t* Text;
        };

        struct ApiMsgMove : IApiMsg
        {
            COORD OriginalStart;
            COORD OriginalEnd;
            TextUnit Unit;
            int RequestedCount;
            int MovedCount;
        };

        struct ApiMsgMoveEndpointByUnit : IApiMsg
        {
            COORD OriginalStart;
            COORD OriginalEnd;
            TextPatternRangeEndpoint Endpoint;
            TextUnit Unit;
            int RequestedCount;
            int MovedCount;
        };

        struct ApiMsgMoveEndpointByRange : IApiMsg
        {
            COORD OriginalStart;
            COORD OriginalEnd;
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
