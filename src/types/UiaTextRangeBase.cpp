// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "UiaTextRangeBase.hpp"
#include "ScreenInfoUiaProviderBase.h"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Types::UiaTextRangeBaseTracing;

// toggle these for additional logging in a debug build
//#define UIATEXTRANGE_DEBUG_MSGS 1
#undef UIATEXTRANGE_DEBUG_MSGS

IdType UiaTextRangeBase::id = 1;

UiaTextRangeBase::MoveState::MoveState(IUiaData* pData,
                                       const UiaTextRangeBase& range,
                                       const MovementDirection direction) :
    StartScreenInfoRow{ UiaTextRangeBase::_endpointToScreenInfoRow(pData, range.GetStart()) },
    StartColumn{ UiaTextRangeBase::_endpointToColumn(pData, range.GetStart()) },
    EndScreenInfoRow{ UiaTextRangeBase::_endpointToScreenInfoRow(pData, range.GetEnd()) },
    EndColumn{ UiaTextRangeBase::_endpointToColumn(pData, range.GetEnd()) },
    Direction{ direction }
{
    if (direction == MovementDirection::Forward)
    {
        LimitingRow = UiaTextRangeBase::_getLastScreenInfoRowIndex(pData);
        FirstColumnInRow = UiaTextRangeBase::_getFirstColumnIndex();
        LastColumnInRow = UiaTextRangeBase::_getLastColumnIndex(pData);
        Increment = MovementIncrement::Forward;
    }
    else
    {
        LimitingRow = UiaTextRangeBase::_getFirstScreenInfoRowIndex();
        FirstColumnInRow = UiaTextRangeBase::_getLastColumnIndex(pData);
        LastColumnInRow = UiaTextRangeBase::_getFirstColumnIndex();
        Increment = MovementIncrement::Backward;
    }
}

UiaTextRangeBase::MoveState::MoveState(const ScreenInfoRow startScreenInfoRow,
                                       const Column startColumn,
                                       const ScreenInfoRow endScreenInfoRow,
                                       const Column endColumn,
                                       const ScreenInfoRow limitingRow,
                                       const Column firstColumnInRow,
                                       const Column lastColumnInRow,
                                       const MovementIncrement increment,
                                       const MovementDirection direction) noexcept :
    StartScreenInfoRow{ startScreenInfoRow },
    StartColumn{ startColumn },
    EndScreenInfoRow{ endScreenInfoRow },
    EndColumn{ endColumn },
    LimitingRow{ limitingRow },
    FirstColumnInRow{ firstColumnInRow },
    LastColumnInRow{ lastColumnInRow },
    Increment{ increment },
    Direction{ direction }
{
}

#if _DEBUG
#include <sstream>
// This is a debugging function that prints out the current
// relationship between screen info rows, text buffer rows, and
// endpoints.
void UiaTextRangeBase::_outputRowConversions(IUiaData* pData)
{
    try
    {
        unsigned int totalRows = _getTotalRows(pData);
        OutputDebugString(L"screenBuffer\ttextBuffer\tendpoint\n");
        for (unsigned int i = 0; i < totalRows; ++i)
        {
            std::wstringstream ss;
            ss << i << "\t" << _screenInfoRowToTextBufferRow(pData, i) << "\t" << _screenInfoRowToEndpoint(pData, i) << "\n";
            std::wstring str = ss.str();
            OutputDebugString(str.c_str());
        }
        OutputDebugString(L"\n");
    }
    catch (...)
    {
        LOG_HR(wil::ResultFromCaughtException());
    }
}

void UiaTextRangeBase::_outputObjectState()
{
    std::wstringstream ss;
    ss << "Object State";
    ss << " _id: " << _id;
    ss << " _start: " << _start;
    ss << " _end: " << _end;
    ss << " _degenerate: " << _degenerate;

    std::wstring str = ss.str();
    OutputDebugString(str.c_str());
    OutputDebugString(L"\n");
}
#endif // _DEBUG

// degenerate range constructor.
#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(_In_ IUiaData* pData, _In_ IRawElementProviderSimple* const pProvider) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pProvider);
    RETURN_HR_IF_NULL(E_INVALIDARG, pData);

    _pProvider = pProvider;
    _start = 0;
    _end = 0;
    _degenerate = true;
    _pData = pData;

    _id = id;
    ++id;

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgConstructor apiMsg;
    apiMsg.Id = _id;
    Tracing::s_TraceUia(nullptr, ApiCall::Constructor, &apiMsg);*/

    return S_OK;
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(_In_ IUiaData* pData,
                                                 _In_ IRawElementProviderSimple* const pProvider,
                                                 const Cursor& cursor) noexcept
{
    RETURN_IF_FAILED(RuntimeClassInitialize(pData, pProvider));

    try
    {
        _degenerate = true;
        _start = _screenInfoRowToEndpoint(_pData, cursor.GetPosition().Y) + cursor.GetPosition().X;
        _end = _start;

#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
        OutputDebugString(L"Constructor\n");
        _outputObjectState();
#endif
    }
    catch (...)
    {
        return E_INVALIDARG;
    }
    return S_OK;
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(_In_ IUiaData* pData,
                                                 _In_ IRawElementProviderSimple* const pProvider,
                                                 const Endpoint start,
                                                 const Endpoint end,
                                                 const bool degenerate) noexcept
{
    RETURN_IF_FAILED(RuntimeClassInitialize(pData, pProvider));
    RETURN_HR_IF(E_INVALIDARG, !degenerate && start > end);

    _degenerate = degenerate;
    _start = start;
    _end = degenerate ? start : end;

#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
    OutputDebugString(L"Constructor\n");
    _outputObjectState();
#endif

    return S_OK;
}

void UiaTextRangeBase::Initialize(_In_ const UiaPoint point)
{
    POINT clientPoint;
    clientPoint.x = static_cast<LONG>(point.x);
    clientPoint.y = static_cast<LONG>(point.y);
    // get row that point resides in
    const RECT windowRect = _getTerminalRect();
    const SMALL_RECT viewport = _pData->GetViewport().ToInclusive();
    ScreenInfoRow row = 0;
    if (clientPoint.y <= windowRect.top)
    {
        row = viewport.Top;
    }
    else if (clientPoint.y >= windowRect.bottom)
    {
        row = viewport.Bottom;
    }
    else
    {
        // change point coords to pixels relative to window
        _TranslatePointFromScreen(&clientPoint);

        const COORD currentFontSize = _getScreenFontSize();
        row = (clientPoint.y / currentFontSize.Y) + viewport.Top;
    }
    _start = _screenInfoRowToEndpoint(_pData, row);
    _end = _start;
    _degenerate = true;
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(const UiaTextRangeBase& a) noexcept
{
    _pProvider = a._pProvider;
    _start = a._start;
    _end = a._end;
    _degenerate = a._degenerate;
    _pData = a._pData;

    _id = id;
    ++id;

#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
    OutputDebugString(L"Copy Constructor\n");
    _outputObjectState();
#endif

    return S_OK;
}

const IdType UiaTextRangeBase::GetId() const noexcept
{
    return _id;
}

const Endpoint UiaTextRangeBase::GetStart() const noexcept
{
    return _start;
}

const Endpoint UiaTextRangeBase::GetEnd() const noexcept
{
    return _end;
}

// Routine Description:
// - returns true if the range is currently degenerate (empty range).
// Arguments:
// - <none>
// Return Value:
// - true if range is degenerate, false otherwise.
const bool UiaTextRangeBase::IsDegenerate() const noexcept
{
    return _degenerate;
}

void UiaTextRangeBase::SetRangeValues(const Endpoint start, const Endpoint end, const bool isDegenerate) noexcept
{
    _start = start;
    _end = end;
    _degenerate = isDegenerate;
}

#pragma region ITextRangeProvider

IFACEMETHODIMP UiaTextRangeBase::Compare(_In_opt_ ITextRangeProvider* pRange, _Out_ BOOL* pRetVal) noexcept
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    *pRetVal = FALSE;
    const UiaTextRangeBase* other = static_cast<UiaTextRangeBase*>(pRange);
    if (other)
    {
        *pRetVal = !!(_start == other->GetStart() &&
                      _end == other->GetEnd() &&
                      _degenerate == other->IsDegenerate());
    }
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgCompare apiMsg;
    apiMsg.OtherId = other == nullptr ? InvalidId : other->GetId();
    apiMsg.Equal = !!*pRetVal;
    Tracing::s_TraceUia(this, ApiCall::Compare, &apiMsg);*/

    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::CompareEndpoints(_In_ TextPatternRangeEndpoint endpoint,
                                                  _In_ ITextRangeProvider* pTargetRange,
                                                  _In_ TextPatternRangeEndpoint targetEndpoint,
                                                  _Out_ int* pRetVal) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    *pRetVal = 0;

    // get the text range that we're comparing to
    const UiaTextRangeBase* range = static_cast<UiaTextRangeBase*>(pTargetRange);
    if (range == nullptr)
    {
        return E_INVALIDARG;
    }

    // get endpoint value that we're comparing to
    const Endpoint theirValue = targetEndpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start ? range->GetStart() : range->GetEnd() + 1;

    // get the values of our endpoint
    const Endpoint ourValue = endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start ? _start : _end + 1;

    // compare them
    *pRetVal = std::clamp(static_cast<int>(ourValue) - static_cast<int>(theirValue), -1, 1);

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgCompareEndpoints apiMsg;
    apiMsg.OtherId = range->GetId();
    apiMsg.Endpoint = endpoint;
    apiMsg.TargetEndpoint = targetEndpoint;
    apiMsg.Result = *pRetVal;
    Tracing::s_TraceUia(this, ApiCall::CompareEndpoints, &apiMsg);*/

    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::ExpandToEnclosingUnit(_In_ TextUnit unit)
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    ApiMsgExpandToEnclosingUnit apiMsg;
    apiMsg.Unit = unit;
    apiMsg.OriginalStart = _start;
    apiMsg.OriginalEnd = _end;

    try
    {
        const ScreenInfoRow topRow = _getFirstScreenInfoRowIndex();
        const ScreenInfoRow bottomRow = _getLastScreenInfoRowIndex(_pData);

        if (unit == TextUnit::TextUnit_Character)
        {
            _end = _start;
        }
        else if (unit <= TextUnit::TextUnit_Line)
        {
            // expand to line
            _start = _textBufferRowToEndpoint(_pData, _endpointToTextBufferRow(_pData, _start));
            _end = _start + _getLastColumnIndex(_pData);
            FAIL_FAST_IF(!(_start <= _end));
        }
        else
        {
            // expand to document
            _start = _screenInfoRowToEndpoint(_pData, topRow);
            _end = _screenInfoRowToEndpoint(_pData, bottomRow) + _getLastColumnIndex(_pData);
        }

        _degenerate = false;

        // TODO GitHub #1914: Re-attach Tracing to UIA Tree
        //Tracing::s_TraceUia(this, ApiCall::ExpandToEnclosingUnit, &apiMsg);

        return S_OK;
    }
    CATCH_RETURN();
}

// we don't support this currently
IFACEMETHODIMP UiaTextRangeBase::FindAttribute(_In_ TEXTATTRIBUTEID /*textAttributeId*/,
                                               _In_ VARIANT /*val*/,
                                               _In_ BOOL /*searchBackward*/,
                                               _Outptr_result_maybenull_ ITextRangeProvider** /*ppRetVal*/) noexcept
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::FindAttribute, nullptr);
    return E_NOTIMPL;
}

