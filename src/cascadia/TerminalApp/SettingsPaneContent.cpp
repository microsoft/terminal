// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsPaneContent.h"
#include "PaneArgs.h"
#include "SettingsPaneContent.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    SettingsPaneContent::SettingsPaneContent(CascadiaSettings settings)
    {
        _sui = winrt::Microsoft::Terminal::Settings::Editor::MainPage{ settings };
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
}
