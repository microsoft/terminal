/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- windowUiaProvider.hpp

Abstract:
- This module provides UI Automation access to the console window to
  support both automation tests and accessibility (screen reading)
  applications.
- Based on examples, sample code, and guidance from
  https://msdn.microsoft.com/en-us/library/windows/desktop/ee671596(v=vs.85).aspx

Author(s):
- Michael Niksa (MiNiksa)     2017
- Austin Diviness (AustDi)    2017
--*/

#pragma once

#include "precomp.h"
#include "screenInfoUiaProvider.hpp"
#include "../types/WindowUiaProviderBase.hpp"

namespace Microsoft::Console::Types
{
    class IConsoleWindow;
}

namespace Microsoft::Console::Interactivity::Win32
{
    class WindowUiaProvider final :
        public Microsoft::Console::Types::WindowUiaProviderBase
    {
    public:
        WindowUiaProvider() = default;
        HRESULT WindowUiaProvider::RuntimeClassInitialize(_In_ Microsoft::Console::Types::IConsoleWindow* baseWindow);

        [[nodiscard]] HRESULT Signal(_In_ EVENTID id) override;
        [[nodiscard]] HRESULT SetTextAreaFocus() override;

        // IRawElementProviderFragment methods
        IFACEMETHODIMP Navigate(_In_ NavigateDirection direction,
                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) override;
        IFACEMETHODIMP SetFocus() override;

        // IRawElementProviderFragmentRoot methods
        IFACEMETHODIMP ElementProviderFromPoint(_In_ double x,
                                                _In_ double y,
                                                _COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) override;
        IFACEMETHODIMP GetFocus(_COM_Outptr_result_maybenull_ IRawElementProviderFragment** ppProvider) override;

    protected:
        const OLECHAR* AutomationIdPropertyName = L"Console Window";
        const OLECHAR* ProviderDescriptionPropertyName = L"Microsoft Console Host Window";

    private:
        WRL::ComPtr<ScreenInfoUiaProvider> _pScreenInfoProvider;
    };
}
