// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles.h"
#include "Profiles.g.cpp"
#include "EnumEntry.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::AccessCache;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles::Profiles()
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(CursorShape, CursorStyle, winrt::Microsoft::Terminal::TerminalControl::CursorStyle, L"Profile_CursorShape", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(BackgroundImageStretchMode, BackgroundImageStretchMode, winrt::Windows::UI::Xaml::Media::Stretch, L"Profile_BackgroundImageStretchMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(AntiAliasingMode, TextAntialiasingMode, winrt::Microsoft::Terminal::TerminalControl::TextAntialiasingMode, L"Profile_AntialiasingMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(CloseOnExitMode, CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, L"Profile_CloseOnExit", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(BellStyle, BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle, L"Profile_BellStyle", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ScrollState, ScrollbarState, winrt::Microsoft::Terminal::TerminalControl::ScrollbarState, L"Profile_ScrollbarVisibility", L"Content");
    }

    void Profiles::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ProfilePageNavigationState>();
    }
}
