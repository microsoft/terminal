// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "uiaTextRange.hpp"
#include "screenInfoUiaProvider.hpp"
#include "..\buffer\out\search.h"
#include "..\interactivity\inc\ServiceLocator.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::WRL;
using Microsoft::Console::Interactivity::ServiceLocator;

HRESULT UiaTextRange::GetSelectionRanges(_In_ IUiaData* pData,
                                         _In_ IRawElementProviderSimple* pProvider,
                                         const std::wstring_view wordDelimiters,
                                         _Out_ std::deque<ComPtr<UiaTextRange>>& ranges)
{
    try
    {
        typename std::remove_reference<decltype(ranges)>::type temporaryResult;

        // get the selection rects
        const auto rectangles = pData->GetSelectionRects();

        // create a range for each row
        for (const auto& rect : rectangles)
        {
            const auto start = rect.Origin();
            const auto end = rect.EndInclusive();

            ComPtr<UiaTextRange> range;
            RETURN_IF_FAILED(MakeAndInitialize<UiaTextRange>(&range, pData, pProvider, start, end, wordDelimiters));
            temporaryResult.emplace_back(std::move(range));
        }
        std::swap(temporaryResult, ranges);
        return S_OK;
    }
    CATCH_RETURN();
}

// degenerate range constructor.
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData, _In_ IRawElementProviderSimple* const pProvider, _In_ const std::wstring_view wordDelimiters)
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, wordDelimiters);
}

// degenerate range at cursor position
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData,
                                             _In_ IRawElementProviderSimple* const pProvider,
                                             const Cursor& cursor,
                                             const std::wstring_view wordDelimiters)
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, cursor, wordDelimiters);
}

// specific endpoint range
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData,
                                             _In_ IRawElementProviderSimple* const pProvider,
                                             const COORD start,
                                             const COORD end,
                                             const std::wstring_view wordDelimiters)
{
    return UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, start, end, wordDelimiters);
}

// returns a degenerate text range of the start of the row closest to the y value of point
HRESULT UiaTextRange::RuntimeClassInitialize(_In_ IUiaData* pData,
                                             _In_ IRawElementProviderSimple* const pProvider,
                                             const UiaPoint point,
                                             const std::wstring_view wordDelimiters)
{
    RETURN_IF_FAILED(UiaTextRangeBase::RuntimeClassInitialize(pData, pProvider, wordDelimiters));
    Initialize(point);
    return S_OK;
}

HRESULT UiaTextRange::RuntimeClassInitialize(const UiaTextRange& a)
{
    return UiaTextRangeBase::RuntimeClassInitialize(a);
}

IFACEMETHODIMP UiaTextRange::Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<UiaTextRange>(ppRetVal, *this));

#if defined(_DEBUG) && defined(UiaTextRangeBase_DEBUG_MSGS)
    OutputDebugString(L"Clone\n");
    std::wstringstream ss;
    ss << _id << L" cloned to " << (static_cast<UiaTextRangeBase*>(*ppRetVal))->_id;
    std::wstring str = ss.str();
    OutputDebugString(str.c_str());
    OutputDebugString(L"\n");
#endif
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    // tracing
    /*ApiMsgClone apiMsg;
    apiMsg.CloneId = static_cast<UiaTextRangeBase*>(*ppRetVal)->GetId();
    Tracing::s_TraceUia(this, ApiCall::Clone, &apiMsg);*/

    return S_OK;
}

IFACEMETHODIMP UiaTextRange::FindText(_In_ BSTR text,
                                      _In_ BOOL searchBackward,
                                      _In_ BOOL ignoreCase,
                                      _Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::FindText, nullptr);
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;
    try
    {
        const std::wstring wstr{ text, SysStringLen(text) };
        const auto sensitivity = ignoreCase ? Search::Sensitivity::CaseInsensitive : Search::Sensitivity::CaseSensitive;

        auto searchDirection = Search::Direction::Forward;
        auto searchAnchor = _start;
        if (searchBackward)
        {
            searchDirection = Search::Direction::Backward;
            searchAnchor = _end;
        }

        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        THROW_HR_IF(E_POINTER, !gci.HasActiveOutputBuffer());
        Search searcher{ gci.renderData, wstr, searchDirection, sensitivity, searchAnchor };

        HRESULT hr = S_OK;
        if (searcher.FindNext())
        {
            const auto foundLocation = searcher.GetFoundLocation();
            const auto start = foundLocation.first;
            const auto end = foundLocation.second;
            const auto bufferSize = _pData->GetTextBuffer().GetSize();
            
            // make sure what was found is within the bounds of the current range
            if ((searchDirection == Search::Direction::Forward && bufferSize.CompareInBounds(end, _end) < 0) ||
                (searchDirection == Search::Direction::Backward && bufferSize.CompareInBounds(start, _start) > 0))
            {
                hr = Clone(ppRetVal);
                if (SUCCEEDED(hr))
                {
                    UiaTextRange& range = static_cast<UiaTextRange&>(**ppRetVal);
                    range._start = start;
                    range._end = end;
                }
            }
        }
        return hr;
    }
    CATCH_RETURN();
}

void UiaTextRange::_ChangeViewport(const SMALL_RECT NewWindow)
{
    auto provider = static_cast<ScreenInfoUiaProvider*>(_pProvider);
    provider->ChangeViewport(NewWindow);
}

void UiaTextRange::_TranslatePointToScreen(LPPOINT clientPoint) const
{
    ClientToScreen(_getWindowHandle(), clientPoint);
}

void UiaTextRange::_TranslatePointFromScreen(LPPOINT screenPoint) const
{
    ScreenToClient(_getWindowHandle(), screenPoint);
}

HWND UiaTextRange::_getWindowHandle() const
{
    const auto provider = static_cast<ScreenInfoUiaProvider*>(_pProvider);
    return provider->GetWindowHandle();
}
