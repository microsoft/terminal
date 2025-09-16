// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "precomp.h"

#include "ApiDetector.hpp"

#include "../inc/IInteractivityFactory.hpp"
#include "PseudoConsoleWindowAccessibilityProvider.hpp"

#include <map>

#pragma hdrstop

namespace Microsoft::Console::Interactivity
{
    class InteractivityFactory final : public IInteractivityFactory
    {
    public:
        [[nodiscard]] NTSTATUS CreateConsoleControl(_Inout_ std::unique_ptr<IConsoleControl>& control);
        [[nodiscard]] NTSTATUS CreateConsoleInputThread(_Inout_ std::unique_ptr<IConsoleInputThread>& thread);

        [[nodiscard]] NTSTATUS CreateHighDpiApi(_Inout_ std::unique_ptr<IHighDpiApi>& api);
        [[nodiscard]] NTSTATUS CreateWindowMetrics(_Inout_ std::unique_ptr<IWindowMetrics>& metrics);
        [[nodiscard]] NTSTATUS CreateSystemConfigurationProvider(_Inout_ std::unique_ptr<ISystemConfigurationProvider>& provider);

        [[nodiscard]] NTSTATUS CreatePseudoWindow(HWND& hwnd);

        void SetOwner(HWND owner) noexcept;
        void SetVisibility(const bool isVisible) noexcept;

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
        std::atomic<HWND> _owner{ HWND_DESKTOP };
        std::atomic<bool> _suppressVisibilityChange{ false };
        WRL::ComPtr<PseudoConsoleWindowAccessibilityProvider> _pPseudoConsoleUiaProvider{ nullptr };
    };
}