IFACEMETHODIMP UiaTextRangeBase::GetAttributeValue(_In_ TEXTATTRIBUTEID textAttributeId,
                                                   _Out_ VARIANT* pRetVal) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetAttributeValue, nullptr);
    if (textAttributeId == UIA_IsReadOnlyAttributeId)
    {
        pRetVal->vt = VT_BOOL;
        pRetVal->boolVal = VARIANT_FALSE;
    }
    else
    {
        pRetVal->vt = VT_UNKNOWN;
        UiaGetReservedNotSupportedValue(&pRetVal->punkVal);
    }
    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::GetBoundingRectangles(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal)
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;

    try
    {
        // vector to put coords into. they go in as four doubles in the
        // order: left, top, width, height. each line will have its own
        // set of coords.
        std::vector<double> coords;
        const TextBufferRow startRow = _endpointToTextBufferRow(_pData, _start);

        if (_degenerate && _isScreenInfoRowInViewport(_pData, startRow))
        {
            _addScreenInfoRowBoundaries(_pData, _textBufferRowToScreenInfoRow(_pData, startRow), coords);
        }
        else
        {
            const unsigned int totalRowsInRange = _rowCountInRange(_pData);
            for (unsigned int i = 0; i < totalRowsInRange; ++i)
            {
                const ScreenInfoRow screenInfoRow = _textBufferRowToScreenInfoRow(_pData, startRow + i);
                if (!_isScreenInfoRowInViewport(_pData, screenInfoRow))
                {
                    continue;
                }
                _addScreenInfoRowBoundaries(_pData, screenInfoRow, coords);
            }
        }

        // convert to a safearray
        *ppRetVal = SafeArrayCreateVector(VT_R8, 0, gsl::narrow<ULONG>(coords.size()));
        if (*ppRetVal == nullptr)
        {
            return E_OUTOFMEMORY;
        }
        HRESULT hr = E_UNEXPECTED;
        for (LONG i = 0; i < gsl::narrow<LONG>(coords.size()); ++i)
        {
            hr = SafeArrayPutElement(*ppRetVal, &i, &coords.at(i));
            if (FAILED(hr))
            {
                SafeArrayDestroy(*ppRetVal);
                *ppRetVal = nullptr;
                return hr;
            }
        }
    }
    CATCH_RETURN();

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetBoundingRectangles, nullptr);

    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::GetEnclosingElement(_Outptr_result_maybenull_ IRawElementProviderSimple** ppRetVal)
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;

    //Tracing::s_TraceUia(this, ApiCall::GetBoundingRectangles, nullptr);
    return _pProvider->QueryInterface(IID_PPV_ARGS(ppRetVal));
}

IFACEMETHODIMP UiaTextRangeBase::GetText(_In_ int maxLength, _Out_ BSTR* pRetVal)
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    *pRetVal = nullptr;

    std::wstring wstr = L"";

    if (maxLength < -1)
    {
        return E_INVALIDARG;
    }
    // the caller must pass in a value for the max length of the text
    // to retrieve. a value of -1 means they don't want the text
    // truncated.
    const bool getPartialText = maxLength != -1;

    if (!_degenerate)
    {
        try
        {
            const ScreenInfoRow startScreenInfoRow = _endpointToScreenInfoRow(_pData, _start);
            const Column startColumn = _endpointToColumn(_pData, _start);
            const ScreenInfoRow endScreenInfoRow = _endpointToScreenInfoRow(_pData, _end);
            const Column endColumn = _endpointToColumn(_pData, _end);
            const unsigned int totalRowsInRange = _rowCountInRange(_pData);
            const TextBuffer& textBuffer = _pData->GetTextBuffer();

#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
            std::wstringstream ss;
            ss << L"---Initial span start=" << _start << L" and end=" << _end << L"\n";
            ss << L"----Retrieving sr:" << startScreenInfoRow << L" sc:" << startColumn << L" er:" << endScreenInfoRow << L" ec:" << endColumn << L"\n";
            OutputDebugString(ss.str().c_str());
#endif

            ScreenInfoRow currentScreenInfoRow = 0;
            for (unsigned int i = 0; i < totalRowsInRange; ++i)
            {
                currentScreenInfoRow = startScreenInfoRow + i;
                const ROW& row = textBuffer.GetRowByOffset(currentScreenInfoRow);
                if (row.GetCharRow().ContainsText())
                {
                    const size_t rowRight = row.GetCharRow().MeasureRight();
                    size_t startIndex = 0;
                    size_t endIndex = rowRight;
                    if (currentScreenInfoRow == startScreenInfoRow)
                    {
                        startIndex = startColumn;
                    }
                    if (currentScreenInfoRow == endScreenInfoRow)
                    {
                        // prevent the end from going past the last non-whitespace char in the row
                        endIndex = std::min(gsl::narrow_cast<size_t>(endColumn) + 1, rowRight);
                    }

                    // if startIndex >= endIndex then _start is
                    // further to the right than the last
                    // non-whitespace char in the row so there
                    // wouldn't be any text to grab.
                    if (startIndex < endIndex)
                    {
                        wstr += row.GetText().substr(startIndex, endIndex - startIndex);
                    }
                }

                if (currentScreenInfoRow != endScreenInfoRow)
                {
                    wstr += L"\r\n";
                }

                if (getPartialText && wstr.size() > static_cast<size_t>(maxLength))
                {
                    wstr.resize(maxLength);
                    break;
                }
            }
        }
        CATCH_RETURN();
    }

    *pRetVal = SysAllocString(wstr.c_str());

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgGetText apiMsg;
    apiMsg.Text = wstr.c_str();
    Tracing::s_TraceUia(this, ApiCall::GetText, &apiMsg);*/

