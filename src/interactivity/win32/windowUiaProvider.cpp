// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "screenInfoUiaProvider.hpp"
#include "windowUiaProvider.hpp"

#include "../renderer/inc/IRenderData.hpp"
#include "../host/renderData.hpp"
#include "../inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;

HRESULT WindowUiaProvider::RuntimeClassInitialize(_In_ IConsoleWindow* baseWindow) noexcept
try
{
    _baseWindow = baseWindow;

    auto& g = ServiceLocator::LocateGlobals();
    auto& gci = g.getConsoleInformation();
    Render::IRenderData* renderData = &gci.renderData;

    RETURN_IF_FAILED(WRL::MakeAndInitialize<ScreenInfoUiaProvider>(&_pScreenInfoProvider, renderData, this));

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(pWindowProvider, ApiCall::Create, nullptr);

    return S_OK;
}
CATCH_RETURN();

[[nodiscard]] HRESULT WindowUiaProvider::Signal(_In_ EVENTID id)
{
    auto hr = S_OK;

    // ScreenInfoUiaProvider is responsible for signaling selection
    // changed events and text changed events
    if (id == UIA_Text_TextSelectionChangedEventId ||
        id == UIA_Text_TextChangedEventId)
    {
        if (_pScreenInfoProvider)
        {
            hr = _pScreenInfoProvider->Signal(id);
        }
        else
        {
            hr = E_POINTER;
        }
        return hr;
    }

    if (_signalEventFiring.find(id) != _signalEventFiring.end() &&
        _signalEventFiring[id] == true)
    {
        return hr;
    }

    try
    {
        _signalEventFiring[id] = true;
    }
    CATCH_RETURN();

    hr = UiaRaiseAutomationEvent(this, id);
    _signalEventFiring[id] = false;

    return hr;
}

[[nodiscard]] HRESULT WindowUiaProvider::SetTextAreaFocus()
{
    try
    {
        return _pScreenInfoProvider->Signal(UIA_AutomationFocusChangedEventId);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Raises a UIA notification event so that screen readers speak the given text
// Arguments:
// - text: the text to be announced
// Return Value:
// - S_OK, or a suitable HRESULT on failure
[[nodiscard]] HRESULT WindowUiaProvider::SignalAnnouncement(const std::wstring_view text) noexcept
try
{
    // Raise the announcement on the text area because
    // that's the element screen readers are actually reading from
    RETURN_HR_IF_NULL(E_POINTER, _pScreenInfoProvider.Get());

    const wil::unique_bstr announcement{ SysAllocStringLen(text.data(), gsl::narrow<UINT>(text.size())) };
    RETURN_IF_NULL_ALLOC(announcement);

    static const auto activityId = wil::make_bstr_nothrow(L"ConhostSearchResultAnnouncement");
    RETURN_IF_NULL_ALLOC(activityId);

    return UiaRaiseNotificationEvent(_pScreenInfoProvider.Get(),
                                     NotificationKind_ActionCompleted,
                                     NotificationProcessing_ImportantMostRecent,
                                     announcement.get(),
                                     activityId.get());
}
CATCH_RETURN();

#pragma region IRawElementProviderSimple

// Implementation of IRawElementProviderSimple::get_ProviderOptions.
// Gets UI Automation provider options.
IFACEMETHODIMP WindowUiaProvider::get_ProviderOptions(_Out_ ProviderOptions* pOptions)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pOptions);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    *pOptions = ProviderOptions_ServerSideProvider;
    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_PatternProvider.
// Gets the object that supports ISelectionPattern.
IFACEMETHODIMP WindowUiaProvider::GetPatternProvider(_In_ PATTERNID /*patternId*/,
                                                     _COM_Outptr_result_maybenull_ IUnknown** ppInterface)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppInterface);
    *ppInterface = nullptr;
    RETURN_IF_FAILED(_EnsureValidHwnd());

    return S_OK;
}

