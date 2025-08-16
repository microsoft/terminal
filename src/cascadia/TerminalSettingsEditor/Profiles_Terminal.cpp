// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Profiles_Terminal.h"
#include "Profiles_Terminal.g.cpp"
#include "ProfileViewModel.h"

#include "EnumEntry.h"
#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Profiles_Terminal::Profiles_Terminal()
    {
        InitializeComponent();
    }

    void Profiles_Terminal::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _Profile = e.Parameter().as<Editor::ProfileViewModel>();

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("profile.terminal", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingValue(_Profile.IsBaseLayer(), "IsProfileDefaults", "If the modified profile is the profile.defaults object"),
            TraceLoggingValue(static_cast<GUID>(_Profile.Guid()), "ProfileGuid", "The guid of the profile that was navigated to"),
            TraceLoggingValue(_Profile.Source().c_str(), "ProfileSource", "The source of the profile that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void Profiles_Terminal::OnNavigatedFrom(const NavigationEventArgs& /*e*/)
    {
        _ViewModelChangedRevoker.revoke();
    }
}