#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
    std::wstringstream ss;
    ss << L"--------Retrieved Text Max Length(" << maxLength << L") [" << _id << L"]: " << wstr.c_str() << "\n";
    OutputDebugString(ss.str().c_str());
#endif

    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::Move(_In_ TextUnit unit,
                                      _In_ int count,
                                      _Out_ int* pRetVal)
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    *pRetVal = 0;
    if (count == 0)
    {
        return S_OK;
    }

    ApiMsgMove apiMsg;
    apiMsg.OriginalStart = _start;
    apiMsg.OriginalEnd = _end;
    apiMsg.Unit = unit;
    apiMsg.RequestedCount = count;
#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
    OutputDebugString(L"Move\n");
    _outputObjectState();

    std::wstringstream ss;
    ss << L" count: " << count;
    std::wstring data = ss.str();
    OutputDebugString(data.c_str());
    OutputDebugString(L"\n");
    _outputRowConversions();
#endif

    auto moveFunc = &_moveByDocument;
    if (unit == TextUnit::TextUnit_Character)
    {
        moveFunc = &_moveByCharacter;
    }
    else if (unit <= TextUnit::TextUnit_Line)
    {
        moveFunc = &_moveByLine;
    }

    const MovementDirection moveDirection = (count > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    std::pair<Endpoint, Endpoint> newEndpoints;

    try
    {
        const MoveState moveState{ _pData, *this, moveDirection };
        newEndpoints = moveFunc(_pData,
                                count,
                                moveState,
                                pRetVal);
    }
    CATCH_RETURN();

    _start = newEndpoints.first;
    _end = newEndpoints.second;

    // a range can't be degenerate after both endpoints have been
    // moved.
    _degenerate = false;

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*apiMsg.MovedCount = *pRetVal;
    Tracing::s_TraceUia(this, ApiCall::Move, &apiMsg);*/

    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::MoveEndpointByUnit(_In_ TextPatternRangeEndpoint endpoint,
                                                    _In_ TextUnit unit,
                                                    _In_ int count,
                                                    _Out_ int* pRetVal)
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    RETURN_HR_IF(E_INVALIDARG, pRetVal == nullptr);
    *pRetVal = 0;
    if (count == 0)
    {
        return S_OK;
    }

    ApiMsgMoveEndpointByUnit apiMsg;
    apiMsg.OriginalStart = _start;
    apiMsg.OriginalEnd = _end;
    apiMsg.Endpoint = endpoint;
    apiMsg.Unit = unit;
    apiMsg.RequestedCount = count;
#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
    OutputDebugString(L"MoveEndpointByUnit\n");
    _outputObjectState();

    std::wstringstream ss;
    ss << L" endpoint: " << endpoint;
    ss << L" count: " << count;
    std::wstring data = ss.str();
    OutputDebugString(data.c_str());
    OutputDebugString(L"\n");
    _outputRowConversions();
#endif

    const MovementDirection moveDirection = (count > 0) ? MovementDirection::Forward : MovementDirection::Backward;

    auto moveFunc = &_moveEndpointByUnitDocument;
    if (unit == TextUnit::TextUnit_Character)
    {
        moveFunc = &_moveEndpointByUnitCharacter;
    }
    else if (unit <= TextUnit::TextUnit_Line)
    {
        moveFunc = &_moveEndpointByUnitLine;
    }

    std::tuple<Endpoint, Endpoint, bool> moveResults;
    try
    {
        const MoveState moveState{ _pData, *this, moveDirection };
        moveResults = moveFunc(_pData, count, endpoint, moveState, pRetVal);
    }
    CATCH_RETURN();

    _start = std::get<0>(moveResults);
    _end = std::get<1>(moveResults);
    _degenerate = std::get<2>(moveResults);

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*apiMsg.MovedCount = *pRetVal;
    Tracing::s_TraceUia(this, ApiCall::MoveEndpointByUnit, &apiMsg);*/

    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::MoveEndpointByRange(_In_ TextPatternRangeEndpoint endpoint,
                                                     _In_ ITextRangeProvider* pTargetRange,
                                                     _In_ TextPatternRangeEndpoint targetEndpoint)
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    const UiaTextRangeBase* range = static_cast<UiaTextRangeBase*>(pTargetRange);
    if (range == nullptr)
    {
        return E_INVALIDARG;
    }

    ApiMsgMoveEndpointByRange apiMsg;
    apiMsg.OriginalStart = _start;
    apiMsg.OriginalEnd = _end;
    apiMsg.Endpoint = endpoint;
    apiMsg.TargetEndpoint = targetEndpoint;
    apiMsg.OtherId = range->GetId();
#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
    OutputDebugString(L"MoveEndpointByRange\n");
    _outputObjectState();

    std::wstringstream ss;
    ss << L" endpoint: " << endpoint;
    ss << L" targetRange: " << range->_id;
    ss << L" targetEndpoint: " << targetEndpoint;
    std::wstring data = ss.str();
    OutputDebugString(data.c_str());
    OutputDebugString(L"\n");
    _outputRowConversions();
#endif

    // get the value that we're updating to
    Endpoint targetEndpointValue = 0;
    if (targetEndpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
    {
        targetEndpointValue = range->GetStart();

        // If we're moving our end relative to their start, we actually have to back up one from
        // their start position because this operation treats it as exclusive.
        if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_End)
        {
            if (targetEndpointValue > 0)
            {
                targetEndpointValue--;
            }
        }
    }
    else
    {
        targetEndpointValue = range->GetEnd();

        // If we're moving our start relative to their end, we actually have to sit one after
        // their end position as it was stored inclusive and we're doing this as an exclusive operation.
        if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
        {
            targetEndpointValue++;
        }
    }

    try
    {
        // convert then endpoints to screen info rows/columns
        const auto startScreenInfoRow = _endpointToScreenInfoRow(_pData, _start);
        const auto startColumn = _endpointToColumn(_pData, _start);
        const auto endScreenInfoRow = _endpointToScreenInfoRow(_pData, _end);
        const auto endColumn = _endpointToColumn(_pData, _end);
        const auto targetScreenInfoRow = _endpointToScreenInfoRow(_pData, targetEndpointValue);
        const auto targetColumn = _endpointToColumn(_pData, targetEndpointValue);

        // set endpoint value and check for crossed endpoints
        bool crossedEndpoints = false;
        if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
        {
            _start = targetEndpointValue;
            if (_compareScreenCoords(_pData, endScreenInfoRow, endColumn, targetScreenInfoRow, targetColumn) == -1)
            {
                // endpoints were crossed
                _end = _start;
                crossedEndpoints = true;
            }
        }
        else
        {
            _end = targetEndpointValue;
            if (_compareScreenCoords(_pData, startScreenInfoRow, startColumn, targetScreenInfoRow, targetColumn) == 1)
            {
                // endpoints were crossed
                _start = _end;
                crossedEndpoints = true;
            }
        }
        _degenerate = crossedEndpoints;
    }
    CATCH_RETURN();

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::MoveEndpointByRange, &apiMsg);
    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::Select()
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    if (_degenerate)
    {
        // calling Select on a degenerate range should clear any current selections
        _pData->ClearSelection();
    }
    else
    {
        const COORD coordStart{ gsl::narrow<SHORT>(_endpointToColumn(_pData, _start)),
                                gsl::narrow<SHORT>(_endpointToScreenInfoRow(_pData, _start)) };
        const COORD coordEnd{ gsl::narrow<SHORT>(_endpointToColumn(_pData, _end)),
                              gsl::narrow<SHORT>(_endpointToScreenInfoRow(_pData, _end)) };

        _pData->SelectNewRegion(coordStart, coordEnd);
    }

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::Select, nullptr);
    return S_OK;
}

// we don't support this
IFACEMETHODIMP UiaTextRangeBase::AddToSelection() noexcept
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::AddToSelection, nullptr);
    return E_NOTIMPL;
}

// we don't support this
IFACEMETHODIMP UiaTextRangeBase::RemoveFromSelection() noexcept
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::RemoveFromSelection, nullptr);
    return E_NOTIMPL;
}

