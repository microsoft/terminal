// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "precomp.h"

#include "ApiDetector.hpp"

#include "../inc/IInteractivityFactory.hpp"
#include <wrl/implements.h>

#include <map>

#pragma hdrstop

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

        const OLECHAR* AutomationPropertyName = L"PseudoConsoleWindow";
        const OLECHAR* ProviderDescriptionPropertyName = L"Pseudo Console Window";
    };

    class InteractivityFactory final : public IInteractivityFactory
    {
    public:
        [[nodiscard]] NTSTATUS CreateConsoleControl(_Inout_ std::unique_ptr<IConsoleControl>& control);
        [[nodiscard]] NTSTATUS CreateConsoleInputThread(_Inout_ std::unique_ptr<IConsoleInputThread>& thread);

        [[nodiscard]] NTSTATUS CreateHighDpiApi(_Inout_ std::unique_ptr<IHighDpiApi>& api);
        [[nodiscard]] NTSTATUS CreateWindowMetrics(_Inout_ std::unique_ptr<IWindowMetrics>& metrics);
        [[nodiscard]] NTSTATUS CreateAccessibilityNotifier(_Inout_ std::unique_ptr<IAccessibilityNotifier>& notifier);
        [[nodiscard]] NTSTATUS CreateSystemConfigurationProvider(_Inout_ std::unique_ptr<ISystemConfigurationProvider>& provider);

        [[nodiscard]] NTSTATUS CreatePseudoWindow(HWND& hwnd, const HWND owner);

        // Wndproc
        [[nodiscard]] static LRESULT CALLBACK s_PseudoWindowProc(_In_ HWND hwnd,
                                                                 _In_ UINT uMsg,
                                                                 _In_ WPARAM wParam,
                                                                 _In_ LPARAM lParam);
        [[nodiscard]] LRESULT CALLBACK PseudoWindowProc(_In_ HWND,
                                                        _In_ UINT uMsg,
                                                        _In_ WPARAM wParam,
                                                        _In_ LPARAM lParam);

    private:
        void _WritePseudoWindowCallback(bool showOrHide);

        HWND _pseudoConsoleWindowHwnd{ nullptr };
        WRL::ComPtr<PseudoConsoleWindowAccessibilityProvider> _pUiaProvider{ nullptr };
    };
}
