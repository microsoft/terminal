// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "OceanContent.g.h"

namespace winrt::TerminalApp::implementation
{
    struct OceanContent : OceanContentT<OceanContent>
    {
        OceanContent();

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::Foundation::Size MinSize();
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetNewTerminalArgs(const bool asContent) const;

        winrt::hstring Title() { return L"Xaml Ocean"; }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        void SetHostingWindow(uint64_t hostingWindow) noexcept;

        til::typed_event<> CloseRequested;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<> TitleChanged;
        til::typed_event<> TabColorChanged;
        til::typed_event<> TaskbarProgressChanged;
        til::typed_event<> ReadOnlyChanged;
        til::typed_event<> FocusRequested;

    private:
        winrt::Windows::UI::Xaml::Controls::Grid _root{ nullptr };

        wil::unique_hwnd _window;
        HWND _hostingHwnd;
        winrt::Windows::UI::Xaml::Controls::Grid::SizeChanged_revoker _sizeChangedRevoker;

        void _createOcean();
        void _resizeToMatch();

        LRESULT _messageHandler(UINT const message, WPARAM const wparam, LPARAM const lparam);
        static LRESULT __stdcall s_WndProc(HWND const window, UINT const message, WPARAM const wparam, LPARAM const lparam) noexcept;
    };
}