IFACEMETHODIMP UiaTextRangeBase::ScrollIntoView(_In_ BOOL alignToTop)
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    try
    {
        const auto oldViewport = _pData->GetViewport().ToInclusive();
        const auto viewportHeight = _getViewportHeight(oldViewport);
        // range rows
        const auto startScreenInfoRow = _endpointToScreenInfoRow(_pData, _start);
        const auto endScreenInfoRow = _endpointToScreenInfoRow(_pData, _end);
        // screen buffer rows
        const auto topRow = _getFirstScreenInfoRowIndex();
        const auto bottomRow = _getLastScreenInfoRowIndex(_pData);

        SMALL_RECT newViewport = oldViewport;

        // there's a bunch of +1/-1s here for setting the viewport. These
        // are to account for the inclusivity of the viewport boundaries.
        if (alignToTop)
        {
            // determine if we can align the start row to the top
            if (startScreenInfoRow + viewportHeight <= bottomRow)
            {
                // we can align to the top
                newViewport.Top = gsl::narrow<SHORT>(startScreenInfoRow);
                newViewport.Bottom = gsl::narrow<SHORT>(startScreenInfoRow + viewportHeight - 1);
            }
            else
            {
                // we can align to the top so we'll just move the viewport
                // to the bottom of the screen buffer
                newViewport.Bottom = gsl::narrow<SHORT>(bottomRow);
                newViewport.Top = gsl::narrow<SHORT>(bottomRow - viewportHeight + 1);
            }
        }
        else
        {
            // we need to align to the bottom
            // check if we can align to the bottom
            if (endScreenInfoRow >= viewportHeight)
            {
                // we can align to bottom
                newViewport.Bottom = gsl::narrow<SHORT>(endScreenInfoRow);
                newViewport.Top = gsl::narrow<SHORT>(endScreenInfoRow - viewportHeight + 1);
            }
            else
            {
                // we can't align to bottom so we'll move the viewport to
                // the top of the screen buffer
                newViewport.Top = gsl::narrow<SHORT>(topRow);
                newViewport.Bottom = gsl::narrow<SHORT>(topRow + viewportHeight - 1);
            }
        }

        FAIL_FAST_IF(!(newViewport.Top >= gsl::narrow<SHORT>(topRow)));
        FAIL_FAST_IF(!(newViewport.Bottom <= gsl::narrow<SHORT>(bottomRow)));
        FAIL_FAST_IF(!(_getViewportHeight(oldViewport) == _getViewportHeight(newViewport)));

        _ChangeViewport(newViewport);

        // TODO GitHub #1914: Re-attach Tracing to UIA Tree
        // tracing
        /*ApiMsgScrollIntoView apiMsg;
        apiMsg.AlignToTop = !!alignToTop;
        Tracing::s_TraceUia(this, ApiCall::ScrollIntoView, &apiMsg);*/
    }
    CATCH_RETURN();
    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::GetChildren(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal) noexcept
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetChildren, nullptr);

    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);

    // we don't have any children
    *ppRetVal = SafeArrayCreateVector(VT_UNKNOWN, 0, 0);
    if (*ppRetVal == nullptr)
    {
        return E_OUTOFMEMORY;
    }
    return S_OK;
}

#pragma endregion

const COORD UiaTextRangeBase::_getScreenBufferCoords(gsl::not_null<IUiaData*> pData)
{
    return pData->GetTextBuffer().GetSize().Dimensions();
}

const COORD UiaTextRangeBase::_getScreenFontSize() const
{
    COORD coordRet = _pData->GetFontInfo().GetSize();

    // For sanity's sake, make sure not to leak 0 out as a possible value. These values are used in division operations.
    coordRet.X = std::max(coordRet.X, 1i16);
    coordRet.Y = std::max(coordRet.Y, 1i16);

    return coordRet;
}

// Routine Description:
// - Gets the number of rows in the output text buffer.
// Arguments:
// - <none>
// Return Value:
// - The number of rows
const unsigned int UiaTextRangeBase::_getTotalRows(gsl::not_null<IUiaData*> pData) noexcept
{
    return pData->GetTextBuffer().TotalRowCount();
}

// Routine Description:
// - Gets the width of the screen buffer rows
// Arguments:
// - <none>
// Return Value:
// - The row width
const unsigned int UiaTextRangeBase::_getRowWidth(gsl::not_null<IUiaData*> pData)
{
    // make sure that we can't leak a 0
    return std::max(static_cast<unsigned int>(_getScreenBufferCoords(pData).X), 1u);
}

// Routine Description:
// - calculates the column refered to by the endpoint.
// Arguments:
// - endpoint - the endpoint to translate
// Return Value:
// - the column value
const Column UiaTextRangeBase::_endpointToColumn(gsl::not_null<IUiaData*> pData, const Endpoint endpoint)
{
    return endpoint % _getRowWidth(pData);
}

// Routine Description:
// - converts an Endpoint into its equivalent text buffer row.
// Arguments:
// - endpoint - the endpoint to convert
// Return Value:
// - the text buffer row value
const TextBufferRow UiaTextRangeBase::_endpointToTextBufferRow(gsl::not_null<IUiaData*> pData,
                                                               const Endpoint endpoint)
{
    return endpoint / _getRowWidth(pData);
}

// Routine Description:
// - counts the number of rows that are fully or partially part of the
// range.
// Arguments:
// - <none>
// Return Value:
// - The number of rows in the range.
const unsigned int UiaTextRangeBase::_rowCountInRange(gsl::not_null<IUiaData*> pData) const
{
    if (_degenerate)
    {
        return 0;
    }

    const ScreenInfoRow startScreenInfoRow = _endpointToScreenInfoRow(pData, _start);
    const Column startColumn = _endpointToColumn(pData, _start);
    const ScreenInfoRow endScreenInfoRow = _endpointToScreenInfoRow(pData, _end);
    const Column endColumn = _endpointToColumn(pData, _end);

    FAIL_FAST_IF(!(_compareScreenCoords(pData, startScreenInfoRow, startColumn, endScreenInfoRow, endColumn) <= 0));

    // + 1 to balance subtracting ScreenInfoRows from each other
    return endScreenInfoRow - startScreenInfoRow + 1;
}

// Routine Description:
// - Converts a TextBufferRow to a ScreenInfoRow.
// Arguments:
// - row - the TextBufferRow to convert
// Return Value:
// - the equivalent ScreenInfoRow.
const ScreenInfoRow UiaTextRangeBase::_textBufferRowToScreenInfoRow(gsl::not_null<IUiaData*> pData,
                                                                    const TextBufferRow row) noexcept
{
    const int firstRowIndex = pData->GetTextBuffer().GetFirstRowIndex();
    return _normalizeRow(pData, row - firstRowIndex);
}

// Routine Description:
// - Converts a ScreenInfoRow to a ViewportRow. Uses the default
// viewport for the conversion.
// Arguments:
// - row - the ScreenInfoRow to convert
// Return Value:
// - the equivalent ViewportRow.
const ViewportRow UiaTextRangeBase::_screenInfoRowToViewportRow(gsl::not_null<IUiaData*> pData, const ScreenInfoRow row) noexcept
{
    const SMALL_RECT viewport = pData->GetViewport().ToInclusive();
    return _screenInfoRowToViewportRow(row, viewport);
}

// Routine Description:
// - normalizes the row index to within the bounds of the output
// buffer. The output buffer stores the text in a circular buffer so
// this method makes sure that we circle around gracefully.
// Arguments:
// - the non-normalized row index
// Return Value:
// - the normalized row index
const Row UiaTextRangeBase::_normalizeRow(gsl::not_null<IUiaData*> pData, const Row row) noexcept
{
    const unsigned int totalRows = _getTotalRows(pData);
    return ((row + totalRows) % totalRows);
}

// Routine Description:
// - Gets the viewport height, measured in char rows.
// Arguments:
// - viewport - The viewport to measure
// Return Value:
// - The viewport height
const unsigned int UiaTextRangeBase::_getViewportHeight(const SMALL_RECT viewport) noexcept
{
    FAIL_FAST_IF(!(viewport.Bottom >= viewport.Top));
    // + 1 because COORD is inclusive on both sides so subtracting top
    // and bottom gets rid of 1 more then it should.
    return viewport.Bottom - viewport.Top + 1;
}

// Routine Description:
// - Gets the viewport width, measured in char columns.
// Arguments:
// - viewport - The viewport to measure
// Return Value:
// - The viewport width
const unsigned int UiaTextRangeBase::_getViewportWidth(const SMALL_RECT viewport) noexcept
{
    FAIL_FAST_IF(!(viewport.Right >= viewport.Left));

    // + 1 because COORD is inclusive on both sides so subtracting left
    // and right gets rid of 1 more then it should.
    return (viewport.Right - viewport.Left + 1);
}

