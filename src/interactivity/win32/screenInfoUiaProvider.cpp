// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "windowUiaProvider.hpp"
#include "screenInfoUiaProvider.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Interactivity;
using namespace Microsoft::Console::Interactivity::Win32;
using namespace Microsoft::WRL;

HRESULT ScreenInfoUiaProvider::RuntimeClassInitialize(_In_ Render::IRenderData* pData,
                                                      _In_ WindowUiaProvider* const pUiaParent)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pUiaParent);
    RETURN_HR_IF_NULL(E_INVALIDARG, pData);

    RETURN_IF_FAILED(ScreenInfoUiaProviderBase::RuntimeClassInitialize(pData));
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

    auto rc = _pUiaParent->GetWindowRect();

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

void ScreenInfoUiaProvider::ChangeViewport(const til::inclusive_rect& NewWindow)
{
    _pUiaParent->ChangeViewport(NewWindow);
}

HRESULT ScreenInfoUiaProvider::GetSelectionRange(_In_ IRawElementProviderSimple* pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ Microsoft::Console::Types::UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;

    const auto start = _pData->GetSelectionAnchor();

    // we need to make end exclusive
    auto end = _pData->GetSelectionEnd();
    _pData->GetTextBuffer().GetSize().IncrementInBounds(end, true);

    // TODO GH #4509: Box Selection is misrepresented here as a line selection.
    UiaTextRange* result;
    RETURN_IF_FAILED(MakeAndInitialize<UiaTextRange>(&result, _pData, pProvider, start, end, _pData->IsBlockSelection(), wordDelimiters));
    *ppUtr = result;
    return S_OK;
}

HRESULT ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;
    UiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<UiaTextRange>(&result, _pData, pProvider, wordDelimiters));
    *ppUtr = result;
    return S_OK;
}

HRESULT ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                               const Cursor& cursor,
                                               const std::wstring_view wordDelimiters,
                                               _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;
    UiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<UiaTextRange>(&result, _pData, pProvider, cursor, wordDelimiters));
    *ppUtr = result;
    return S_OK;
}

HRESULT ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                               const til::point start,
                                               const til::point end,
                                               const std::wstring_view wordDelimiters,
                                               _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;
    UiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<UiaTextRange>(&result, _pData, pProvider, start, end, false, wordDelimiters));
    *ppUtr = result;
    return S_OK;
}

HRESULT ScreenInfoUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                               const UiaPoint point,
                                               const std::wstring_view wordDelimiters,
                                               _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;
    UiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<UiaTextRange>(&result, _pData, pProvider, point, wordDelimiters));
    *ppUtr = result;
    return S_OK;
}
