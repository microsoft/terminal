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
            const auto settings = winrt::get_self<implementation::ProfileViewModel>(_Profile)->TermSettings();
            _previewConnection->DisplayPowerlineGlyphs(_Profile.DefaultAppearance().HasPowerlineCharacters());
            _previewControl = Control::TermControl(settings, settings, *_previewConnection);
            _previewControl.IsEnabled(false);
            _previewControl.AllowFocusWhenDisabled(false);
            ControlPreview().Child(_previewControl);
        }

        // Subscribe to some changes in the view model
        // These changes should force us to update our own set of "Current<Setting>" members,
        // and propagate those changes to the UI
        _ViewModelChangedRevoker = _Profile.PropertyChanged(winrt::auto_revoke, { this, &Profiles_Appearance::_onProfilePropertyChanged });
        // The Appearances object handles updating the values in the settings UI, but
        // we still need to listen to the changes here just to update the preview control
        _AppearanceViewModelChangedRevoker = _Profile.DefaultAppearance().PropertyChanged(winrt::auto_revoke, { this, &Profiles_Appearance::_onProfilePropertyChanged });

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("profile.appearance", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingValue(_Profile.IsBaseLayer(), "IsProfileDefaults", "If the modified profile is the profile.defaults object"),
            TraceLoggingValue(static_cast<GUID>(_Profile.Guid()), "ProfileGuid", "The guid of the profile that was navigated to"),
            TraceLoggingValue(_Profile.Source().c_str(), "ProfileSource", "The source of the profile that was navigated to"),
            TraceLoggingValue(_Profile.DefaultAppearance().BackgroundImageSettingsVisible(), "HasBackgroundImage", "If the profile has a background image defined"),
            TraceLoggingValue(_Profile.HasUnfocusedAppearance(), "HasUnfocusedAppearance", "If the profile has an unfocused appearance defined"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void Profiles_Appearance::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
        _AppearanceViewModelChangedRevoker.revoke();
    }

    void Profiles_Appearance::CreateUnfocusedAppearance_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "CreateUnfocusedAppearance",
            TraceLoggingDescription("Event emitted when the user creates an unfocused appearance for a profile"),
            TraceLoggingValue(_Profile.IsBaseLayer(), "IsProfileDefaults", "If the modified profile is the profile.defaults object"),
            TraceLoggingValue(static_cast<GUID>(_Profile.Guid()), "ProfileGuid", "The guid of the profile that was navigated to"),
            TraceLoggingValue(_Profile.Source().c_str(), "ProfileSource", "The source of the profile that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

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
            _updatePreviewControl = std::make_shared<ThrottledFunc<>>(
                winrt::Windows::System::DispatcherQueue::GetForCurrentThread(),
                til::throttled_func_options{
                    .delay = std::chrono::milliseconds{ 100 },
                    .debounce = true,
                    .trailing = true,
                },
                [this]() {
                    const auto settings = winrt::get_self<implementation::ProfileViewModel>(_Profile)->TermSettings();
                    _previewConnection->DisplayPowerlineGlyphs(_Profile.DefaultAppearance().HasPowerlineCharacters());
                    _previewControl.UpdateControlSettings(settings, settings);
                });
        }

        _updatePreviewControl->Run();
    }
}