// Routine Description:
// - checks if the row is currently visible in the viewport. Uses the
// default viewport.
// Arguments:
// - row - the screen info row to check
// Return Value:
// - true if the row is within the bounds of the viewport
const bool UiaTextRangeBase::_isScreenInfoRowInViewport(gsl::not_null<IUiaData*> pData,
                                                        const ScreenInfoRow row) noexcept
{
    return _isScreenInfoRowInViewport(row, pData->GetViewport().ToInclusive());
}

// Routine Description:
// - checks if the row is currently visible in the viewport
// Arguments:
// - row - the row to check
// - viewport - the viewport to use for the bounds
// Return Value:
// - true if the row is within the bounds of the viewport
const bool UiaTextRangeBase::_isScreenInfoRowInViewport(const ScreenInfoRow row,
                                                        const SMALL_RECT viewport) noexcept
{
    const ViewportRow viewportRow = _screenInfoRowToViewportRow(row, viewport);
    return viewportRow >= 0 &&
           viewportRow < gsl::narrow<ViewportRow>(_getViewportHeight(viewport));
}

// Routine Description:
// - Converts a ScreenInfoRow to a TextBufferRow.
// Arguments:
// - row - the ScreenInfoRow to convert
// Return Value:
// - the equivalent TextBufferRow.
const TextBufferRow UiaTextRangeBase::_screenInfoRowToTextBufferRow(gsl::not_null<IUiaData*> pData,
                                                                    const ScreenInfoRow row) noexcept
{
    const TextBufferRow firstRowIndex = pData->GetTextBuffer().GetFirstRowIndex();
    return _normalizeRow(pData, row + firstRowIndex);
}

// Routine Description:
// - Converts a TextBufferRow to an Endpoint.
// Arguments:
// - row - the TextBufferRow to convert
// Return Value:
// - the equivalent Endpoint, starting at the beginning of the TextBufferRow.
const Endpoint UiaTextRangeBase::_textBufferRowToEndpoint(gsl::not_null<IUiaData*> pData, const TextBufferRow row)
{
    return _getRowWidth(pData) * row;
}

// Routine Description:
// - Converts a ScreenInfoRow to an Endpoint.
// Arguments:
// - row - the ScreenInfoRow to convert
// Return Value:
// - the equivalent Endpoint.
const Endpoint UiaTextRangeBase::_screenInfoRowToEndpoint(gsl::not_null<IUiaData*> pData,
                                                          const ScreenInfoRow row)
{
    return _textBufferRowToEndpoint(pData, _screenInfoRowToTextBufferRow(pData, row));
}

// Routine Description:
// - Converts an Endpoint to an ScreenInfoRow.
// Arguments:
// - endpoint - the endpoint to convert
// Return Value:
// - the equivalent ScreenInfoRow.
const ScreenInfoRow UiaTextRangeBase::_endpointToScreenInfoRow(gsl::not_null<IUiaData*> pData,
                                                               const Endpoint endpoint)
{
    return _textBufferRowToScreenInfoRow(pData, _endpointToTextBufferRow(pData, endpoint));
}

// Routine Description:
// - adds the relevant coordinate points from screenInfoRow to coords.
// Arguments:
// - screenInfoRow - row to calculate coordinate positions from
// - coords - vector to add the calucated coords to
// Return Value:
// - <none>
// Notes:
// - alters coords. may throw an exception.
void UiaTextRangeBase::_addScreenInfoRowBoundaries(gsl::not_null<IUiaData*> pData,
                                                   const ScreenInfoRow screenInfoRow,
                                                   _Inout_ std::vector<double>& coords) const
{
    const COORD currentFontSize = _getScreenFontSize();

    POINT topLeft{ 0 };
    POINT bottomRight{ 0 };

    if (_endpointToScreenInfoRow(pData, _start) == screenInfoRow)
    {
        // start is somewhere in this row so we start from its position
        topLeft.x = _endpointToColumn(pData, _start) * currentFontSize.X;
    }
    else
    {
        // otherwise we start from the beginning of the row
        topLeft.x = 0;
    }

    topLeft.y = _screenInfoRowToViewportRow(pData, screenInfoRow) * currentFontSize.Y;

    if (_endpointToScreenInfoRow(pData, _end) == screenInfoRow)
    {
        // the endpoints are on the same row
        bottomRight.x = (_endpointToColumn(pData, _end) + 1) * currentFontSize.X;
    }
    else
    {
        // _end is not on this row so span to the end of the row
        bottomRight.x = _getViewportWidth(_pData->GetViewport().ToInclusive()) * currentFontSize.X;
    }

    // we add the font height only once here because we are adding each line individually
    bottomRight.y = topLeft.y + currentFontSize.Y;

    // convert the coords to be relative to the screen instead of
    // the client window
    _TranslatePointToScreen(&topLeft);
    _TranslatePointToScreen(&bottomRight);

    const LONG width = bottomRight.x - topLeft.x;
    const LONG height = bottomRight.y - topLeft.y;

    // insert the coords
    coords.push_back(topLeft.x);
    coords.push_back(topLeft.y);
    coords.push_back(width);
    coords.push_back(height);
}

// Routine Description:
// - returns the index of the first row of the screen info
// Arguments:
// - <none>
// Return Value:
// - the index of the first row (0-indexed) of the screen info
const unsigned int UiaTextRangeBase::_getFirstScreenInfoRowIndex() noexcept
{
    return 0;
}

// Routine Description:
// - returns the index of the last row of the screen info
// Arguments:
// - <none>
// Return Value:
// - the index of the last row (0-indexed) of the screen info
const unsigned int UiaTextRangeBase::_getLastScreenInfoRowIndex(gsl::not_null<IUiaData*> pData) noexcept
{
    return _getTotalRows(pData) - 1;
}

// Routine Description:
// - returns the index of the first column of the screen info rows
// Arguments:
// - <none>
// Return Value:
// - the index of the first column (0-indexed) of the screen info rows
const Column UiaTextRangeBase::_getFirstColumnIndex() noexcept
{
    return 0;
}

// Routine Description:
// - returns the index of the last column of the screen info rows
// Arguments:
// - <none>
// Return Value:
// - the index of the last column (0-indexed) of the screen info rows
const Column UiaTextRangeBase::_getLastColumnIndex(gsl::not_null<IUiaData*> pData)
{
    return _getRowWidth(pData) - 1;
}

// Routine Description:
// - Compares two sets of screen info coordinates
// Arguments:
// - rowA - the row index of the first position
// - colA - the column index of the first position
// - rowB - the row index of the second position
// - colB - the column index of the second position
// Return Value:
//   -1 if A < B
//    1 if A > B
//    0 if A == B
const int UiaTextRangeBase::_compareScreenCoords(gsl::not_null<IUiaData*> pData,
                                                 const ScreenInfoRow rowA,
                                                 const Column colA,
                                                 const ScreenInfoRow rowB,
                                                 const Column colB)
{
    FAIL_FAST_IF(!(rowA >= _getFirstScreenInfoRowIndex()));
    FAIL_FAST_IF(!(rowA <= _getLastScreenInfoRowIndex(pData)));

    FAIL_FAST_IF(!(colA >= _getFirstColumnIndex()));
    FAIL_FAST_IF(!(colA <= _getLastColumnIndex(pData)));

    FAIL_FAST_IF(!(rowB >= _getFirstScreenInfoRowIndex()));
    FAIL_FAST_IF(!(rowB <= _getLastScreenInfoRowIndex(pData)));

    FAIL_FAST_IF(!(colB >= _getFirstColumnIndex()));
    FAIL_FAST_IF(!(colB <= _getLastColumnIndex(pData)));

    if (rowA < rowB)
    {
        return -1;
    }
    else if (rowA > rowB)
    {
        return 1;
    }
    // rowA == rowB
    else if (colA < colB)
    {
        return -1;
    }
    else if (colA > colB)
    {
        return 1;
    }
    // colA == colB
    return 0;
}

// Routine Description:
// - calculates new Endpoints if they were to be moved moveCount times
// by character.
// Arguments:
// - moveCount - the number of times to move
// - moveState - values indicating the state of the console for the
// move operation
// - pAmountMoved - the number of times that the return values are "moved"
// Return Value:
// - a pair of endpoints of the form <start, end>
std::pair<Endpoint, Endpoint> UiaTextRangeBase::_moveByCharacter(gsl::not_null<IUiaData*> pData,
                                                                 const int moveCount,
                                                                 const MoveState moveState,
                                                                 _Out_ gsl::not_null<int*> const pAmountMoved)
{
    if (moveState.Direction == MovementDirection::Forward)
    {
        return _moveByCharacterForward(pData, moveCount, moveState, pAmountMoved);
    }
    else
    {
        return _moveByCharacterBackward(pData, moveCount, moveState, pAmountMoved);
    }
}

