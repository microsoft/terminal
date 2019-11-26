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
- Carlos Zamora (CaZamor)     2019
--*/

#pragma once

#include "../types/WindowUiaProviderBase.hpp"
#include "../types/IUiaWindow.h"

class WindowUiaProvider final :
    public Microsoft::Console::Types::WindowUiaProviderBase
{
public:
    WindowUiaProvider() = default;
    HRESULT RuntimeClassInitialize(Microsoft::Console::Types::IUiaWindow* baseWindow);
    static WindowUiaProvider* Create(Microsoft::Console::Types::IUiaWindow* baseWindow);

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
    const OLECHAR* AutomationIdPropertyName = L"Terminal Window";
    const OLECHAR* ProviderDescriptionPropertyName = L"Microsoft Windows Terminal Window";
};
