// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AddProfile.h"
#include "AddProfile.g.cpp"
#include "AddProfilePageNavigationState.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    AddProfile::AddProfile()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"AddProfile_AddNewTextBlock/Text"));
        Automation::AutomationProperties::SetName(DuplicateButton(), RS_(L"AddProfile_DuplicateTextBlock/Text"));
    }

    void AddProfile::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _State = e.Parameter().as<Editor::AddProfilePageNavigationState>();

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "NavigatedToPage",
            TraceLoggingDescription("Event emitted when the user navigates to a page in the settings UI"),
            TraceLoggingValue("addProfile", "PageId", "The identifier of the page that was navigated to"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void AddProfile::AddNewClick(const IInspectable& /*sender*/,
                                 const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "AddNewProfile",
            TraceLoggingDescription("Event emitted when the user adds a new profile"),
            TraceLoggingValue("EmptyProfile", "Type", "The type of the creation method (i.e. empty profile, duplicate)"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        _State.RequestAddNew();
    }

    void AddProfile::DuplicateClick(const IInspectable& /*sender*/,
                                    const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        if (const auto selected = Profiles().SelectedItem())
        {
            const auto selectedProfile = selected.as<Model::Profile>();
            TraceLoggingWrite(
                g_hTerminalSettingsEditorProvider,
                "AddNewProfile",
                TraceLoggingDescription("Event emitted when the user adds a new profile"),
                TraceLoggingValue("Duplicate", "Type", "The type of the creation method (i.e. empty profile, duplicate)"),
                TraceLoggingValue(!selectedProfile.Source().empty(), "SourceProfileHasSource", "True, if the source profile has a source (i.e. dynamic profile generator namespace, fragment). Otherwise, False, indicating it's based on a custom profile."),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

            _State.RequestDuplicate(selectedProfile.Guid());
        }
    }

    void AddProfile::ProfilesSelectionChanged(const IInspectable& /*sender*/,
                                              const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        if (!_IsProfileSelected)
        {
            IsProfileSelected(true);
        }
    }
}
