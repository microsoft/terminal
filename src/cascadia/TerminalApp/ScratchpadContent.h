// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "ScratchpadContent.g.h"

namespace winrt::TerminalApp::implementation
{
    struct ScratchpadContent : ScratchpadContentT<ScratchpadContent>
    {
        ScratchpadContent();

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::Foundation::Size MinSize();
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::NewTerminalArgs GetNewTerminalArgs(const bool asContent) const;

        winrt::hstring Title() { return L"Scratchpad"; }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        til::typed_event<> CloseRequested;
        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<> TitleChanged;
        til::typed_event<> TabColorChanged;
        til::typed_event<> TaskbarProgressChanged;
        til::typed_event<> ConnectionStateChanged;
        til::typed_event<> ReadOnlyChanged;
        til::typed_event<> FocusRequested;

    private:
        winrt::Windows::UI::Xaml::Controls::Grid _root{ nullptr };
        winrt::Windows::UI::Xaml::Controls::TextBox _box{ nullptr };
    };
}
