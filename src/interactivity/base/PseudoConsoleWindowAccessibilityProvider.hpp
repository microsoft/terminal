// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "precomp.h"

namespace Microsoft::Console::Interactivity
{
    class PseudoConsoleWindowAccessibilityProvider final :
        public WRL::RuntimeClass<WRL::RuntimeClassFlags<WRL::ClassicCom | WRL::InhibitFtmBase>, IRawElementProviderSimple>
    {
    public:
        PseudoConsoleWindowAccessibilityProvider() = default;
        ~PseudoConsoleWindowAccessibilityProvider() = default;
        HRESULT RuntimeClassInitialize(HWND pseudoConsoleHwnd) noexcept;

        PseudoConsoleWindowAccessibilityProvider(const PseudoConsoleWindowAccessibilityProvider&) = delete;
        PseudoConsoleWindowAccessibilityProvider(PseudoConsoleWindowAccessibilityProvider&&) = delete;
        PseudoConsoleWindowAccessibilityProvider& operator=(const PseudoConsoleWindowAccessibilityProvider&) = delete;
        PseudoConsoleWindowAccessibilityProvider& operator=(PseudoConsoleWindowAccessibilityProvider&&) = delete;

        // IRawElementProviderSimple methods
        IFACEMETHODIMP get_ProviderOptions(_Out_ ProviderOptions* pOptions) override;
        IFACEMETHODIMP GetPatternProvider(_In_ PATTERNID iid,
                                          _COM_Outptr_result_maybenull_ IUnknown** ppInterface) override;
        IFACEMETHODIMP GetPropertyValue(_In_ PROPERTYID idProp,
                                        _Out_ VARIANT* pVariant) override;
        IFACEMETHODIMP get_HostRawElementProvider(_COM_Outptr_result_maybenull_ IRawElementProviderSimple** ppProvider) override;

    private:
        HWND _pseudoConsoleHwnd;
    };
}
