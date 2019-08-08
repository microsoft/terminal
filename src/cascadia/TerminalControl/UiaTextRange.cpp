// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "UiaTextRange.hpp"
#include "screenInfoUiaProvider.hpp"

using namespace Microsoft::Terminal;

std::deque<UiaTextRange*> UiaTextRange::GetSelectionRanges(_In_ Microsoft::Console::Render::IRenderData* pData,
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
            throw E_INVALIDARG;
        }
        else
        {
            ranges.push_back(range);
        }
    }
    return ranges;
}

UiaTextRange* UiaTextRange::Create(_In_ Microsoft::Console::Render::IRenderData* pData,
                                   _In_ IRawElementProviderSimple* const pProvider)
{
    UiaTextRange* range = nullptr;
    ;
    try
    {
        range = new UiaTextRange(pData, pProvider);
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

UiaTextRange* UiaTextRange::Create(_In_ Microsoft::Console::Render::IRenderData* pData,
                                   _In_ IRawElementProviderSimple* const pProvider,
                                   const Cursor& cursor)
{
    UiaTextRange* range = nullptr;
    try
    {
        range = new UiaTextRange(pData, pProvider, cursor);
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

UiaTextRange* UiaTextRange::Create(_In_ Microsoft::Console::Render::IRenderData* pData,
                                   _In_ IRawElementProviderSimple* const pProvider,
                                   const Endpoint start,
                                   const Endpoint end,
                                   const bool degenerate)
{
    UiaTextRange* range = nullptr;
    try
    {
        range = new UiaTextRange(pData,
                                 pProvider,
                                 start,
                                 end,
                                 degenerate);
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

UiaTextRange* UiaTextRange::Create(_In_ Microsoft::Console::Render::IRenderData* pData,
                                   _In_ IRawElementProviderSimple* const pProvider,
                                   const UiaPoint point)
{
    UiaTextRange* range = nullptr;
    try
    {
        range = new UiaTextRange(pData, pProvider, point);
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
UiaTextRange::UiaTextRange(_In_ Microsoft::Console::Render::IRenderData* pData, _In_ IRawElementProviderSimple* const pProvider) :
    UiaTextRangeBase(pData, pProvider)
{
}

UiaTextRange::UiaTextRange(_In_ Microsoft::Console::Render::IRenderData* pData,
                           _In_ IRawElementProviderSimple* const pProvider,
                           const Cursor& cursor) :
    UiaTextRangeBase(pData, pProvider, cursor)
{
}

UiaTextRange::UiaTextRange(_In_ Microsoft::Console::Render::IRenderData* pData,
                           _In_ IRawElementProviderSimple* const pProvider,
                           const Endpoint start,
                           const Endpoint end,
                           const bool degenerate) :
    UiaTextRangeBase(pData, pProvider, start, end, degenerate)
{
}

// returns a degenerate text range of the start of the row closest to the y value of point
UiaTextRange::UiaTextRange(_In_ Microsoft::Console::Render::IRenderData* pData,
                           _In_ IRawElementProviderSimple* const pProvider,
                           const UiaPoint point) :
    UiaTextRangeBase(pData, pProvider)
{
    Initialize(point);
}

IFACEMETHODIMP UiaTextRange::Clone(_Outptr_result_maybenull_ ITextRangeProvider** ppRetVal)
{
    try
    {
        *ppRetVal = new UiaTextRange(*this);
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
    // TODO GitHub #605: Search functionality
    return E_NOTIMPL;
}

void UiaTextRange::_ChangeViewport(const SMALL_RECT NewWindow)
{
    // TODO GitHub #2361: Update viewport when calling UiaTextRangeBase::ScrollIntoView()
}

void UiaTextRange::_TranslatePointToScreen(LPPOINT clientPoint) const
{
    // TODO GitHub #2103: NON-HWND IMPLEMENTATION OF CLIENTTOSCREEN()
}

void UiaTextRange::_TranslatePointFromScreen(LPPOINT screenPoint) const
{
    // TODO GitHub #2103: NON-HWND IMPLEMENTATION OF SCREENTOCLIENT()
}
