// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "IUiaWindow.h"
#include "WindowUiaProviderBase.hpp"

using namespace Microsoft::Console::Types;

#pragma warning(suppress : 26434) // WRL RuntimeClassInitialize base is a no-op and we need this for MakeAndInitialize
HRESULT WindowUiaProviderBase::RuntimeClassInitialize(IUiaWindow* baseWindow) noexcept
{
    _baseWindow = baseWindow;
    return S_OK;
}

#pragma region IRawElementProviderSimple

// Implementation of IRawElementProviderSimple::get_ProviderOptions.
// Gets UI Automation provider options.
IFACEMETHODIMP WindowUiaProviderBase::get_ProviderOptions(_Out_ ProviderOptions* pOptions)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pOptions);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    *pOptions = ProviderOptions_ServerSideProvider;
    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_PatternProvider.
// Gets the object that supports ISelectionPattern.
IFACEMETHODIMP WindowUiaProviderBase::GetPatternProvider(_In_ PATTERNID /*patternId*/,
                                                         _COM_Outptr_result_maybenull_ IUnknown** ppInterface)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppInterface);
    *ppInterface = nullptr;
    RETURN_IF_FAILED(_EnsureValidHwnd());

    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_PropertyValue.
// Gets custom properties.
IFACEMETHODIMP WindowUiaProviderBase::GetPropertyValue(_In_ PROPERTYID propertyId, _Out_ VARIANT* pVariant)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pVariant);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    pVariant->vt = VT_EMPTY;

    // Returning the default will leave the property as the default
    // so we only really need to touch it for the properties we want to implement
    if (propertyId == UIA_ControlTypePropertyId)
    {
        pVariant->vt = VT_I4;
        pVariant->lVal = UIA_WindowControlTypeId;
    }
    else if (propertyId == UIA_AutomationIdPropertyId)
    {
        pVariant->bstrVal = SysAllocString(AutomationIdPropertyName);
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
    }
    else if (propertyId == UIA_IsControlElementPropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }
    else if (propertyId == UIA_IsContentElementPropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }
    else if (propertyId == UIA_IsKeyboardFocusablePropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }
    else if (propertyId == UIA_HasKeyboardFocusPropertyId)
    {
        pVariant->vt = VT_BOOL;
        pVariant->boolVal = VARIANT_TRUE;
    }
    else if (propertyId == UIA_ProviderDescriptionPropertyId)
    {
        pVariant->bstrVal = SysAllocString(ProviderDescriptionPropertyName);
        if (pVariant->bstrVal != nullptr)
        {
            pVariant->vt = VT_BSTR;
        }
    }

    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_HostRawElementProvider.
// Gets the default UI Automation provider for the host window. This provider
// supplies many properties.
IFACEMETHODIMP WindowUiaProviderBase::get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppProvider);
    try
    {
        const HWND hwnd = GetWindowHandle();
        return UiaHostProviderFromHwnd(hwnd, ppProvider);
    }
    catch (...)
    {
        return gsl::narrow_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
    }
}
#pragma endregion

#pragma region IRawElementProviderFragment

IFACEMETHODIMP WindowUiaProviderBase::GetRuntimeId(_Outptr_result_maybenull_ SAFEARRAY** ppRuntimeId)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRuntimeId);
    RETURN_IF_FAILED(_EnsureValidHwnd());
    // Root defers this to host, others must implement it...
    *ppRuntimeId = nullptr;

    return S_OK;
}

IFACEMETHODIMP WindowUiaProviderBase::get_BoundingRectangle(_Out_ UiaRect* pRect)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pRect);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    const IUiaWindow* const pConsoleWindow = _baseWindow;
    RETURN_HR_IF_NULL((HRESULT)UIA_E_ELEMENTNOTAVAILABLE, pConsoleWindow);

    RECT const rc = pConsoleWindow->GetWindowRect();

    pRect->left = rc.left;
    pRect->top = rc.top;

    LONG longWidth = 0;
    RETURN_IF_FAILED(LongSub(rc.right, rc.left, &longWidth));
    pRect->width = longWidth;
    LONG longHeight = 0;
    RETURN_IF_FAILED(LongSub(rc.bottom, rc.top, &longHeight));
    pRect->height = longHeight;

    return S_OK;
}

IFACEMETHODIMP WindowUiaProviderBase::GetEmbeddedFragmentRoots(_Outptr_result_maybenull_ SAFEARRAY** ppRoots)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRoots);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    *ppRoots = nullptr;
    return S_OK;
}

IFACEMETHODIMP WindowUiaProviderBase::get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppProvider);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    RETURN_IF_FAILED(QueryInterface(IID_PPV_ARGS(ppProvider)));
    return S_OK;
}

#pragma endregion

HWND WindowUiaProviderBase::GetWindowHandle() const
{
    const IUiaWindow* const pConsoleWindow = _baseWindow;
    if (pConsoleWindow)
    {
        return pConsoleWindow->GetWindowHandle();
    }
    else
    {
        return nullptr;
    }
}

[[nodiscard]] HRESULT WindowUiaProviderBase::_EnsureValidHwnd() const
{
    try
    {
        HWND const hwnd = GetWindowHandle();
        RETURN_HR_IF((HRESULT)UIA_E_ELEMENTNOTAVAILABLE, !(IsWindow(hwnd)));
    }
    CATCH_RETURN();
    return S_OK;
}

void WindowUiaProviderBase::ChangeViewport(const SMALL_RECT NewWindow)
{
    _baseWindow->ChangeViewport(NewWindow);
}

RECT WindowUiaProviderBase::GetWindowRect() const noexcept
{
    return _baseWindow->GetWindowRect();
}
