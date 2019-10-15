// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TermControlUiaProvider.hpp"
#include "TermControl.h"

using namespace Microsoft::Terminal;
using namespace Microsoft::Console::Types;

HRESULT TermControlUiaProvider::RuntimeClassInitialize(_In_ winrt::Microsoft::Terminal::TerminalControl::implementation::TermControl* termControl,
                                                       _In_ std::function<RECT(void)> GetBoundingRect)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, termControl);
    RETURN_IF_FAILED(__super::RuntimeClassInitialize(termControl->GetUiaData()));

    _getBoundingRect = GetBoundingRect;
    _termControl = termControl;

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(nullptr, ApiCall::Constructor, nullptr);
    return S_OK;
}

IFACEMETHODIMP TermControlUiaProvider::Navigate(_In_ NavigateDirection direction,
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

IFACEMETHODIMP TermControlUiaProvider::get_BoundingRectangle(_Out_ UiaRect* pRect)
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

IFACEMETHODIMP TermControlUiaProvider::get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider)
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

const COORD TermControlUiaProvider::GetFontSize() const
{
    return _termControl->GetActualFont().GetSize();
}

const winrt::Windows::UI::Xaml::Thickness TermControlUiaProvider::GetPadding() const
{
    return _termControl->GetPadding();
}

std::deque<UiaTextRangeBase*> TermControlUiaProvider::GetSelectionRanges(_In_ IRawElementProviderSimple* const pProvider)
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

UiaTextRangeBase* TermControlUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider)
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

UiaTextRangeBase* TermControlUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
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

UiaTextRangeBase* TermControlUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
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

UiaTextRangeBase* TermControlUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
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