std::pair<Endpoint, Endpoint> UiaTextRangeBase::_moveByCharacterForward(gsl::not_null<IUiaData*> pData,
                                                                        const int moveCount,
                                                                        const MoveState moveState,
                                                                        _Out_ gsl::not_null<int*> const pAmountMoved)
{
    *pAmountMoved = 0;
    const int count = moveCount;
    ScreenInfoRow currentScreenInfoRow = moveState.StartScreenInfoRow;
    Column currentColumn = moveState.StartColumn;

    for (int i = 0; i < abs(count); ++i)
    {
        // get the current row's right
        const ROW& row = pData->GetTextBuffer().GetRowByOffset(currentScreenInfoRow);
        const size_t right = row.GetCharRow().MeasureRight();

        // check if we're at the edge of the screen info buffer
        if (currentScreenInfoRow == moveState.LimitingRow &&
            gsl::narrow_cast<size_t>(currentColumn) + 1 >= right)
        {
            break;
        }
        else if (gsl::narrow_cast<size_t>(currentColumn) + 1 >= right)
        {
            // we're at the edge of a row and need to go to the next one
            currentColumn = moveState.FirstColumnInRow;
            currentScreenInfoRow += static_cast<int>(moveState.Increment);
        }
        else
        {
            // moving somewhere away from the edges of a row
            currentColumn += static_cast<int>(moveState.Increment);
        }
        *pAmountMoved += static_cast<int>(moveState.Increment);

        FAIL_FAST_IF(!(currentColumn >= _getFirstColumnIndex()));
        FAIL_FAST_IF(!(currentColumn <= _getLastColumnIndex(pData)));
        FAIL_FAST_IF(!(currentScreenInfoRow >= _getFirstScreenInfoRowIndex()));
        FAIL_FAST_IF(!(currentScreenInfoRow <= _getLastScreenInfoRowIndex(pData)));
    }

    Endpoint start = _screenInfoRowToEndpoint(pData, currentScreenInfoRow) + currentColumn;
    Endpoint end = start;
    return std::make_pair<Endpoint, Endpoint>(std::move(start), std::move(end));
}

std::pair<Endpoint, Endpoint> UiaTextRangeBase::_moveByCharacterBackward(gsl::not_null<IUiaData*> pData,
                                                                         const int moveCount,
                                                                         const MoveState moveState,
                                                                         _Out_ gsl::not_null<int*> const pAmountMoved)
{
    *pAmountMoved = 0;
    const int count = moveCount;
    ScreenInfoRow currentScreenInfoRow = moveState.StartScreenInfoRow;
    Column currentColumn = moveState.StartColumn;

    for (int i = 0; i < abs(count); ++i)
    {
        // check if we're at the edge of the screen info buffer
        if (currentScreenInfoRow == moveState.LimitingRow &&
            currentColumn == moveState.LastColumnInRow)
        {
            break;
        }
        else if (currentColumn == moveState.LastColumnInRow)
        {
            // we're at the edge of a row and need to go to the
            // next one. move to the cell with the last non-whitespace charactor

            currentScreenInfoRow += static_cast<int>(moveState.Increment);
            // get the right cell for the next row
            const ROW& row = pData->GetTextBuffer().GetRowByOffset(currentScreenInfoRow);
            const size_t right = row.GetCharRow().MeasureRight();
            currentColumn = gsl::narrow<Column>((right == 0) ? 0 : right - 1);
        }
        else
        {
            // moving somewhere away from the edges of a row
            currentColumn += static_cast<int>(moveState.Increment);
        }
        *pAmountMoved += static_cast<int>(moveState.Increment);

        FAIL_FAST_IF(!(currentColumn >= _getFirstColumnIndex()));
        FAIL_FAST_IF(!(currentColumn <= _getLastColumnIndex(pData)));
        FAIL_FAST_IF(!(currentScreenInfoRow >= _getFirstScreenInfoRowIndex()));
        FAIL_FAST_IF(!(currentScreenInfoRow <= _getLastScreenInfoRowIndex(pData)));
    }

    Endpoint start = _screenInfoRowToEndpoint(pData, currentScreenInfoRow) + currentColumn;
    Endpoint end = start;
    return std::make_pair<Endpoint, Endpoint>(std::move(start), std::move(end));
}

// Routine Description:
// - calculates new Endpoints if they were to be moved moveCount times
// by line.
// Arguments:
// - moveCount - the number of times to move
// - moveState - values indicating the state of the console for the
// move operation
// - pAmountMoved - the number of times that the return values are "moved"
// Return Value:
// - a pair of endpoints of the form <start, end>
std::pair<Endpoint, Endpoint> UiaTextRangeBase::_moveByLine(gsl::not_null<IUiaData*> pData,
                                                            const int moveCount,
                                                            const MoveState moveState,
                                                            _Out_ gsl::not_null<int*> const pAmountMoved)
{
    *pAmountMoved = 0;
    Endpoint start = _screenInfoRowToEndpoint(pData, moveState.StartScreenInfoRow) + moveState.StartColumn;
    Endpoint end = _screenInfoRowToEndpoint(pData, moveState.EndScreenInfoRow) + moveState.EndColumn;
    ScreenInfoRow currentScreenInfoRow = moveState.StartScreenInfoRow;
    // we don't want to move the range if we're already in the
    // limiting row and trying to move off the end of the screen buffer
    const bool illegalMovement = (currentScreenInfoRow == moveState.LimitingRow &&
                                  ((moveCount < 0 && moveState.Increment == MovementIncrement::Backward) ||
                                   (moveCount > 0 && moveState.Increment == MovementIncrement::Forward)));

    if (moveCount != 0 && !illegalMovement)
    {
        // move the range
        for (int i = 0; i < abs(moveCount); ++i)
        {
            if (currentScreenInfoRow == moveState.LimitingRow)
            {
                break;
            }
            currentScreenInfoRow += static_cast<int>(moveState.Increment);
            *pAmountMoved += static_cast<int>(moveState.Increment);

            FAIL_FAST_IF(!(currentScreenInfoRow >= _getFirstScreenInfoRowIndex()));
            FAIL_FAST_IF(!(currentScreenInfoRow <= _getLastScreenInfoRowIndex(pData)));
        }
        start = _screenInfoRowToEndpoint(pData, currentScreenInfoRow);
        end = start + _getLastColumnIndex(pData);
    }

    return std::make_pair<Endpoint, Endpoint>(std::move(start), std::move(end));
}

// Routine Description:
// - calculates new Endpoints if they were to be moved moveCount times
// by document.
// Arguments:
// - moveCount - the number of times to move
// - moveState - values indicating the state of the console for the
// move operation
// - pAmountMoved - the number of times that the return values are "moved"
// Return Value:
// - a pair of endpoints of the form <start, end>
std::pair<Endpoint, Endpoint> UiaTextRangeBase::_moveByDocument(gsl::not_null<IUiaData*> pData,
                                                                const int /*moveCount*/,
                                                                const MoveState moveState,
                                                                _Out_ gsl::not_null<int*> const pAmountMoved)
{
    // We can't move by anything larger than a line, so move by document will apply and will
    // just report that it can't do that.
    *pAmountMoved = 0;

    // We then have to return the same endpoints as what we initially had so nothing happens.
    Endpoint start = _screenInfoRowToEndpoint(pData, moveState.StartScreenInfoRow) + moveState.StartColumn;
    Endpoint end = _screenInfoRowToEndpoint(pData, moveState.EndScreenInfoRow) + moveState.EndColumn;

    return std::make_pair<Endpoint, Endpoint>(std::move(start), std::move(end));
}

// Routine Description:
// - calculates new Endpoints/degenerate state if the indicated
// endpoint was moved moveCount times by character.
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - moveState - values indicating the state of the console for the
// move operation
// - pAmountMoved - the number of times that the return values are "moved"
// Return Value:
// - A tuple of elements of the form <start, end, degenerate>
std::tuple<Endpoint, Endpoint, bool> UiaTextRangeBase::_moveEndpointByUnitCharacter(gsl::not_null<IUiaData*> pData,
                                                                                    const int moveCount,
                                                                                    const TextPatternRangeEndpoint endpoint,
                                                                                    const MoveState moveState,
                                                                                    _Out_ gsl::not_null<int*> const pAmountMoved)
{
    if (moveState.Direction == MovementDirection::Forward)
    {
        return _moveEndpointByUnitCharacterForward(pData, moveCount, endpoint, moveState, pAmountMoved);
    }
    else
    {
        return _moveEndpointByUnitCharacterBackward(pData, moveCount, endpoint, moveState, pAmountMoved);
    }
}

