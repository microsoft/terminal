// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "precomp.h"

#include "screenInfoUiaProvider.hpp"
#include "windowUiaProvider.hpp"

#include "../types/IUiaData.h"
#include "../host/renderData.hpp"
#include "../inc/ServiceLocator.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity::Win32;

HRESULT WindowUiaProvider::RuntimeClassInitialize(_In_ IConsoleWindow* baseWindow)
{
    RETURN_IF_FAILED(WindowUiaProviderBase::RuntimeClassInitialize(baseWindow));

    Globals& g = ServiceLocator::LocateGlobals();
    CONSOLE_INFORMATION& gci = g.getConsoleInformation();
    IUiaData* uiaData = &gci.renderData;

    RETURN_IF_FAILED(WRL::MakeAndInitialize<ScreenInfoUiaProvider>(&_pScreenInfoProvider, uiaData, this));

    // TODO GitHub #1914: Re-attach Tracing to UIA Tree
    //Tracing::s_TraceUia(pWindowProvider, ApiCall::Create, nullptr);

    return S_OK;
}

[[nodiscard]] HRESULT WindowUiaProvider::SetTextAreaFocus()
{
    try
    {
        return _pScreenInfoProvider->Signal(UIA_AutomationFocusChangedEventId);
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT WindowUiaProvider::Signal(_In_ EVENTID id)
{
    HRESULT hr = S_OK;

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

#pragma region IRawElementProviderFragment

IFACEMETHODIMP WindowUiaProvider::Navigate(_In_ NavigateDirection direction, _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());
    *ppProvider = nullptr;
    HRESULT hr = S_OK;

    if (direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild)
    {
        *ppProvider = _pScreenInfoProvider.Get();

        // signal that the focus changed
        LOG_IF_FAILED(_pScreenInfoProvider->Signal(UIA_AutomationFocusChangedEventId));
    }

    // For the other directions (parent, next, previous) the default of nullptr is correct
    return hr;
}

IFACEMETHODIMP WindowUiaProvider::SetFocus()
{
    RETURN_IF_FAILED(_EnsureValidHwnd());
    return Signal(UIA_AutomationFocusChangedEventId);
}
#pragma endregion

#pragma region IRawElementProviderFragmentRoot

IFACEMETHODIMP WindowUiaProvider::ElementProviderFromPoint(_In_ double /*x*/,
                                                           _In_ double /*y*/,
                                                           _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());

    *ppProvider = _pScreenInfoProvider.Get();

    return S_OK;
}

IFACEMETHODIMP WindowUiaProvider::GetFocus(_COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());
    return _pScreenInfoProvider->QueryInterface(IID_PPV_ARGS(ppProvider));
}

#pragma endregion
