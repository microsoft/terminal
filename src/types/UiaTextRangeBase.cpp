// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "UiaTextRangeBase.hpp"
#include "ScreenInfoUiaProviderBase.h"
#include "..\buffer\out\search.h"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Types::UiaTextRangeBaseTracing;

// toggle these for additional logging in a debug build
//#define _DEBUG 1
//#define UIATEXTRANGE_DEBUG_MSGS 1
#undef UIATEXTRANGE_DEBUG_MSGS

IdType UiaTextRangeBase::id = 1;

#if _DEBUG
#include <sstream>
void UiaTextRangeBase::_outputObjectState()
{
    std::wstringstream ss;
    ss << "Object State";
    ss << " _id: " << _id;
    ss << " _start: { " << _start.X << ", " << _start.Y << " }";
    ss << " _end: { " << _end.X << ", " << _end.Y << " }";
    ss << " _degenerate: " << IsDegenerate();

    std::wstring str = ss.str();
    OutputDebugString(str.c_str());
    OutputDebugString(L"\n");
}
#endif // _DEBUG

// degenerate range constructor.
#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(_In_ IUiaData* pData, _In_ IRawElementProviderSimple* const pProvider, _In_ std::wstring_view wordDelimiters) noexcept
try
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pProvider);
    RETURN_HR_IF_NULL(E_INVALIDARG, pData);

    _pProvider = pProvider;
    _pData = pData;
    _start = pData->GetViewport().Origin();
    _end = pData->GetViewport().Origin();
    _wordDelimiters = wordDelimiters;

    _id = id;
    ++id;

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgConstructor apiMsg;
    apiMsg.Id = _id;
    Tracing::s_TraceUia(nullptr, ApiCall::Constructor, &apiMsg);*/

    return S_OK;
}
CATCH_RETURN();

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(_In_ IUiaData* pData,
                                                 _In_ IRawElementProviderSimple* const pProvider,
                                                 _In_ const Cursor& cursor,
                                                 _In_ std::wstring_view wordDelimiters) noexcept
{
    RETURN_IF_FAILED(RuntimeClassInitialize(pData, pProvider, wordDelimiters));

    try
    {
        _start = cursor.GetPosition();
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
                                                 _In_ const COORD start,
                                                 _In_ const COORD end,
                                                 _In_ std::wstring_view wordDelimiters) noexcept
{
    RETURN_IF_FAILED(RuntimeClassInitialize(pData, pProvider, wordDelimiters));

    _start = start;
    _end = end;

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
    short row = 0;
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
        row = gsl::narrow<SHORT>(clientPoint.y / static_cast<LONG>(currentFontSize.Y)) + viewport.Top;
    }
    _start = { 0, row };
    _end = _start;
}

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT UiaTextRangeBase::RuntimeClassInitialize(const UiaTextRangeBase& a) noexcept
try
{
    _pProvider = a._pProvider;
    _start = a._start;
    _end = a._end;
    _pData = a._pData;
    _wordDelimiters = a._wordDelimiters;

    _id = id;
    ++id;

#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
    OutputDebugString(L"Copy Constructor\n");
    _outputObjectState();
#endif

    return S_OK;
}
CATCH_RETURN();

const IdType UiaTextRangeBase::GetId() const noexcept
{
    return _id;
}

const COORD UiaTextRangeBase::GetEndpoint(TextPatternRangeEndpoint endpoint) const noexcept
{
    switch (endpoint)
    {
    case TextPatternRangeEndpoint::TextPatternRangeEndpoint_End:
        return _end;
    case TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start:
    default:
        return _start;
    }
}