std::tuple<Endpoint, Endpoint, bool>
UiaTextRangeBase::_moveEndpointByUnitCharacterForward(gsl::not_null<IUiaData*> pData,
                                                      const int moveCount,
                                                      const TextPatternRangeEndpoint endpoint,
                                                      const MoveState moveState,
                                                      _Out_ gsl::not_null<int*> const pAmountMoved)
{
    *pAmountMoved = 0;
    const int count = moveCount;
    ScreenInfoRow currentScreenInfoRow = 0;
    Column currentColumn = 0;

    // set current location vars
    if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
    {
        currentScreenInfoRow = moveState.StartScreenInfoRow;
        currentColumn = moveState.StartColumn;
    }
    else
    {
        currentScreenInfoRow = moveState.EndScreenInfoRow;
        currentColumn = moveState.EndColumn;
    }

    for (int i = 0; i < abs(count); ++i)
    {
        // get the current row's right
        const ROW& row = pData->GetTextBuffer().GetRowByOffset(currentScreenInfoRow);
        const size_t right = row.GetCharRow().MeasureRight();

        // check if we're at the edge of the screen info buffer
        if (currentScreenInfoRow == moveState.LimitingRow &&
            gsl::narrow_cast<size_t>(currentColumn) + 1 >= right)
        {
            break;
        }
        else if (gsl::narrow_cast<size_t>(currentColumn) + 1 >= right)
        {
            // we're at the edge of a row and need to go to the next one
            currentColumn = moveState.FirstColumnInRow;
            currentScreenInfoRow += static_cast<int>(moveState.Increment);
        }
        else
        {
            // moving somewhere away from the edges of a row
            currentColumn += static_cast<int>(moveState.Increment);
        }
        *pAmountMoved += static_cast<int>(moveState.Increment);

        FAIL_FAST_IF(!(currentColumn >= _getFirstColumnIndex()));
        FAIL_FAST_IF(!(currentColumn <= _getLastColumnIndex(pData)));
        FAIL_FAST_IF(!(currentScreenInfoRow >= _getFirstScreenInfoRowIndex()));
        FAIL_FAST_IF(!(currentScreenInfoRow <= _getLastScreenInfoRowIndex(pData)));
    }

    // translate the row back to an endpoint and handle any crossed endpoints
    const Endpoint convertedEndpoint = _screenInfoRowToEndpoint(pData, currentScreenInfoRow) + currentColumn;
    Endpoint start = _screenInfoRowToEndpoint(pData, moveState.StartScreenInfoRow) + moveState.StartColumn;
    Endpoint end = _screenInfoRowToEndpoint(pData, moveState.EndScreenInfoRow) + moveState.EndColumn;
    bool degenerate = false;
    if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
    {
        start = convertedEndpoint;
        if (_compareScreenCoords(pData,
                                 currentScreenInfoRow,
                                 currentColumn,
                                 moveState.EndScreenInfoRow,
                                 moveState.EndColumn) == 1)
        {
            end = start;
            degenerate = true;
        }
    }
    else
    {
        end = convertedEndpoint;
        if (_compareScreenCoords(pData,
                                 currentScreenInfoRow,
                                 currentColumn,
                                 moveState.StartScreenInfoRow,
                                 moveState.StartColumn) == -1)
        {
            start = end;
            degenerate = true;
        }
    }
    return std::make_tuple(start, end, degenerate);
}

std::tuple<Endpoint, Endpoint, bool>
UiaTextRangeBase::_moveEndpointByUnitCharacterBackward(gsl::not_null<IUiaData*> pData,
                                                       const int moveCount,
                                                       const TextPatternRangeEndpoint endpoint,
                                                       const MoveState moveState,
                                                       _Out_ gsl::not_null<int*> const pAmountMoved)
{
    *pAmountMoved = 0;
    const int count = moveCount;
    ScreenInfoRow currentScreenInfoRow = 0;
    Column currentColumn = 0;

    // set current location vars
    if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
    {
        currentScreenInfoRow = moveState.StartScreenInfoRow;
        currentColumn = moveState.StartColumn;
    }
    else
    {
        currentScreenInfoRow = moveState.EndScreenInfoRow;
        currentColumn = moveState.EndColumn;
    }

    for (int i = 0; i < abs(count); ++i)
    {
        // check if we're at the edge of the screen info buffer
        if (currentScreenInfoRow == moveState.LimitingRow &&
            currentColumn == moveState.LastColumnInRow)
        {
            break;
        }
        else if (currentColumn == moveState.LastColumnInRow)
        {
            // we're at the edge of a row and need to go to the
            // next one. move to the cell with the last non-whitespace charactor

            currentScreenInfoRow += static_cast<int>(moveState.Increment);
            // get the right cell for the next row
            const ROW& row = pData->GetTextBuffer().GetRowByOffset(currentScreenInfoRow);
            const size_t right = row.GetCharRow().MeasureRight();
            currentColumn = gsl::narrow<Column>((right == 0) ? 0 : right - 1);
        }
        else
        {
            // moving somewhere away from the edges of a row
            currentColumn += static_cast<int>(moveState.Increment);
        }
        *pAmountMoved += static_cast<int>(moveState.Increment);

        FAIL_FAST_IF(!(currentColumn >= _getFirstColumnIndex()));
        FAIL_FAST_IF(!(currentColumn <= _getLastColumnIndex(pData)));
        FAIL_FAST_IF(!(currentScreenInfoRow >= _getFirstScreenInfoRowIndex()));
        FAIL_FAST_IF(!(currentScreenInfoRow <= _getLastScreenInfoRowIndex(pData)));
    }

    // translate the row back to an endpoint and handle any crossed endpoints
    const Endpoint convertedEndpoint = _screenInfoRowToEndpoint(pData, currentScreenInfoRow) + currentColumn;
    Endpoint start = _screenInfoRowToEndpoint(pData, moveState.StartScreenInfoRow) + moveState.StartColumn;
    Endpoint end = _screenInfoRowToEndpoint(pData, moveState.EndScreenInfoRow) + moveState.EndColumn;
    bool degenerate = false;
    if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
    {
        start = convertedEndpoint;
        if (_compareScreenCoords(pData,
                                 currentScreenInfoRow,
                                 currentColumn,
                                 moveState.EndScreenInfoRow,
                                 moveState.EndColumn) == 1)
        {
            end = start;
            degenerate = true;
        }
    }
    else
    {
        end = convertedEndpoint;
        if (_compareScreenCoords(pData,
                                 currentScreenInfoRow,
                                 currentColumn,
                                 moveState.StartScreenInfoRow,
                                 moveState.StartColumn) == -1)
        {
            start = end;
            degenerate = true;
        }
    }
    return std::make_tuple(start, end, degenerate);
}

