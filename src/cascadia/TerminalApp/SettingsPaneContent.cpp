// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsPaneContent.h"
#include "PaneArgs.h"
#include "SettingsPaneContent.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

#define ASSERT_UI_THREAD() assert(_sui.Dispatcher().HasThreadAccess())

namespace winrt::TerminalApp::implementation
{
    SettingsPaneContent::SettingsPaneContent(CascadiaSettings settings)
    {
        _sui = winrt::Microsoft::Terminal::Settings::Editor::MainPage{ settings };
    }

    void SettingsPaneContent::UpdateSettings(const CascadiaSettings& settings)
    {
        ASSERT_UI_THREAD();
        _sui.UpdateSettings(settings);

        // Stash away the current requested theme of the app. We'll need that in
        // _BackgroundBrush() to do a theme-aware resource lookup
        // _requestedTheme = settings.GlobalSettings().CurrentTheme().RequestedTheme();
    }

    winrt::Windows::UI::Xaml::FrameworkElement SettingsPaneContent::GetRoot()
    {
        return _sui;
    }
    winrt::Windows::Foundation::Size SettingsPaneContent::MinSize()
    {
        return { 1, 1 };
    }
    void SettingsPaneContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        if (reason != FocusState::Unfocused)
        {
            _sui.as<Controls::Page>().Focus(reason);
        }
    }
    void SettingsPaneContent::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    NewTerminalArgs SettingsPaneContent::GetNewTerminalArgs(const bool /* asContent */) const
    {
        // TODO! hey, can we somehow replicate std::vector<ActionAndArgs> SettingsTab::BuildStartupActions?
        return nullptr;
    }

    winrt::hstring SettingsPaneContent::Icon() const
    {
        // This is the Setting icon (looks like a gear)
        static constexpr std::wstring_view glyph{ L"\xE713" };
        return winrt::hstring{ glyph };
    }
}
