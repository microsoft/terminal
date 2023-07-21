// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "TermControlUiaProvider.hpp"

using namespace Microsoft::Terminal;
using namespace Microsoft::Console::Types;
using namespace Microsoft::WRL;

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT TermControlUiaProvider::RuntimeClassInitialize(_In_ Console::Render::IRenderData* const renderData,
                                                       _In_ ::Microsoft::Console::Types::IControlAccessibilityInfo* controlInfo) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, renderData);
    RETURN_IF_FAILED(ScreenInfoUiaProviderBase::RuntimeClassInitialize(renderData));

    _controlInfo = controlInfo;
    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_PropertyValue.
// Gets custom properties.
IFACEMETHODIMP TermControlUiaProvider::GetPropertyValue(_In_ PROPERTYID propertyId,
                                                        _Out_ VARIANT* pVariant) noexcept
{
    pVariant->vt = VT_EMPTY;

    // Returning the default will leave the property as the default
    // so we only really need to touch it for the properties we want to implement
    switch (propertyId)
    {
    case UIA_ClassNamePropertyId:
        pVariant->bstrVal = SysAllocString(L"TermControl");
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
        break;
    case UIA_ControlTypePropertyId:
        pVariant->vt = VT_I4;
        pVariant->lVal = UIA_TextControlTypeId;
        break;
    case UIA_LocalizedControlTypePropertyId:
        // TODO: we should use RS_(L"TerminalControl_ControlType"),
        // but that's exposed/defined in the TermControl project
        pVariant->bstrVal = SysAllocString(L"terminal");
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
        break;
    case UIA_OrientationPropertyId:
        pVariant->vt = VT_I4;
        pVariant->lVal = OrientationType_Vertical;
        break;
    case UIA_LiveSettingPropertyId:
        pVariant->vt = VT_I4;
        pVariant->lVal = LiveSetting::Polite;
        break;
    default:
        // fall back to the shared implementation
        return ScreenInfoUiaProviderBase::GetPropertyValue(propertyId, pVariant);
    }
    return S_OK;
}

IFACEMETHODIMP TermControlUiaProvider::Navigate(_In_ NavigateDirection direction,
                                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppProvider);
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

IFACEMETHODIMP TermControlUiaProvider::get_BoundingRectangle(_Out_ UiaRect* pRect) noexcept
{
    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(this, ApiCall::GetBoundingRectangle, nullptr);

    const auto rc = _controlInfo->GetBounds();

    pRect->left = rc.left;
    pRect->top = rc.top;
    pRect->width = static_cast<double>(rc.right) - static_cast<double>(rc.left);
    pRect->height = static_cast<double>(rc.bottom) - static_cast<double>(rc.top);

    return S_OK;
}

IFACEMETHODIMP TermControlUiaProvider::get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider) noexcept
{
    try
    {
        return _controlInfo->GetHostUiaProvider(ppProvider);
    }
    CATCH_RETURN();
}

IFACEMETHODIMP TermControlUiaProvider::get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppProvider);

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

til::size TermControlUiaProvider::GetFontSize() const noexcept
{
    return _controlInfo->GetFontSize();
}

til::rect TermControlUiaProvider::GetPadding() const noexcept
{
    return _controlInfo->GetPadding();
}

double TermControlUiaProvider::GetScaleFactor() const noexcept
{
    return _controlInfo->GetScaleFactor();
}

void TermControlUiaProvider::ChangeViewport(const til::inclusive_rect& NewWindow)
{
    _controlInfo->ChangeViewport(NewWindow);
}

HRESULT TermControlUiaProvider::GetSelectionRange(_In_ IRawElementProviderSimple* pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;

    const auto start = _pData->GetSelectionAnchor();

    // we need to make end exclusive
    auto end = _pData->GetSelectionEnd();
    _pData->GetTextBuffer().GetSize().IncrementInBounds(end, true);

    TermControlUiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<TermControlUiaTextRange>(&result, _pData, pProvider, start, end, _pData->IsBlockSelection(), wordDelimiters));
    *ppUtr = result;
    return S_OK;
}

HRESULT TermControlUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider, const std::wstring_view wordDelimiters, _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;
    TermControlUiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<TermControlUiaTextRange>(&result, _pData, pProvider, wordDelimiters));
    *ppUtr = result;
    return S_OK;
}

HRESULT TermControlUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                const Cursor& cursor,
                                                const std::wstring_view wordDelimiters,
                                                _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;
    TermControlUiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<TermControlUiaTextRange>(&result, _pData, pProvider, cursor, wordDelimiters));
    *ppUtr = result;
    return S_OK;
}

HRESULT TermControlUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                const til::point start,
                                                const til::point end,
                                                const std::wstring_view wordDelimiters,
                                                _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;
    TermControlUiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<TermControlUiaTextRange>(&result, _pData, pProvider, start, end, false, wordDelimiters));
    *ppUtr = result;
    return S_OK;
}

HRESULT TermControlUiaProvider::CreateTextRange(_In_ IRawElementProviderSimple* const pProvider,
                                                const UiaPoint point,
                                                const std::wstring_view wordDelimiters,
                                                _COM_Outptr_result_maybenull_ UiaTextRangeBase** ppUtr)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppUtr);
    *ppUtr = nullptr;
    TermControlUiaTextRange* result = nullptr;
    RETURN_IF_FAILED(MakeAndInitialize<TermControlUiaTextRange>(&result, _pData, pProvider, point, wordDelimiters));
    *ppUtr = result;
    return S_OK;
}
