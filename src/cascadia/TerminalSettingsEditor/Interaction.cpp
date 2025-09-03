// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Interaction.h"
#include "Interaction.g.cpp"

#include "EnumEntry.h"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Interaction::Interaction()
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(WarnAboutMultiLinePaste, WarnAboutMultiLinePaste, winrt::Microsoft::Terminal::Control::WarnAboutMultiLinePaste, L"Globals_WarnAboutMultiLinePaste", L"Content");
    }

    void Interaction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::InteractionViewModel>();

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("interaction", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }
}
