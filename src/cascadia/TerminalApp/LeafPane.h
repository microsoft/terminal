// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include <winrt/Microsoft.Terminal.TerminalControl.h>
#include "winrt/Microsoft.UI.Xaml.Controls.h"
#include <winrt/TerminalApp.h>
#include "../../cascadia/inc/cppwinrt_utils.h"

#include "LeafPane.g.h"

namespace winrt::TerminalApp::implementation
{
    struct LeafPane : LeafPaneT<LeafPane>
    {
    public:
        LeafPane();
        LeafPane(const GUID& profile,
                 const winrt::Microsoft::Terminal::TerminalControl::TermControl& control,
                 const bool lastFocused = false);

        winrt::Windows::UI::Xaml::Controls::Grid GetRootElement();

        LeafPane* GetActivePane();
        winrt::Microsoft::Terminal::TerminalControl::TermControl GetTerminalControl();
        GUID GetProfile();
        void FocusPane(uint32_t id);

        bool WasLastFocused() const noexcept;
        void UpdateVisuals();
        void ClearActive();
        void SetActive(const bool active);


    private:
        winrt::Microsoft::Terminal::TerminalControl::TermControl _control{ nullptr };
        GUID _profile;
        bool _lastActive{ false };
        winrt::Windows::UI::Xaml::Controls::Grid _root{};
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(LeafPane);
}
