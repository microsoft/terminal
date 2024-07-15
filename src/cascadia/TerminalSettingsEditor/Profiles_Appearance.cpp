// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Appearance.h"

#include "ProfileViewModel.h"
#include "PreviewConnection.h"

#include "Profiles_Appearance.g.cpp"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Appearance::Profiles_Appearance()
    {
        InitializeComponent();
        _previewConnection = winrt::make_self<PreviewConnection>();
    }

    void Profiles_Appearance::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToProfileArgs>();
        _Profile = args.Profile();
        _windowRoot = args.WindowRoot();

        if (!_previewControl)
        {
            const auto settings = _Profile.TermSettings();
            _previewConnection->DisplayPowerlineGlyphs(_Profile.DefaultAppearance().HasPowerlineCharacters());
            _previewControl = Control::TermControl(settings, settings, *_previewConnection);
            _previewControl.IsEnabled(false);
            _previewControl.AllowFocusWhenDisabled(false);
            _previewControl.CursorVisibility(Microsoft::Terminal::Control::CursorDisplayState::Shown);
            ControlPreview().Child(_previewControl);
        }

        // Subscribe to some changes in the view model
        // These changes should force us to update our own set of "Current<Setting>" members,
        // and propagate those changes to the UI
        _ViewModelChangedRevoker = _Profile.PropertyChanged(winrt::auto_revoke, { this, &Profiles_Appearance::_onProfilePropertyChanged });
        // The Appearances object handles updating the values in the settings UI, but
        // we still need to listen to the changes here just to update the preview control
        _AppearanceViewModelChangedRevoker = _Profile.DefaultAppearance().PropertyChanged(winrt::auto_revoke, { this, &Profiles_Appearance::_onProfilePropertyChanged });
    }

    void Profiles_Appearance::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
        _AppearanceViewModelChangedRevoker.revoke();
    }

    void Profiles_Appearance::CreateUnfocusedAppearance_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _Profile.CreateUnfocusedAppearance();
    }

    void Profiles_Appearance::DeleteUnfocusedAppearance_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _Profile.DeleteUnfocusedAppearance();
    }

    void Profiles_Appearance::_onProfilePropertyChanged(const IInspectable&, const PropertyChangedEventArgs&)
    {
        if (!_updatePreviewControl)
        {
            _updatePreviewControl = std::make_shared<ThrottledFuncTrailing<>>(
                winrt::Windows::System::DispatcherQueue::GetForCurrentThread(),
                std::chrono::milliseconds{ 100 },
                [this]() {
                    const auto settings = _Profile.TermSettings();
                    _previewConnection->DisplayPowerlineGlyphs(_Profile.DefaultAppearance().HasPowerlineCharacters());
                    _previewControl.UpdateControlSettings(settings, settings);
                });
        }

        _updatePreviewControl->Run();
    }
}
