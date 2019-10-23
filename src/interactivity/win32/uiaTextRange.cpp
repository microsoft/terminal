// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "uiaTextRange.hpp"
#include "screenInfoUiaProvider.hpp"
#include "..\host\search.h"
#include "..\interactivity\inc\ServiceLocator.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;
using Microsoft::Console::Interactivity::ServiceLocator;

std::deque<UiaTextRange*> UiaTextRange::GetSelectionRanges(_In_ IUiaData* pData,
                                                           _In_ IRawElementProviderSimple* pProvider)
{
    std::deque<UiaTextRange*> ranges;

    // get the selection rects
    const auto rectangles = pData->GetSelectionRects();

    // create a range for each row
    for (const auto& rect : rectangles)
    {
        ScreenInfoRow currentRow = rect.Top();
        Endpoint start = _screenInfoRowToEndpoint(pData, currentRow) + rect.Left();
        Endpoint end = _screenInfoRowToEndpoint(pData, currentRow) + rect.RightInclusive();
        UiaTextRange* range = UiaTextRange::Create(pData,
                                                   pProvider,
                                                   start,
                                                   end,
                                                   false);
        if (range == nullptr)
        {
            // something when wrong, clean up and throw
            while (!ranges.empty())
            {
                UiaTextRangeBase* temp = ranges[0];
                ranges.pop_front();
                temp->Release();
            }
            THROW_HR(E_INVALIDARG);
        }
        else
        {
            ranges.push_back(range);
        }
    }
    return ranges;
}

UiaTextRange* UiaTextRange::Create(_In_ IUiaData* pData,
                                   _In_ IRawElementProviderSimple* const pProvider)
{
    UiaTextRange* range = nullptr;
    try
    {
        range = WRL::Make<UiaTextRange>(pData, pProvider).Detach();
    }
    catch (...)
    {
        range = nullptr;
    }

    if (range)
    {
        pProvider->AddRef();
    }
    return range;
}

UiaTextRange* UiaTextRange::Create(_In_ IUiaData* pData,
                                   _In_ IRawElementProviderSimple* const pProvider,
                                   const Cursor& cursor)
{
    UiaTextRange* range = nullptr;
    try
    {
        range = WRL::Make<UiaTextRange>(pData, pProvider, cursor).Detach();
    }
    catch (...)
    {
        range = nullptr;
    }

    if (range)
    {
        pProvider->AddRef();
    }
    return range;
}

UiaTextRange* UiaTextRange::Create(_In_ IUiaData* pData,
                                   _In_ IRawElementProviderSimple* const pProvider,
                                   const Endpoint start,
                                   const Endpoint end,
                                   const bool degenerate)
{
    UiaTextRange* range = nullptr;
    try
    {
        range = WRL::Make<UiaTextRange>(pData,
                                        pProvider,
                                        start,
                                        end,
                                        degenerate)
                    .Detach();
    }
    catch (...)
    {
        range = nullptr;
    }

    if (range)
    {
        pProvider->AddRef();
    }
    return range;
}

UiaTextRange* UiaTextRange::Create(_In_ IUiaData* pData,
                                   _In_ IRawElementProviderSimple* const pProvider,
                                   const UiaPoint point)
{
    UiaTextRange* range = nullptr;
    try
    {
        range = WRL::Make<UiaTextRange>(pData, pProvider, point).Detach();
    }
    catch (...)
    {
        range = nullptr;
    }

    if (range)
    {
        pProvider->AddRef();
    }
    return range;
}

// degenerate range constructor.
UiaTextRange::UiaTextRange(_In_ IUiaData* pData, _In_ IRawElementProviderSimple* const pProvider) :
    UiaTextRangeBase(pData, pProvider)
{
}

UiaTextRange::UiaTextRange(_In_ IUiaData* pData,
                           _In_ IRawElementProviderSimple* const pProvider,
                           const Cursor& cursor) :
    UiaTextRangeBase(pData, pProvider, cursor)
{
}

UiaTextRange::UiaTextRange(_In_ IUiaData* pData,
                           _In_ IRawElementProviderSimple* const pProvider,
                           const Endpoint start,
                           const Endpoint end,
                           const bool degenerate) :
    UiaTextRangeBase(pData, pProvider, start, end, degenerate)
{
}

// returns a degenerate text range of the start of the row closest to the y value of point
UiaTextRange::UiaTextRange(_In_ IUiaData* pData,
                           _In_ IRawElementProviderSimple* const pProvider,
                           const UiaPoint point) :
    UiaTextRangeBase(pData, pProvider)
{
    Initialize(point);
}

IFACEMETHODIMP UiaTextRange::Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    RETURN_HR_IF(E_INVALIDARG, ppRetVal == nullptr);
    *ppRetVal = nullptr;
    try
    {
        *ppRetVal = WRL::Make<UiaTextRange>(*this).Detach();
    }
    catch (...)
    {
        *ppRetVal = nullptr;
        return wil::ResultFromCaughtException();
    }
    if (*ppRetVal == nullptr)
    {
        return E_OUTOFMEMORY;
    }

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
        Endpoint searchAnchor = _start;
        if (searchBackward)
        {
            searchDirection = Search::Direction::Backward;
            searchAnchor = _end;
        }

        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        THROW_HR_IF(E_POINTER, !gci.HasActiveOutputBuffer());
        const auto& screenInfo = gci.GetActiveOutputBuffer().GetActiveBuffer();
        Search searcher{ screenInfo, wstr, searchDirection, sensitivity, _endpointToCoord(_pData, searchAnchor) };

        HRESULT hr = S_OK;
        if (searcher.FindNext())
        {
            const auto foundLocation = searcher.GetFoundLocation();
            const Endpoint start = _coordToEndpoint(_pData, foundLocation.first);
            const Endpoint end = _coordToEndpoint(_pData, foundLocation.second);
            // make sure what was found is within the bounds of the current range
            if ((searchDirection == Search::Direction::Forward && end < _end) ||
                (searchDirection == Search::Direction::Backward && start > _start))
            {
                hr = Clone(ppRetVal);
                if (SUCCEEDED(hr))
                {
                    UiaTextRange& range = static_cast<UiaTextRange&>(**ppRetVal);
                    range._start = start;
                    range._end = end;
                    range._degenerate = false;
                }
            }
        }
        return hr;
    }
    CATCH_RETURN();
}

void UiaTextRange::_ChangeViewport(const SMALL_RECT NewWindow)
{
    auto provider = static_cast<ScreenInfoUiaProvider*>(_pProvider.get());
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
    const auto provider = static_cast<ScreenInfoUiaProvider*>(_pProvider.get());
    return provider->GetWindowHandle();
}
