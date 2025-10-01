// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Compatibility.h"
#include "EnumEntry.h"
#include "Compatibility.g.cpp"
#include "CompatibilityViewModel.g.cpp"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    CompatibilityViewModel::CompatibilityViewModel(Model::CascadiaSettings settings) :
        _settings{ settings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(TextMeasurement, TextMeasurement, winrt::Microsoft::Terminal::Control::TextMeasurement, L"Globals_TextMeasurement_", L"Text");
    }

    bool CompatibilityViewModel::DebugFeaturesAvailable() const noexcept
    {
        return Feature_DebugModeUI::IsEnabled();
    }

    void CompatibilityViewModel::ResetApplicationState()
    {
        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "ResetApplicationState",
            TraceLoggingDescription("Event emitted when the user resets their application state"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        _settings.ResetApplicationState();
    }

    void CompatibilityViewModel::ResetToDefaultSettings()
    {
        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "ResetToDefaultSettings",
            TraceLoggingDescription("Event emitted when the user resets their settings to their default value"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        _settings.ResetToDefaultSettings();
    }

    Compatibility::Compatibility()
    {
        InitializeComponent();
    }

    void Compatibility::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::CompatibilityViewModel>();

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("compatibility", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void Compatibility::ResetApplicationStateButton_Click(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        _ViewModel.ResetApplicationState();
        ResetCacheFlyout().Hide();
    }
}
