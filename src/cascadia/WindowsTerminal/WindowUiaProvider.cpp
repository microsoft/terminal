// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
#include "pch.h"

#include "WindowUiaProvider.hpp"

#include "../host/renderData.hpp"

HRESULT WindowUiaProvider::RuntimeClassInitialize(Microsoft::Console::Types::IUiaWindow* baseWindow)
{
    return WindowUiaProviderBase::RuntimeClassInitialize(baseWindow);
}

WindowUiaProvider* WindowUiaProvider::Create(Microsoft::Console::Types::IUiaWindow* /*baseWindow*/)
{
    WindowUiaProvider* pWindowProvider = nullptr;
    //Microsoft::Terminal::TermControlUiaProvider* pScreenInfoProvider = nullptr;
    try
    {
        //pWindowProvider = new WindowUiaProvider(baseWindow);

        // TODO GitHub #2447: Hook up ScreenInfoUiaProvider to WindowUiaProvider
        // This may be needed for the signaling model
        /*Globals& g = ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();
        Microsoft::Console::Render::IRenderData* renderData = &gci.renderData;

        pScreenInfoProvider = new Microsoft::Console::Types::ScreenInfoUiaProvider(renderData, pWindowProvider);
        pWindowProvider->_pScreenInfoProvider = pScreenInfoProvider;
        */

        // TODO GitHub #1914: Re-attach Tracing to UIA Tree
        //Tracing::s_TraceUia(pWindowProvider, ApiCall::Create, nullptr);

        return pWindowProvider;
    }
    catch (...)
    {
        if (nullptr != pWindowProvider)
        {
            pWindowProvider->Release();
        }

        LOG_CAUGHT_EXCEPTION();

        return nullptr;
    }
}

[[nodiscard]] HRESULT WindowUiaProvider::SetTextAreaFocus()
{
    try
    {
        // TODO GitHub #2447: Hook up ScreenInfoUiaProvider to WindowUiaProvider
        // This may be needed for the signaling model
        //return _pScreenInfoProvider->Signal(UIA_AutomationFocusChangedEventId);
        return E_NOTIMPL;
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
        // TODO GitHub #2447: Hook up ScreenInfoUiaProvider to WindowUiaProvider
        // This may be needed for the signaling model
        /*if (_pScreenInfoProvider)
        {
            hr = _pScreenInfoProvider->Signal(id);
        }
        else
        {
            hr = E_POINTER;
        }*/
        hr = E_POINTER;
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

    IRawElementProviderSimple* pProvider = static_cast<IRawElementProviderSimple*>(this);
    hr = UiaRaiseAutomationEvent(pProvider, id);
    _signalEventFiring[id] = false;

    return hr;
}

#pragma region IRawElementProviderFragment

IFACEMETHODIMP WindowUiaProvider::Navigate(_In_ NavigateDirection /*direction*/, _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());
    *ppProvider = nullptr;
    HRESULT hr = S_OK;

    // TODO GitHub #2102 or #2447: Hook up ScreenInfoUiaProvider to WindowUiaProvider
    // This may be needed for the signaling model
    /*if (direction == NavigateDirection_FirstChild || direction == NavigateDirection_LastChild)
    {
        *ppProvider = _pScreenInfoProvider;
        (*ppProvider)->AddRef();

        // signal that the focus changed
        LOG_IF_FAILED(_pScreenInfoProvider->Signal(UIA_AutomationFocusChangedEventId));
    }*/

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
                                                           _COM_Outptr_result_maybenull_ IRawElementProviderFragment** /*ppProvider*/)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());

    // TODO GitHub #2447: Hook up ScreenInfoUiaProvider to WindowUiaProvider
    // This may be needed for the signaling model
    /**ppProvider = _pScreenInfoProvider;
    (*ppProvider)->AddRef();*/

    return S_OK;
}

IFACEMETHODIMP WindowUiaProvider::GetFocus(_COM_Outptr_result_maybenull_ IRawElementProviderFragment** /*ppProvider*/)
{
    RETURN_IF_FAILED(_EnsureValidHwnd());
    // TODO GitHub #2447: Hook up ScreenInfoUiaProvider to WindowUiaProvider
    // This may be needed for the signaling model
    //return _pScreenInfoProvider->QueryInterface(IID_PPV_ARGS(ppProvider));
    return S_OK;
}

#pragma endregion
