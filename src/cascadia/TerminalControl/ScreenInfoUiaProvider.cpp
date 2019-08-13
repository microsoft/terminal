// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ScreenInfoUiaProvider.hpp"
#include "TermControl.h"

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

using namespace Microsoft::Terminal;

ScreenInfoUiaProvider::ScreenInfoUiaProvider(_In_ winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl const& termControl,
                                             _In_ std::function<RECT(void)> GetBoundingRect) :
    _getBoundingRect(GetBoundingRect),
    _termControl(termControl),
    ScreenInfoUiaProviderBase(THROW_HR_IF_NULL(E_INVALIDARG, termControl.GetRenderData()))
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(nullptr, ApiCall::Constructor, nullptr);
}

IFACEMETHODIMP ScreenInfoUiaProvider::Navigate(_In_ NavigateDirection direction,
                                               _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    /*ApiMsgNavigate apiMsg;
    apiMsg.Direction = direction;
    Tracing::s_TraceUia(this, ApiCall::Navigate, &apiMsg);*/
    *ppProvider = nullptr;

    if (direction == NavigateDirection_Parent)
    {
        try
        {
            // TODO GitHub #2102: UIA Tree Navigation
            //_pUiaParent->QueryInterface(IID_PPV_ARGS(ppProvider));
        }
        catch (...)
        {
            *ppProvider = nullptr;
            return wil::ResultFromCaughtException();
        }
        RETURN_IF_NULL_ALLOC(*ppProvider);
    }

    // For the other directions the default of nullptr is correct
    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_BoundingRectangle(_Out_ UiaRect* pRect)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetBoundingRectangle, nullptr);

    RECT rc = _getBoundingRect();

    pRect->left = rc.left;
    pRect->top = rc.top;
    pRect->width = rc.right - rc.left;
    pRect->height = rc.bottom - rc.top;

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider)
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetFragmentRoot, nullptr);
    try
    {
        // TODO GitHub #2102: UIA Tree Navigation - the special fragments that knows about all of its descendants is called a fragment root
        //_pUiaParent->QueryInterface(IID_PPV_ARGS(ppProvider));
        *ppProvider = nullptr;
    }
    catch (...)
    {
        *ppProvider = nullptr;
        return wil::ResultFromCaughtException();
    }
    RETURN_IF_NULL_ALLOC(*ppProvider);
    return S_OK;
}

const COORD ScreenInfoUiaProvider::getFontSize() const
{
    return _termControl.GetActualFont().GetSize();
}

std::deque<UiaTextRangeBase*> ScreenInfoUiaProvider::GetSelectionRangeUTRs(_In_ IRenderData* pData,
                                                                           _In_ IRawElementProviderSimple* pProvider)
{
    std::deque<UiaTextRangeBase*> result;

    auto ranges = UiaTextRange::GetSelectionRanges(pData, pProvider);
    while (!ranges.empty())
    {
        result.emplace_back(ranges.back());
        ranges.pop_back();
    }

    return result;
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateUTR(_In_ IRenderData* pData,
                                                   _In_ IRawElementProviderSimple* const pProvider)
{
    return UiaTextRange::Create(pData, pProvider);
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateUTR(_In_ IRenderData* pData,
                                                   _In_ IRawElementProviderSimple* const pProvider,
                                                   const Cursor& cursor)
{
    return UiaTextRange::Create(pData, pProvider, cursor);
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateUTR(_In_ IRenderData* pData,
                                                   _In_ IRawElementProviderSimple* const pProvider,
                                                   const Endpoint start,
                                                   const Endpoint end,
                                                   const bool degenerate)
{
    return UiaTextRange::Create(pData, pProvider, start, end, degenerate);
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateUTR(_In_ IRenderData* pData,
                                                   _In_ IRawElementProviderSimple* const pProvider,
                                                   const UiaPoint point)
{
    return UiaTextRange::Create(pData, pProvider, point);
}
