// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Rendering.h"
#include "Rendering.g.cpp"

using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Rendering::Rendering()
    {
        InitializeComponent();
    }

    void Rendering::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::RenderingViewModel>();

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("rendering", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }
}
