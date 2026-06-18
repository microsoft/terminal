// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_UnfocusedAppearance.h"
#include "Appearances.h"

#include "ProfileViewModel.h"
#include "PreviewConnection.h"

#include "Profiles_UnfocusedAppearance.g.cpp"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;

static constexpr std::wstring_view AppearanceSettingPrefix{ L"App." };

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_UnfocusedAppearance::Profiles_UnfocusedAppearance()
    {
        InitializeComponent();
        _previewConnection = winrt::make_self<PreviewConnection>();
    }

    void Profiles_UnfocusedAppearance::OnNavigatedTo(const NavigationEventArgs& e)
    {
        const auto args = e.Parameter().as<Editor::NavigateToPageArgs>();
        _Profile = args.ViewModel().as<Editor::ProfileViewModel>();
        _weakWindowRoot = args.WindowRoot();

        // Auto-create the unfocused appearance on navigate so the preview and editor are ready to go.
        if (!_Profile.HasUnfocusedAppearance())
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

        // Settings are stored in Profiles_UnfocusedAppearance and Appearances.
        // We use the "App." prefix to indicate if it's in Appearances,
        // and remove it on the way to Appearances object.
        const auto elementToFocus = args.ElementToFocus();
        if (elementToFocus.starts_with(AppearanceSettingPrefix))
        {
            std::wstring correctedName{ elementToFocus.c_str() };
            get_self<implementation::Appearances>(UnfocusedAppearanceView())->BringIntoViewWhenLoaded(hstring{ correctedName.substr(AppearanceSettingPrefix.size()) });
        }
        else
        {
            BringIntoViewWhenLoaded(elementToFocus);
        }

        if (!_previewControl)
        {
            const auto settings = winrt::get_self<implementation::ProfileViewModel>(_Profile)->TermSettingsUnfocused();
            _previewConnection->DisplayPowerlineGlyphs(_Profile.UnfocusedAppearance().HasPowerlineCharacters());
            _previewControl = Control::TermControl(settings, settings, *_previewConnection);
            _previewControl.CursorVisibility(Control::CursorDisplayState::Shown);
            _previewControl.IsEnabled(false);
            _previewControl.AllowFocusWhenDisabled(false);
            ControlPreview().Child(_previewControl);
        }

        // Subscribe to some changes in the view model
        // These changes should force us to update our own set of "Current<Setting>" members,
        // and propagate those changes to the UI
        _ViewModelChangedRevoker = _Profile.PropertyChanged(winrt::auto_revoke, { this, &Profiles_UnfocusedAppearance::_onProfilePropertyChanged });
        // The Appearances object handles updating the values in the settings UI, but
        // we still need to listen to the changes here just to update the preview control
        _AppearanceViewModelChangedRevoker = _Profile.UnfocusedAppearance().PropertyChanged(winrt::auto_revoke, { this, &Profiles_UnfocusedAppearance::_onProfilePropertyChanged });

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("profile.unfocusedAppearance", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingValue(_Profile.IsBaseLayer(), "IsProfileDefaults", "If the modified profile is the profile.defaults object"),
            TraceLoggingValue(static_cast<GUID>(_Profile.Guid()), "ProfileGuid", "The guid of the profile that was navigated to"),
            TraceLoggingValue(_Profile.Source().c_str(), "ProfileSource", "The source of the profile that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void Profiles_UnfocusedAppearance::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
        _AppearanceViewModelChangedRevoker.revoke();
    }

    void Profiles_UnfocusedAppearance::DeleteUnfocusedAppearance_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        // Stop listening to the unfocused appearance before it goes away.
        _AppearanceViewModelChangedRevoker.revoke();
        _Profile.DeleteUnfocusedAppearance();

        // Return to the base profile page now that there's no unfocused appearance to edit.
        _Profile.CurrentPage(ProfileSubPage::Base);
    }

    void Profiles_UnfocusedAppearance::_onProfilePropertyChanged(const IInspectable&, const PropertyChangedEventArgs&)
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
                    if (!_Profile.HasUnfocusedAppearance())
                    {
                        return;
                    }
                    const auto settings = winrt::get_self<implementation::ProfileViewModel>(_Profile)->TermSettingsUnfocused();
                    _previewConnection->DisplayPowerlineGlyphs(_Profile.UnfocusedAppearance().HasPowerlineCharacters());
                    _previewControl.UpdateControlSettings(settings, settings);
                });
        }

        _updatePreviewControl->Run();
    }
}
