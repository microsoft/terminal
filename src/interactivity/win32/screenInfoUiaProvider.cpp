// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "..\types\WindowUiaProviderBase.hpp"
#include "screenInfoUiaProvider.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Interactivity::Win32;

HRESULT ScreenInfoUiaProvider::RuntimeClassInitialize(_In_ Microsoft::Console::Types::IUiaData* pData,
                                                      _In_ Microsoft::Console::Types::WindowUiaProviderBase* const pUiaParent)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pUiaParent);
    RETURN_HR_IF_NULL(E_INVALIDARG, pData);

    RETURN_IF_FAILED(__super::RuntimeClassInitialize(pData));
    _pUiaParent = pUiaParent;
    return S_OK;
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
    UiaTextRange* retVal;
    auto hr = WRL::MakeAndInitialize<UiaTextRange>(&retVal, _pData, pProvider);
    if (FAILED(hr))
    {
        LOG_HR(hr);
        return nullptr;
    }
    return retVal;
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                         const Cursor& cursor)
{
    UiaTextRange* retVal;
    auto hr = WRL::MakeAndInitialize<UiaTextRange>(&retVal, _pData, pProvider, cursor);
    if (FAILED(hr))
    {
        LOG_HR(hr);
        return nullptr;
    }
    return retVal;
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                         const Endpoint start,
                                                         const Endpoint end,
                                                         const bool degenerate)
{
    UiaTextRange* retVal;
    auto hr = WRL::MakeAndInitialize<UiaTextRange>(&retVal, _pData, pProvider, start, end, degenerate);
    if (FAILED(hr))
    {
        LOG_HR(hr);
        return nullptr;
    }
    return retVal;
}

UiaTextRangeBase* ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                         const UiaPoint point)
{
    UiaTextRange* retVal;
    auto hr = WRL::MakeAndInitialize<UiaTextRange>(&retVal, _pData, pProvider, point);
    if (FAILED(hr))
    {
        LOG_HR(hr);
        return nullptr;
    }
    return retVal;
}