// Routine Description:
// - sets the target endpoint to the given COORD value
// - if the target endpoint crosses the other endpoint, become a degenerate range
// Arguments:
// - endpoint - the target endpoint (start or end)
// - val - the value that it will be set to
// Return Value:
// - true if range is degenerate, false otherwise.
bool UiaTextRangeBase::SetEndpoint(TextPatternRangeEndpoint endpoint, const COORD val)
{
    const auto bufferSize = _pData->GetTextBuffer().GetSize();
    switch (endpoint)
    {
    case TextPatternRangeEndpoint::TextPatternRangeEndpoint_End:
        _end = val;
        // if end is before start...
        if (bufferSize.CompareInBounds(_end, _start, true) < 0)
        {
            // make this range degenerate at end
            _start = _end;
        }
        break;
    case TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start:
        _start = val;
        // if start is after end...
        if (bufferSize.CompareInBounds(_start, _end, true) > 0)
        {
            // make this range degenerate at start
            _end = _start;
        }
        break;
    default:
        break;
    }
    return IsDegenerate();
}

// Routine Description:
// - returns true if the range is currently degenerate (empty range).
// Arguments:
// - <none>
// Return Value:
// - true if range is degenerate, false otherwise.
const bool UiaTextRangeBase::IsDegenerate() const noexcept
{
    return _start == _end;
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
        *pRetVal = (_start == other->GetEndpoint(TextPatternRangeEndpoint_Start) &&
                    _end == other->GetEndpoint(TextPatternRangeEndpoint_End));
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
try
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
    const auto other = range->GetEndpoint(targetEndpoint);

    // get the values of our endpoint
    const auto mine = GetEndpoint(endpoint);

    // compare them
    *pRetVal = _pData->GetTextBuffer().GetSize().CompareInBounds(mine, other, true);

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
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::ExpandToEnclosingUnit(_In_ TextUnit unit) noexcept
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
        const auto& buffer = _pData->GetTextBuffer();
        const auto bufferSize = buffer.GetSize();
        const auto bufferEnd = bufferSize.EndExclusive();

        if (unit == TextUnit::TextUnit_Character)
        {
            _end = _start;
            bufferSize.IncrementInBounds(_end, true);
        }
        else if (unit <= TextUnit::TextUnit_Word)
        {
            // expand to word
            _start = buffer.GetWordStart(_start, _wordDelimiters, true);
            _end = buffer.GetWordEnd(_start, _wordDelimiters, true);
        }
        else if (unit <= TextUnit::TextUnit_Line)
        {
            // expand to line
            _start.X = 0;
            _end.X = 0;
            _end.Y = base::ClampAdd(_start.Y, 1);
        }
        else
        {
            // expand to document
            _start = bufferSize.Origin();
            _end = bufferSize.EndExclusive();
        }

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

IFACEMETHODIMP UiaTextRangeBase::FindText(_In_ BSTR text,
                                          _In_ BOOL searchBackward,
                                          _In_ BOOL ignoreCase,
                                          _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal) noexcept
try
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::FindText, nullptr);
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;

    const std::wstring queryText{ text, SysStringLen(text) };
    const auto bufferSize = _pData->GetTextBuffer().GetSize();
    const auto sensitivity = ignoreCase ? Search::Sensitivity::CaseInsensitive : Search::Sensitivity::CaseSensitive;

    auto searchDirection = Search::Direction::Forward;
    auto searchAnchor = _start;
    if (searchBackward)
    {
        searchDirection = Search::Direction::Backward;

        // we need to convert the end to inclusive
        // because Search operates with an inclusive COORD
        searchAnchor = _end;
        bufferSize.DecrementInBounds(searchAnchor, true);
    }

    Search searcher{ *_pData, queryText, searchDirection, sensitivity, searchAnchor };

    if (searcher.FindNext())
    {
        const auto foundLocation = searcher.GetFoundLocation();
        const auto start = foundLocation.first;

        // we need to increment the position of end because it's exclusive
        auto end = foundLocation.second;
        bufferSize.IncrementInBounds(end, true);

        // make sure what was found is within the bounds of the current range
        if ((searchDirection == Search::Direction::Forward && bufferSize.CompareInBounds(end, _end, true) < 0) ||
            (searchDirection == Search::Direction::Backward && bufferSize.CompareInBounds(start, _start) > 0))
        {
            RETURN_IF_FAILED(Clone(ppRetVal));
            UiaTextRangeBase& range = static_cast<UiaTextRangeBase&>(**ppRetVal);
            range._start = start;
            range._end = end;
        }
    }
    return S_OK;
}
CATCH_RETURN();

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

