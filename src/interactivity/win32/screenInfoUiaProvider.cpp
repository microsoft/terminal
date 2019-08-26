// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "screenInfoUiaProvider.hpp"
#include "..\types\WindowUiaProviderBase.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Interactivity::Win32;

ScreenInfoUiaProvider::ScreenInfoUiaProvider(_In_ IUiaData* pData,
                                             _In_ WindowUiaProviderBase* const pUiaParent) :
    _pUiaParent(THROW_HR_IF_NULL(E_INVALIDARG, pUiaParent)),
    ScreenInfoUiaProviderBase(THROW_HR_IF_NULL(E_INVALIDARG, pData))
{
}

IFACEMETHODIMP ScreenInfoUiaProvider::Navigate(_In_ NavigateDirection direction,
                                               _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    RETURN_HR_IF(E_INVALIDARG, ppProvider == nullptr);
    *ppProvider = nullptr;

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    /*ApiMsgNavigate apiMsg;
    apiMsg.Direction = direction;
    Tracing::s_TraceUia(this, ApiCall::Navigate, &apiMsg);*/

    if (direction == NavigateDirection_Parent)
    {
        try
        {
            _pUiaParent->QueryInterface(IID_PPV_ARGS(ppProvider));
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

    RECT rc = _pUiaParent->GetWindowRect();

    pRect->left = rc.left;
    pRect->top = rc.top;
    pRect->width = rc.right - rc.left;
    pRect->height = rc.bottom - rc.top;

    return S_OK;
}

IFACEMETHODIMP ScreenInfoUiaProvider::get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider)
{
    RETURN_HR_IF(E_INVALIDARG, ppProvider == nullptr);
    *ppProvider = nullptr;

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetFragmentRoot, nullptr);
    try
    {
        _pUiaParent->QueryInterface(IID_PPV_ARGS(ppProvider));
    }
    catch (...)
    {
        *ppProvider = nullptr;
        return wil::ResultFromCaughtException();
    }
    RETURN_IF_NULL_ALLOC(*ppProvider);
    return S_OK;
}

HWND ScreenInfoUiaProvider::GetWindowHandle() const
{
    return _pUiaParent->GetWindowHandle();
}

void ScreenInfoUiaProvider::ChangeViewport(const SMALL_RECT NewWindow)
{
    _pUiaParent->ChangeViewport(NewWindow);
}

std::deque<UiaTextRangeBase*> ScreenInfoUiaProvider::GetSelectionRanges(_In_ IRawElementProviderSimple* pProvider)
{
    std::deque<UiaTextRangeBase*> result;

    auto ranges = UiaTextRange::GetSelectionRanges(_pData, pProvider);
    while (!ranges.empty())
    {
        result.emplace_back(ranges.back());
        ranges.pop_back();
    }

    return result;
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider)
{
    return UiaTextRange::Create(_pData, pProvider);
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                         const Cursor& cursor)
{
    return UiaTextRange::Create(_pData, pProvider, cursor);
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                         const Endpoint start,
                                                         const Endpoint end,
                                                         const bool degenerate)
{
    return UiaTextRange::Create(_pData, pProvider, start, end, degenerate);
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                         const UiaPoint point)
{
    return UiaTextRange::Create(_pData, pProvider, point);
}
