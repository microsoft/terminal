// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Appearance.h"
#include "Profiles_Appearance.g.cpp"
#include "PreviewConnection.h"
#include "EnumEntry.h"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Appearance::Profiles_Appearance() :
        _previewControl{ Control::TermControl(Model::TerminalSettings{}, make<PreviewConnection>()) }
    {
        InitializeComponent();
        INITIALIZE_BINDABLE_ENUM_SETTING(ScrollState, ScrollbarState, winrt::Microsoft::Terminal::Control::ScrollbarState, L"Profile_ScrollbarVisibility", L"Content");

        _previewControl.IsEnabled(false);
        _previewControl.AllowFocusWhenDisabled(false);
        ControlPreview().Child(_previewControl);
    }

    void Profiles_Appearance::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ProfilePageNavigationState>();
    }

    void Profiles_Appearance::CreateUnfocusedAppearance_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _State.CreateUnfocusedAppearance();
    }

    void Profiles_Appearance::DeleteUnfocusedAppearance_Click(IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _State.DeleteUnfocusedAppearance();
    }
}
