// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "winrt/TerminalApp.h"
#include "BasicPaneEvents.h"

namespace winrt::TerminalApp::implementation
{
    class ScratchpadContent : public winrt::implements<ScratchpadContent, IPaneContent>, public BasicPaneEvents
    {
    public:
        ScratchpadContent();

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::Foundation::Size MinimumSize();

        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs GetNewTerminalArgs(BuildStartupKind kind) const;

        winrt::hstring Title() { return L"Scratchpad"; }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        // See BasicPaneEvents for most generic event definitions

    private:
        winrt::Windows::UI::Xaml::Controls::Grid _root{ nullptr };
        winrt::Windows::UI::Xaml::Controls::TextBox _box{ nullptr };
    };
}
