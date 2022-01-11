// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Advanced.h"
#include "Profiles_Advanced.g.cpp"

#include "EnumEntry.h"
#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Advanced::Profiles_Advanced()
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(AntiAliasingMode, TextAntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode, L"Profile_AntialiasingMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(CloseOnExitMode, CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode, L"Profile_CloseOnExit", L"Content");
    }

    void Profiles_Advanced::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::ProfilePageNavigationState>();

        // Subscribe to some changes in the view model
        // These changes should force us to update our own set of "Current<Setting>" members,
        // and propagate those changes to the UI
        _ViewModelChangedRevoker = _State.Profile().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
            const auto settingName{ args.PropertyName() };
            if (settingName == L"AntialiasingMode")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentAntiAliasingMode" });
            }
            else if (settingName == L"CloseOnExit")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCloseOnExitMode" });
            }
            else if (settingName == L"BellStyle")
            {
                _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsBellStyleFlagSet" });
            }
        });
    }

    void Profiles_Advanced::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }

    bool Profiles_Advanced::IsBellStyleFlagSet(const uint32_t flag)
    {
        return (WI_EnumValue(_State.Profile().BellStyle()) & flag) == flag;
    }

    void Profiles_Advanced::SetBellStyleAudible(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = State().Profile().BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Audible, winrt::unbox_value<bool>(on));
        State().Profile().BellStyle(currentStyle);
    }

    void Profiles_Advanced::SetBellStyleWindow(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = State().Profile().BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Window, winrt::unbox_value<bool>(on));
        State().Profile().BellStyle(currentStyle);
    }

    void Profiles_Advanced::SetBellStyleTaskbar(winrt::Windows::Foundation::IReference<bool> on)
    {
        auto currentStyle = State().Profile().BellStyle();
        WI_UpdateFlag(currentStyle, Model::BellStyle::Taskbar, winrt::unbox_value<bool>(on));
        State().Profile().BellStyle(currentStyle);
    }
}