// Implementation of IRawElementProviderSimple::get_PropertyValue.
// Gets custom properties.
IFACEMETHODIMP WindowUiaProvider::GetPropertyValue(_In_ PROPERTYID propertyId, _Out_ VARIANT* pVariant)
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
IFACEMETHODIMP WindowUiaProvider::get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppProvider);
    try
    {
        const auto hwnd = GetWindowHandle();
        return UiaHostProviderFromHwnd(hwnd, ppProvider);
    }
    catch (...)
    {
        return gsl::narrow_cast<HRESULT>(UIA_E_ELEMENTNOTAVAILABLE);
    }
}
#pragma endregion

#pragma region IRawElementProviderFragment

IFACEMETHODIMP WindowUiaProvider::Navigate(_In_ NavigateDirection direction, _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());
    *ppProvider = nullptr;
    auto hr = S_OK;

    if (direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild)
    {
        RETURN_IF_FAILED(_pScreenInfoProvider.CopyTo(ppProvider));

        // signal that the focus changed
        LOG_IF_FAILED(_pScreenInfoProvider->Signal(UIA_AutomationFocusChangedEventId));
    }

    // For the other directions (parent, next, previous) the default of nullptr is correct
    return hr;
}

IFACEMETHODIMP WindowUiaProvider::GetRuntimeId(_Outptr_result_maybenull_ SAFEARRAY** ppRuntimeId)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRuntimeId);
    RETURN_IF_FAILED(_EnsureValidHwnd());
    // Root defers this to host, others must implement it...
    *ppRuntimeId = nullptr;

    return S_OK;
}

IFACEMETHODIMP WindowUiaProvider::get_BoundingRectangle(_Out_ UiaRect* pRect)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pRect);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    RETURN_HR_IF_NULL((HRESULT)UIA_E_ELEMENTNOTAVAILABLE, _baseWindow);

    const auto rc = _baseWindow->GetWindowRect();

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

IFACEMETHODIMP WindowUiaProvider::GetEmbeddedFragmentRoots(_Outptr_result_maybenull_ SAFEARRAY** ppRoots)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppRoots);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    *ppRoots = nullptr;
    return S_OK;
}

IFACEMETHODIMP WindowUiaProvider::SetFocus()
{
    RETURN_IF_FAILED(_EnsureValidHwnd());
    return Signal(UIA_AutomationFocusChangedEventId);
}

IFACEMETHODIMP WindowUiaProvider::get_FragmentRoot(_COM_Outptr_result_maybenull_ IRawElementProviderFragmentRoot** ppProvider)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, ppProvider);
    RETURN_IF_FAILED(_EnsureValidHwnd());

    RETURN_IF_FAILED(QueryInterface(IID_PPV_ARGS(ppProvider)));
    return S_OK;
}

#pragma endregion

#pragma region IRawElementProviderFragmentRoot

IFACEMETHODIMP WindowUiaProvider::ElementProviderFromPoint(_In_ double /*x*/,
                                                           _In_ double /*y*/,
                                                           _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());

    RETURN_IF_FAILED(_pScreenInfoProvider.CopyTo(ppProvider));

    return S_OK;
}

IFACEMETHODIMP WindowUiaProvider::GetFocus(_COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());
    return _pScreenInfoProvider->QueryInterface(IID_PPV_ARGS(ppProvider));
}

#pragma endregion

HWND WindowUiaProvider::GetWindowHandle() const
{
    if (_baseWindow)
    {
        return _baseWindow->GetWindowHandle();
    }
    else
    {
        return nullptr;
    }
}

[[nodiscard]] HRESULT WindowUiaProvider::_EnsureValidHwnd() const
{
    try
    {
        const auto hwnd = GetWindowHandle();
        RETURN_HR_IF((HRESULT)UIA_E_ELEMENTNOTAVAILABLE, !(IsWindow(hwnd)));
    }
    CATCH_RETURN();
    return S_OK;
}

void WindowUiaProvider::ChangeViewport(const til::inclusive_rect& NewWindow)
{
    _baseWindow->ChangeViewport(NewWindow);
}

til::rect WindowUiaProvider::GetWindowRect() const noexcept
{
    return _baseWindow->GetWindowRect();
}
