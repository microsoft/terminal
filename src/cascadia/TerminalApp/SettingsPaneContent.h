// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "winrt/TerminalApp.h"
#include <LibraryResources.h>
#include "BasicPaneEvents.h"

namespace winrt::TerminalApp::implementation
{
    class SettingsPaneContent : public winrt::implements<SettingsPaneContent, IPaneContent>, public BasicPaneEvents
    {
    public:
        SettingsPaneContent(winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings settings);

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();
        winrt::Microsoft::Terminal::Settings::Editor::MainPage SettingsUI() { return _sui; }

        winrt::Windows::Foundation::Size MinimumSize();
        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs GetNewTerminalArgs(const BuildStartupKind kind) const;

        winrt::hstring Title() { return RS_(L"SettingsTab"); }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept;
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        // See BasicPaneEvents for most generic event definitions

    private:
        winrt::Microsoft::Terminal::Settings::Editor::MainPage _sui{ nullptr };
        winrt::Windows::UI::Xaml::ElementTheme _requestedTheme;
    };
}
