// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ContentProcess.g.h"
#include "ControlInteractivity.h"

struct __declspec(uuid("AD778226-B097-48E9-B1C3-9555A2D73830")) ISwapChainProvider : ::IUnknown
{
    virtual STDMETHODIMP GetSwapChainHandle([out] HANDLE* swapChainHandle) = 0;
};

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ContentProcess : ContentProcessT<ContentProcess, ISwapChainProvider>
    {
        ContentProcess();
        ~ContentProcess();

        bool Initialize(Control::IControlSettings settings,
                        Control::IControlAppearance unfocusedAppearance,
                        TerminalConnection::ConnectionInformation connectionInfo);
        Control::ControlInteractivity GetInteractivity();

        uint64_t GetPID();
        uint64_t RequestSwapChainHandle(const uint64_t pid);

        STDMETHODIMP GetSwapChainHandle(HANDLE* swapChainHandle);

    private:
        Control::ControlInteractivity _interactivity{ nullptr };
        uint64_t _ourPID;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(ContentProcess);
}