IFACEMETHODIMP UiaTextRangeBase::GetBoundingRectangles(_Outptr_result_maybenull_ SAFEARRAY** ppRetVal) noexcept
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

        const auto bufferSize = _pData->GetTextBuffer().GetSize();

        // these viewport vars are converted to the buffer coordinate space
        const auto viewport = bufferSize.ConvertToOrigin(_pData->GetViewport());
        const auto viewportOrigin = viewport.Origin();
        const auto viewportEnd = viewport.EndExclusive();

        // startAnchor: the earliest COORD we will get a bounding rect for
        auto startAnchor = GetEndpoint(TextPatternRangeEndpoint_Start);
        if (bufferSize.CompareInBounds(startAnchor, viewportOrigin, true) < 0)
        {
            // earliest we can be is the origin
            startAnchor = viewportOrigin;
        }

        // endAnchor: the latest COORD we will get a bounding rect for
        auto endAnchor = GetEndpoint(TextPatternRangeEndpoint_End);
        if (bufferSize.CompareInBounds(endAnchor, viewportEnd, true) > 0)
        {
            // latest we can be is the viewport end
            endAnchor = viewportEnd;
        }

        // _end is exclusive, let's be inclusive so we don't have to think about it anymore for bounding rects
        bufferSize.DecrementInBounds(endAnchor, true);

        if (IsDegenerate() || bufferSize.CompareInBounds(_start, viewportEnd, true) > 0 || bufferSize.CompareInBounds(_end, viewportOrigin, true) < 0)
        {
            // An empty array is returned for a degenerate (empty) text range.
            // reference: https://docs.microsoft.com/en-us/windows/win32/api/uiautomationclient/nf-uiautomationclient-iuiautomationtextrange-getboundingrectangles

            // Remember, start cannot be past end, so
            //   if start is past the viewport end,
            //   or end is past the viewport origin
            //   draw nothing
        }
        else
        {
            for (auto row = startAnchor.Y; row <= endAnchor.Y; ++row)
            {
                // assume that we are going to draw the entire row
                COORD startCoord = { 0, row };
                COORD endCoord = { viewport.RightInclusive(), row };

                if (row == startAnchor.Y)
                {
                    // first row --> reduce left side
                    startCoord.X = startAnchor.X;
                }

                if (row == endAnchor.Y)
                {
                    // last row --> reduce right side
                    endCoord.X = endAnchor.X;
                }

                _getBoundingRect(startCoord, endCoord, coords);
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

IFACEMETHODIMP UiaTextRangeBase::GetEnclosingElement(_Outptr_result_maybenull_ IRawElementProviderSimple** ppRetVal) noexcept
try
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;

    //Tracing::s_TraceUia(this, ApiCall::GetBoundingRectangles, nullptr);
#pragma warning(suppress : 26447) // QueryInterface's exception should be caught by the try-CATCH_RETURN block to allow this to be noexcept
    return _pProvider->QueryInterface(IID_PPV_ARGS(ppRetVal));
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::GetText(_In_ int maxLength, _Out_ BSTR* pRetVal) noexcept
try
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

    if (!IsDegenerate())
    {
#if defined(_DEBUG) && defined(UIATEXTRANGE_DEBUG_MSGS)
        std::wstringstream ss;
        ss << L"---Initial span start={" << _start.X << L", " << _start.Y << L"} and end={" << _end.X << ", " << _end.Y << L"}\n";
        OutputDebugString(ss.str().c_str());
#endif

        // if _end is at 0, we ignore that row because _end is exclusive
        const auto& buffer = _pData->GetTextBuffer();
        const short totalRowsInRange = (_end.X == buffer.GetSize().Left()) ?
                                           base::ClampSub(_end.Y, _start.Y) :
                                           base::ClampAdd(base::ClampSub(_end.Y, _start.Y), base::ClampedNumeric<short>(1));
        const short lastRowInRange = _start.Y + totalRowsInRange - 1;

        short currentScreenInfoRow = 0;
        for (short i = 0; i < totalRowsInRange; ++i)
        {
            currentScreenInfoRow = _start.Y + i;
            const ROW& row = buffer.GetRowByOffset(currentScreenInfoRow);
            if (row.GetCharRow().ContainsText())
            {
                const size_t rowRight = row.GetCharRow().MeasureRight();
                size_t startIndex = 0;
                size_t endIndex = rowRight;
                if (currentScreenInfoRow == _start.Y)
                {
                    startIndex = _start.X;
                }

                if (currentScreenInfoRow == _end.Y)
                {
                    // prevent the end from going past the last non-whitespace char in the row
                    endIndex = std::max<size_t>(startIndex + 1, std::min(gsl::narrow_cast<size_t>(_end.X), rowRight));
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

            if (currentScreenInfoRow != lastRowInRange)
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
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::Move(_In_ TextUnit unit,
                                      _In_ int count,
                                      _Out_ int* pRetVal) noexcept
{
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
    ss << L" unit: " << unit;
    ss << L" count: " << count;
    std::wstring data = ss.str();
    OutputDebugString(data.c_str());
    OutputDebugString(L"\n");
#endif

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    // We can abstract this movement by moving _start, but disallowing moving to the end of the buffer
    constexpr auto endpoint = TextPatternRangeEndpoint::TextPatternRangeEndpoint_Start;
    constexpr auto preventBufferEnd = true;
    try
    {
        if (unit == TextUnit::TextUnit_Character)
        {
            _moveEndpointByUnitCharacter(count, endpoint, pRetVal, preventBufferEnd);
        }
        else if (unit <= TextUnit::TextUnit_Word)
        {
            _moveEndpointByUnitWord(count, endpoint, pRetVal, preventBufferEnd);
        }
        else if (unit <= TextUnit::TextUnit_Line)
        {
            _moveEndpointByUnitLine(count, endpoint, pRetVal, preventBufferEnd);
        }
        else if (unit <= TextUnit::TextUnit_Document)
        {
            _moveEndpointByUnitDocument(count, endpoint, pRetVal, preventBufferEnd);
        }
    }
    CATCH_RETURN();

    // If we actually moved...
    if (*pRetVal != 0)
    {
        // then just expand to get our _end
        ExpandToEnclosingUnit(unit);
    }

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*apiMsg.MovedCount = *pRetVal;
    Tracing::s_TraceUia(this, ApiCall::Move, &apiMsg);*/

    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::MoveEndpointByUnit(_In_ TextPatternRangeEndpoint endpoint,
                                                    _In_ TextUnit unit,
                                                    _In_ int count,
                                                    _Out_ int* pRetVal) noexcept
{
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
#endif

    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    try
    {
        if (unit == TextUnit::TextUnit_Character)
        {
            _moveEndpointByUnitCharacter(count, endpoint, pRetVal);
        }
        else if (unit <= TextUnit::TextUnit_Word)
        {
            _moveEndpointByUnitWord(count, endpoint, pRetVal);
        }
        else if (unit <= TextUnit::TextUnit_Line)
        {
            _moveEndpointByUnitLine(count, endpoint, pRetVal);
        }
        else if (unit <= TextUnit::TextUnit_Document)
        {
            _moveEndpointByUnitDocument(count, endpoint, pRetVal);
        }
    }
    CATCH_RETURN();

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*apiMsg.MovedCount = *pRetVal;
    Tracing::s_TraceUia(this, ApiCall::MoveEndpointByUnit, &apiMsg);*/

    return S_OK;
}

IFACEMETHODIMP UiaTextRangeBase::MoveEndpointByRange(_In_ TextPatternRangeEndpoint endpoint,
                                                     _In_ ITextRangeProvider* pTargetRange,
                                                     _In_ TextPatternRangeEndpoint targetEndpoint) noexcept
try
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
#endif

    SetEndpoint(endpoint, range->GetEndpoint(targetEndpoint));

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::MoveEndpointByRange, &apiMsg);
    return S_OK;
}
CATCH_RETURN();

IFACEMETHODIMP UiaTextRangeBase::Select() noexcept
try
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    if (IsDegenerate())
    {
        // calling Select on a degenerate range should clear any current selections
        _pData->ClearSelection();
    }
    else
    {
        auto temp = _end;
        _pData->GetTextBuffer().GetSize().DecrementInBounds(temp);
        _pData->SelectNewRegion(_start, temp);
    }

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::Select, nullptr);
    return S_OK;
}
CATCH_RETURN();

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

IFACEMETHODIMP UiaTextRangeBase::ScrollIntoView(_In_ BOOL alignToTop) noexcept
try
{
    _pData->LockConsole();
    auto Unlock = wil::scope_exit([&]() noexcept {
        _pData->UnlockConsole();
    });

    const auto oldViewport = _pData->GetViewport().ToInclusive();
    const auto viewportHeight = _getViewportHeight(oldViewport);
    // range rows
    const base::ClampedNumeric<short> startScreenInfoRow = _start.Y;
    const base::ClampedNumeric<short> endScreenInfoRow = _end.Y;
    // screen buffer rows
    const base::ClampedNumeric<short> topRow = 0;
    const base::ClampedNumeric<short> bottomRow = _pData->GetTextBuffer().TotalRowCount() - 1;

    SMALL_RECT newViewport = oldViewport;

    // there's a bunch of +1/-1s here for setting the viewport. These
    // are to account for the inclusivity of the viewport boundaries.
    if (alignToTop)
    {
        // determine if we can align the start row to the top
        if (startScreenInfoRow + viewportHeight <= bottomRow)
        {
            // we can align to the top
            newViewport.Top = startScreenInfoRow;
            newViewport.Bottom = startScreenInfoRow + viewportHeight - 1;
        }
        else
        {
            // we can align to the top so we'll just move the viewport
            // to the bottom of the screen buffer
            newViewport.Bottom = bottomRow;
            newViewport.Top = bottomRow - viewportHeight + 1;
        }
    }
    else
    {
        // we need to align to the bottom
        // check if we can align to the bottom
        if (static_cast<unsigned int>(endScreenInfoRow) >= viewportHeight)
        {
            // we can align to bottom
            newViewport.Bottom = endScreenInfoRow;
            newViewport.Top = endScreenInfoRow - viewportHeight + 1;
        }
        else
        {
            // we can't align to bottom so we'll move the viewport to
            // the top of the screen buffer
            newViewport.Top = topRow;
            newViewport.Bottom = topRow + viewportHeight - 1;
        }
    }

    FAIL_FAST_IF(!(newViewport.Top >= topRow));
    FAIL_FAST_IF(!(newViewport.Bottom <= bottomRow));
    FAIL_FAST_IF(!(_getViewportHeight(oldViewport) == _getViewportHeight(newViewport)));

    _ChangeViewport(newViewport);

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgScrollIntoView apiMsg;
        apiMsg.AlignToTop = !!alignToTop;
        Tracing::s_TraceUia(this, ApiCall::ScrollIntoView, &apiMsg);*/

    return S_OK;
}
CATCH_RETURN();

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

const COORD UiaTextRangeBase::_getScreenFontSize() const
{
    COORD coordRet = _pData->GetFontInfo().GetSize();

    // For sanity's sake, make sure not to leak 0 out as a possible value. These values are used in division operations.
    coordRet.X = std::max(coordRet.X, 1i16);
    coordRet.Y = std::max(coordRet.Y, 1i16);

    return coordRet;
}

// Routine Description:
// - Gets the viewport height, measured in char rows.
// Arguments:
// - viewport - The viewport to measure
// Return Value:
// - The viewport height
const unsigned int UiaTextRangeBase::_getViewportHeight(const SMALL_RECT viewport) const noexcept
{
    FAIL_FAST_IF(!(viewport.Bottom >= viewport.Top));
    // + 1 because COORD is inclusive on both sides so subtracting top
    // and bottom gets rid of 1 more then it should.
    return viewport.Bottom - viewport.Top + 1;
}

// Routine Description:
// - adds the relevant coordinate points from the row to coords.
// - it is assumed that startAnchor and endAnchor are within the same row
//    and NOT DEGENERATE
// Arguments:
// - startAnchor - the start anchor of interested data within the viewport. In text buffer coordinate space. Inclusive.
// - endAnchor - the end anchor of interested data within the viewport. In text buffer coordinate space. Inclusive
// - coords - vector to add the calculated coords to
// Return Value:
// - <none>
void UiaTextRangeBase::_getBoundingRect(_In_ const COORD startAnchor, _In_ const COORD endAnchor, _Inout_ std::vector<double>& coords) const
{
    FAIL_FAST_IF(startAnchor.Y != endAnchor.Y);
    FAIL_FAST_IF(startAnchor.X > endAnchor.X);

    const auto viewport = _pData->GetViewport();
    const auto currentFontSize = _getScreenFontSize();

    POINT topLeft{ 0 };
    POINT bottomRight{ 0 };

    // startAnchor is converted to the viewport coordinate space
#pragma warning(suppress : 26496) // analysis can't see this, TODO GH: 4015 to improve Viewport to be less bad because it'd go away if ConvertToOrigin returned instead of inout'd.
    auto startCoord = startAnchor;
    viewport.ConvertToOrigin(&startCoord);

    // we want to clamp to a long (output type), not a short (input type)
    // so we need to explicitly say <long,long>
    topLeft.x = base::ClampMul<long, long>(startCoord.X, currentFontSize.X);
    topLeft.y = base::ClampMul<long, long>(startCoord.Y, currentFontSize.Y);

    // endAnchor is converted to the viewport coordinate space
#pragma warning(suppress : 26496) // analysis can't see this, TODO GH: 4015 to improve Viewport to be less bad because it'd go away if ConvertToOrigin returned instead of inout'd.
    auto endCoord = endAnchor;
    viewport.ConvertToOrigin(&endCoord);
    bottomRight.x = base::ClampMul<long, long>(base::ClampAdd(endCoord.X, 1), currentFontSize.X);
    bottomRight.y = base::ClampMul<long, long>(base::ClampAdd(endCoord.Y, 1), currentFontSize.Y);

    // convert the coords to be relative to the screen instead of
    // the client window
    _TranslatePointToScreen(&topLeft);
    _TranslatePointToScreen(&bottomRight);

    const long width = base::ClampSub(bottomRight.x, topLeft.x);
    const long height = base::ClampSub(bottomRight.y, topLeft.y);

    // insert the coords
    coords.push_back(topLeft.x);
    coords.push_back(topLeft.y);
    coords.push_back(width);
    coords.push_back(height);
}

// Routine Description:
// - moves the UTR's endpoint by moveCount times by character.
// - if endpoints crossed, the degenerate range is created and both endpoints are moved
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - pAmountMoved - the number of times that the return values are "moved"
// - preventBufferEnd - when enabled, prevent endpoint from being at the end of the buffer
//                      This is used for general movement, where you are not allowed to
//                      create a degenerate range
// Return Value:
// - <none>
void UiaTextRangeBase::_moveEndpointByUnitCharacter(_In_ const int moveCount,
                                                    _In_ const TextPatternRangeEndpoint endpoint,
                                                    _Out_ gsl::not_null<int*> const pAmountMoved,
                                                    _In_ const bool preventBufferEnd)
{
    *pAmountMoved = 0;

    if (moveCount == 0)
    {
        return;
    }

    const bool allowBottomExclusive = !preventBufferEnd;
    const MovementDirection moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    const auto bufferSize = _pData->GetTextBuffer().GetSize();

    bool success = true;
    auto target = GetEndpoint(endpoint);
    while (abs(*pAmountMoved) < abs(moveCount) && success)
    {
        switch (moveDirection)
        {
        case MovementDirection::Forward:
            success = bufferSize.IncrementInBounds(target, allowBottomExclusive);
            if (success)
            {
                (*pAmountMoved)++;
            }
            break;
        case MovementDirection::Backward:
            success = bufferSize.DecrementInBounds(target, allowBottomExclusive);
            if (success)
            {
                (*pAmountMoved)--;
            }
            break;
        default:
            break;
        }
    }

    SetEndpoint(endpoint, target);
}

// Routine Description:
// - moves the UTR's endpoint by moveCount times by word.
// - if endpoints crossed, the degenerate range is created and both endpoints are moved
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - pAmountMoved - the number of times that the return values are "moved"
// - preventBufferEnd - when enabled, prevent endpoint from being at the end of the buffer
//                      This is used for general movement, where you are not allowed to
//                      create a degenerate range
// Return Value:
// - <none>
void UiaTextRangeBase::_moveEndpointByUnitWord(_In_ const int moveCount,
                                               _In_ const TextPatternRangeEndpoint endpoint,
                                               _Out_ gsl::not_null<int*> const pAmountMoved,
                                               _In_ const bool preventBufferEnd)
{
    *pAmountMoved = 0;

    if (moveCount == 0)
    {
        return;
    }

    const bool allowBottomExclusive = !preventBufferEnd;
    const MovementDirection moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    const auto& buffer = _pData->GetTextBuffer();
    const auto bufferSize = buffer.GetSize();
    const auto bufferOrigin = bufferSize.Origin();
    const auto bufferEnd = bufferSize.EndExclusive();
    const auto lastCharPos = buffer.GetLastNonSpaceCharacter();

    auto resultPos = GetEndpoint(endpoint);
    auto nextPos = resultPos;

    bool success = true;
    while (std::abs(*pAmountMoved) < std::abs(moveCount) && success)
    {
        nextPos = resultPos;
        switch (moveDirection)
        {
        case MovementDirection::Forward:
        {
            if (nextPos == bufferEnd)
            {
                success = false;
            }
            else if (buffer.MoveToNextWord(nextPos, _wordDelimiters, lastCharPos))
            {
                resultPos = nextPos;
                (*pAmountMoved)++;
            }
            else if (allowBottomExclusive)
            {
                resultPos = bufferEnd;
                (*pAmountMoved)++;
            }
            break;
        }
        case MovementDirection::Backward:
        {
            if (nextPos == bufferOrigin)
            {
                success = false;
            }
            else if (buffer.MoveToPreviousWord(nextPos, _wordDelimiters))
            {
                resultPos = nextPos;
                (*pAmountMoved)--;
            }
            else
            {
                resultPos = bufferOrigin;
            }
            break;
        }
        default:
            return;
        }
    }

    SetEndpoint(endpoint, resultPos);
}

// Routine Description:
// - moves the UTR's endpoint by moveCount times by line.
// - if endpoints crossed, the degenerate range is created and both endpoints are moved
// - a successful movement on start entails start being at Left()
// - a successful movement on end entails end being at Left() of the NEXT line
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - pAmountMoved - the number of times that the return values are "moved"
// - preventBufferEnd - when enabled, prevent endpoint from being at the end of the buffer
//                      This is used for general movement, where you are not allowed to
//                      create a degenerate range
// Return Value:
// - <none>
void UiaTextRangeBase::_moveEndpointByUnitLine(_In_ const int moveCount,
                                               _In_ const TextPatternRangeEndpoint endpoint,
                                               _Out_ gsl::not_null<int*> const pAmountMoved,
                                               _In_ const bool preventBufferEnd)
{
    *pAmountMoved = 0;

    if (moveCount == 0)
    {
        return;
    }

    const bool allowBottomExclusive = !preventBufferEnd;
    const MovementDirection moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    const auto bufferSize = _pData->GetTextBuffer().GetSize();

    bool success = true;
    auto resultPos = GetEndpoint(endpoint);
    while (abs(*pAmountMoved) < abs(moveCount) && success)
    {
        auto nextPos = resultPos;
        switch (moveDirection)
        {
        case MovementDirection::Forward:
        {
            // can't move past end
            if (nextPos.Y >= bufferSize.BottomInclusive())
            {
                if (preventBufferEnd || nextPos == bufferSize.EndExclusive())
                {
                    success = false;
                    break;
                }
            }

            nextPos.X = bufferSize.RightInclusive();
            success = bufferSize.IncrementInBounds(nextPos, allowBottomExclusive);
            if (success)
            {
                resultPos = nextPos;
                (*pAmountMoved)++;
            }
            break;
        }
        case MovementDirection::Backward:
        {
            // can't move past top
            if (!allowBottomExclusive && nextPos.Y == bufferSize.Top())
            {
                success = false;
                break;
            }

            // NOTE: Automatically detects if we are trying to move past origin
            success = bufferSize.DecrementInBounds(nextPos, allowBottomExclusive);

            if (success)
            {
                nextPos.X = bufferSize.Left();
                resultPos = nextPos;
                (*pAmountMoved)--;
            }
            break;
        }
        default:
            break;
        }
    }

    SetEndpoint(endpoint, resultPos);
}

// Routine Description:
// - moves the UTR's endpoint by moveCount times by document.
// - if endpoints crossed, the degenerate range is created and both endpoints are moved
// Arguments:
// - moveCount - the number of times to move
// - endpoint - the endpoint to move
// - pAmountMoved - the number of times that the return values are "moved"
// - preventBufferEnd - when enabled, prevent endpoint from being at the end of the buffer
//                      This is used for general movement, where you are not allowed to
//                      create a degenerate range
// Return Value:
// - <none>
void UiaTextRangeBase::_moveEndpointByUnitDocument(_In_ const int moveCount,
                                                   _In_ const TextPatternRangeEndpoint endpoint,
                                                   _Out_ gsl::not_null<int*> const pAmountMoved,
                                                   _In_ const bool preventBufferEnd)
{
    *pAmountMoved = 0;

    if (moveCount == 0)
    {
        return;
    }

    const MovementDirection moveDirection = (moveCount > 0) ? MovementDirection::Forward : MovementDirection::Backward;
    const auto bufferSize = _pData->GetTextBuffer().GetSize();

    const auto target = GetEndpoint(endpoint);
    switch (moveDirection)
    {
    case MovementDirection::Forward:
    {
        const auto documentEnd = bufferSize.EndExclusive();
        if (preventBufferEnd || target == documentEnd)
        {
            return;
        }
        else
        {
            SetEndpoint(endpoint, documentEnd);
            (*pAmountMoved)++;
        }
        break;
    }
    case MovementDirection::Backward:
    {
        const auto documentBegin = bufferSize.Origin();
        if (target == documentBegin)
        {
            return;
        }
        else
        {
            SetEndpoint(endpoint, documentBegin);
            (*pAmountMoved)--;
        }
        break;
    }
    default:
        break;
    }
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
