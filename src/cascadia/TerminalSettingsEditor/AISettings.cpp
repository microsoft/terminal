// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AISettings.h"
#include "AISettings.g.cpp"

#include <LibraryResources.h>
#include <WtExeUtils.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    AISettings::AISettings()
    {
        InitializeComponent();

        auto disclaimerLinkText = Windows::UI::Xaml::Documents::Run();
        disclaimerLinkText.Text(RS_(L"AISettings_DisclaimerLink"));
        DisclaimerLink().Inlines().Append(disclaimerLinkText);

        auto prerequisite1LinkText = Windows::UI::Xaml::Documents::Run();
        prerequisite1LinkText.Text(RS_(L"AISettings_AzureOpenAIPrerequisite1Hyperlink"));
        Prerequisite1Hyperlink().Inlines().Append(prerequisite1LinkText);

        auto prerequisite3LinkText = Windows::UI::Xaml::Documents::Run();
        prerequisite3LinkText.Text(RS_(L"AISettings_AzureOpenAIPrerequisite3Hyperlink"));
        Prerequisite3Hyperlink().Inlines().Append(prerequisite3LinkText);

        auto productTermsLinkText = Windows::UI::Xaml::Documents::Run();
        productTermsLinkText.Text(RS_(L"AISettings_AzureOpenAIProductTermsHyperlink"));
        ProductTermsHyperlink().Inlines().Append(productTermsLinkText);
    }

    void AISettings::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::AISettingsViewModel>();

        TraceLoggingWrite(
            g_hSettingsEditorProvider,
            "AISettingsPageOpened",
            TraceLoggingDescription("Event emitted when the user navigates to the AI Settings page"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void AISettings::ClearKeyAndEndpoint_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.AIEndpoint(L"");
        _ViewModel.AIKey(L"");
    }

    void AISettings::StoreKeyAndEndpoint_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        // only store anything if both fields are filled
        if (!EndpointInputBox().Text().empty() && !KeyInputBox().Text().empty())
        {
            _ViewModel.AIEndpoint(EndpointInputBox().Text());
            _ViewModel.AIKey(KeyInputBox().Text());
            EndpointInputBox().Text(L"");
            KeyInputBox().Text(L"");

            TraceLoggingWrite(
                g_hSettingsEditorProvider,
                "AIEndpointAndKeySaved",
                TraceLoggingDescription("Event emitted when the user stores an AI key and endpoint"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }
    }
}