// Routine Description:
// - calculates new Endpoints/degenerate state if the indicated
// endpoint was moved moveCount times by line.
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - moveState - values indicating the state of the console for the
// move operation
// - pAmountMoved - the number of times that the return values are "moved"
// Return Value:
// - A tuple of elements of the form <start, end, degenerate>
std::tuple<Endpoint, Endpoint, bool> UiaTextRangeBase::_moveEndpointByUnitLine(gsl::not_null<IUiaData*> pData,
                                                                               const int moveCount,
                                                                               const TextPatternRangeEndpoint endpoint,
                                                                               const MoveState moveState,
                                                                               _Out_ gsl::not_null<int*> const pAmountMoved)
{
    *pAmountMoved = 0;
    int count = moveCount;
    ScreenInfoRow currentScreenInfoRow = 0;
    Column currentColumn = 0;
    bool forceDegenerate = false;
    Endpoint start = _screenInfoRowToEndpoint(pData, moveState.StartScreenInfoRow) + moveState.StartColumn;
    Endpoint end = _screenInfoRowToEndpoint(pData, moveState.EndScreenInfoRow) + moveState.EndColumn;
    bool degenerate = false;

    if (moveCount == 0)
    {
        return std::make_tuple(start, end, degenerate);
    }

    const MovementDirection moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;

    if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
    {
        currentScreenInfoRow = moveState.StartScreenInfoRow;
        currentColumn = moveState.StartColumn;
    }
    else
    {
        currentScreenInfoRow = moveState.EndScreenInfoRow;
        currentColumn = moveState.EndColumn;
    }

    // check if we can't be moved anymore
    if (currentScreenInfoRow == moveState.LimitingRow &&
        currentColumn == moveState.LastColumnInRow)
    {
        return std::make_tuple(start, end, degenerate);
    }
    else if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start &&
             moveDirection == MovementDirection::Forward)
    {
        if (moveState.StartScreenInfoRow == moveState.LimitingRow)
        {
            // _start is somewhere on the limiting row but not at
            // the very end. move to the end of the last row
            count -= static_cast<int>(moveState.Increment);
            *pAmountMoved += static_cast<int>(moveState.Increment);
            currentColumn = _getLastColumnIndex(pData);
            forceDegenerate = true;
        }
        if (moveState.StartColumn != _getFirstColumnIndex())
        {
            // _start is somewhere in the middle of a row, so do a
            // partial movement to the beginning of the next row
            count -= static_cast<int>(moveState.Increment);
            *pAmountMoved += static_cast<int>(moveState.Increment);
            currentScreenInfoRow += static_cast<int>(moveState.Increment);
            currentColumn = _getFirstColumnIndex();
        }
    }
    else if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start &&
             moveDirection == MovementDirection::Backward)
    {
        if (moveState.StartColumn != _getFirstColumnIndex())
        {
            // moving backwards when we weren't already at the beginning of
            // the row so move there first to align with the text unit boundary
            count -= static_cast<int>(moveState.Increment);
            *pAmountMoved += static_cast<int>(moveState.Increment);
            currentColumn = _getFirstColumnIndex();
        }
    }
    else if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_End &&
             moveDirection == MovementDirection::Forward)
    {
        if (moveState.EndColumn != _getLastColumnIndex(pData))
        {
            // _end is not at the last column in a row, so we move
            // forward to it with a partial movement
            count -= static_cast<int>(moveState.Increment);
            *pAmountMoved += static_cast<int>(moveState.Increment);
            currentColumn = _getLastColumnIndex(pData);
        }
    }
    else
    {
        // _end moving backwards
        if (moveState.EndScreenInfoRow == moveState.LimitingRow)
        {
            // _end is somewhere on the limiting row but not at the
            // front. move it there
            count -= static_cast<int>(moveState.Increment);
            *pAmountMoved += static_cast<int>(moveState.Increment);
            currentColumn = _getFirstColumnIndex();
            forceDegenerate = true;
        }
        else if (moveState.EndColumn != _getLastColumnIndex(pData))
        {
            // _end is not at the last column in a row, so we move it
            // backwards to it with a partial move
            count -= static_cast<int>(moveState.Increment);
            *pAmountMoved += static_cast<int>(moveState.Increment);
            currentColumn = _getLastColumnIndex(pData);
            currentScreenInfoRow += static_cast<int>(moveState.Increment);
        }
    }

    FAIL_FAST_IF(!(currentColumn >= _getFirstColumnIndex()));
    FAIL_FAST_IF(!(currentColumn <= _getLastColumnIndex(pData)));
    FAIL_FAST_IF(!(currentScreenInfoRow >= _getFirstScreenInfoRowIndex()));
    FAIL_FAST_IF(!(currentScreenInfoRow <= _getLastScreenInfoRowIndex(pData)));

    // move the row that the endpoint corresponds to
    while (count != 0 && currentScreenInfoRow != moveState.LimitingRow)
    {
        count -= static_cast<int>(moveState.Increment);
        currentScreenInfoRow += static_cast<int>(moveState.Increment);
        *pAmountMoved += static_cast<int>(moveState.Increment);

        FAIL_FAST_IF(!(currentScreenInfoRow >= _getFirstScreenInfoRowIndex()));
        FAIL_FAST_IF(!(currentScreenInfoRow <= _getLastScreenInfoRowIndex(pData)));
    }

    // translate the row back to an endpoint and handle any crossed endpoints
    const Endpoint convertedEndpoint = _screenInfoRowToEndpoint(pData, currentScreenInfoRow) + currentColumn;
    if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
    {
        start = convertedEndpoint;
        if (currentScreenInfoRow > moveState.EndScreenInfoRow || forceDegenerate)
        {
            degenerate = true;
            end = start;
        }
    }
    else
    {
        end = convertedEndpoint;
        if (currentScreenInfoRow < moveState.StartScreenInfoRow || forceDegenerate)
        {
            degenerate = true;
            start = end;
        }
    }

    return std::make_tuple(start, end, degenerate);
}

// Routine Description:
// - calculates new Endpoints/degenerate state if the indicate
// endpoint was moved moveCount times by document.
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - moveState - values indicating the state of the console for the
// move operation
// - pAmountMoved - the number of times that the return values are "moved"
// Return Value:
// - A tuple of elements of the form <start, end, degenerate>
std::tuple<Endpoint, Endpoint, bool> UiaTextRangeBase::_moveEndpointByUnitDocument(gsl::not_null<IUiaData*> pData,
                                                                                   const int moveCount,
                                                                                   const TextPatternRangeEndpoint endpoint,
                                                                                   const MoveState moveState,
                                                                                   _Out_ gsl::not_null<int*> const pAmountMoved)
{
    *pAmountMoved = 0;

    Endpoint start = 0;
    Endpoint end = 0;
    bool degenerate = false;
    if (endpoint == TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start)
    {
        if (moveCount < 0)
        {
            // moving _start backwards
            start = _screenInfoRowToEndpoint(pData, _getFirstScreenInfoRowIndex()) + _getFirstColumnIndex();
            end = _screenInfoRowToEndpoint(pData, moveState.EndScreenInfoRow) + moveState.EndColumn;
            if (!(moveState.StartScreenInfoRow == _getFirstScreenInfoRowIndex() &&
                  moveState.StartColumn == _getFirstColumnIndex()))
            {
                *pAmountMoved += static_cast<int>(moveState.Increment);
            }
        }
        else
        {
            // moving _start forwards
            start = _screenInfoRowToEndpoint(pData, _getLastScreenInfoRowIndex(pData)) + _getLastColumnIndex(pData);
            end = start;
            degenerate = true;
            if (!(moveState.StartScreenInfoRow == _getLastScreenInfoRowIndex(pData) &&
                  moveState.StartColumn == _getLastColumnIndex(pData)))
            {
                *pAmountMoved += static_cast<int>(moveState.Increment);
            }
        }
    }
    else
    {
        if (moveCount < 0)
        {
            // moving _end backwards
            end = _screenInfoRowToEndpoint(pData, _getFirstScreenInfoRowIndex()) + _getFirstColumnIndex();
            start = end;
            degenerate = true;
            if (!(moveState.EndScreenInfoRow == _getFirstScreenInfoRowIndex() &&
                  moveState.EndColumn == _getFirstColumnIndex()))
            {
                *pAmountMoved += static_cast<int>(moveState.Increment);
            }
        }
        else
        {
            // moving _end forwards
            end = _screenInfoRowToEndpoint(pData, _getLastScreenInfoRowIndex(pData)) + _getLastColumnIndex(pData);
            start = _screenInfoRowToEndpoint(pData, moveState.StartScreenInfoRow) + moveState.StartColumn;
            if (!(moveState.EndScreenInfoRow == _getLastScreenInfoRowIndex(pData) &&
                  moveState.EndColumn == _getLastColumnIndex(pData)))
            {
                *pAmountMoved += static_cast<int>(moveState.Increment);
            }
        }
    }

    return std::make_tuple(start, end, degenerate);
}

COORD UiaTextRangeBase::_endpointToCoord(gsl::not_null<IUiaData*> pData, const Endpoint endpoint)
{
    return { gsl::narrow<short>(_endpointToColumn(pData, endpoint)), gsl::narrow<short>(_endpointToScreenInfoRow(pData, endpoint)) };
}

Endpoint UiaTextRangeBase::_coordToEndpoint(gsl::not_null<IUiaData*> pData,
                                            const COORD coord)
{
    return _screenInfoRowToEndpoint(pData, coord.Y) + coord.X;
}

RECT UiaTextRangeBase::_getTerminalRect() const
{
    UiaRect result{ 0 };

    IRawElementProviderFragment* pRawElementProviderFragment;
    THROW_IF_FAILED(_pProvider->QueryInterface<IRawElementProviderFragment>(&pRawElementProviderFragment));
    if (pRawElementProviderFragment)
    {
        pRawElementProviderFragment->get_BoundingRectangle(&result);
    }

    return {
        gsl::narrow<LONG>(result.left),
        gsl::narrow<LONG>(result.top),
        gsl::narrow<LONG>(result.left + result.width),
        gsl::narrow<LONG>(result.top + result.height)
    };
}
