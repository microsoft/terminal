// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "PseudoConsoleWindowAccessibilityProvider.hpp"

using namespace Microsoft::Console::Interactivity;

HRESULT PseudoConsoleWindowAccessibilityProvider::RuntimeClassInitialize(HWND pseudoConsoleHwnd) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pseudoConsoleHwnd);
    _pseudoConsoleHwnd = pseudoConsoleHwnd;
    return S_OK;
}

IFACEMETHODIMP PseudoConsoleWindowAccessibilityProvider::get_ProviderOptions(_Out_ ProviderOptions* pOptions)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pOptions);
    *pOptions = ProviderOptions_ServerSideProvider;
    return S_OK;
}

IFACEMETHODIMP PseudoConsoleWindowAccessibilityProvider::GetPatternProvider(_In_ PATTERNID /*iid*/,
                                                                            _COM_Outptr_result_maybenull_ IUnknown** ppInterface)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppInterface);
    *ppInterface = nullptr;
    return S_OK;
}

IFACEMETHODIMP PseudoConsoleWindowAccessibilityProvider::GetPropertyValue(_In_ PROPERTYID propertyId,
                                                                          _Out_ VARIANT* pVariant)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pVariant);

    pVariant->vt = VT_EMPTY;

    // Returning the default will leave the property as the default
    // so we only really need to touch it for the properties we want to implement
    switch (propertyId)
    {
    case UIA_ControlTypePropertyId:
    {
        pVariant->vt = VT_I4;
        pVariant->lVal = UIA_WindowControlTypeId;
        break;
    }
    case UIA_NamePropertyId:
    {
        static constexpr auto AutomationPropertyName = L"Internal Console Management Window";
        pVariant->bstrVal = SysAllocString(AutomationPropertyName);
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
        break;
    }
    case UIA_IsControlElementPropertyId:
    case UIA_IsContentElementPropertyId:
    case UIA_IsKeyboardFocusablePropertyId:
    case UIA_HasKeyboardFocusPropertyId:
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_FALSE;
        break;
    }
    default:
        break;
    }
    return S_OK;
}

IFACEMETHODIMP PseudoConsoleWindowAccessibilityProvider::get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppProvider);
    RETURN_HR_IF_NULL(gsl::narrow_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE), _pseudoConsoleHwnd);
    return UiaHostProviderFromHwnd(_pseudoConsoleHwnd, ppProvider);
}
