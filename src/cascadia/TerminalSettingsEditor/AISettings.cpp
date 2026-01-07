// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AISettings.h"
#include "AISettings.g.cpp"
#include "..\types\inc\utils.hpp"

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

        std::array<std::wstring, 1> disclaimerPlaceholders{ RS_(L"AISettings_DisclaimerLinkText").c_str() };
        std::span<std::wstring> disclaimerPlaceholdersSpan{ disclaimerPlaceholders };
        const auto disclaimerParts = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_Disclaimer"), disclaimerPlaceholdersSpan);

        AISettings_DisclaimerPart1().Text(disclaimerParts.at(0));
        AISettings_DisclaimerLinkText().Text(disclaimerParts.at(1));
        AISettings_DisclaimerPart2().Text(disclaimerParts.at(2));

        std::array<std::wstring, 1> prerequisite1Placeholders{ RS_(L"AISettings_AzureOpenAIPrerequisite1LinkText").c_str() };
        std::span<std::wstring> prerequisite1PlaceholdersSpan{ prerequisite1Placeholders };
        const auto prerequisite1Parts = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_AzureOpenAIPrerequisite1"), prerequisite1PlaceholdersSpan);

        AISettings_AzureOpenAIPrerequisite1Part1().Text(prerequisite1Parts.at(0));
        AISettings_AzureOpenAIPrerequisite1LinkText().Text(prerequisite1Parts.at(1));
        AISettings_AzureOpenAIPrerequisite1Part2().Text(prerequisite1Parts.at(2));

        std::array<std::wstring, 1> prerequisite3Placeholders{ RS_(L"AISettings_AzureOpenAIPrerequisite3LinkText").c_str() };
        std::span<std::wstring> prerequisite3PlaceholdersSpan{ prerequisite3Placeholders };
        const auto prerequisite3Parts = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_AzureOpenAIPrerequisite3"), prerequisite3PlaceholdersSpan);

        AISettings_AzureOpenAIPrerequisite3Part1().Text(prerequisite3Parts.at(0));
        AISettings_AzureOpenAIPrerequisite3LinkText().Text(prerequisite3Parts.at(1));
        AISettings_AzureOpenAIPrerequisite3Part2().Text(prerequisite3Parts.at(2));

        std::array<std::wstring, 1> productTermsPlaceholders{ RS_(L"AISettings_AzureOpenAIProductTermsLinkText").c_str() };
        std::span<std::wstring> productTermsPlaceholdersSpan{ productTermsPlaceholders };
        const auto productTermsParts = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_AzureOpenAIProductTerms"), productTermsPlaceholdersSpan);

        AISettings_AzureOpenAIProductTermsPart1().Text(productTermsParts.at(0));
        AISettings_AzureOpenAIProductTermsLinkText().Text(productTermsParts.at(1));
        AISettings_AzureOpenAIProductTermsPart2().Text(productTermsParts.at(2));

        std::array<std::wstring, 1> openAIDescriptionPlaceholders{ RS_(L"AISettings_OpenAILearnMoreLinkText").c_str() };
        std::span<std::wstring> openAIDescriptionPlaceholdersSpan{ openAIDescriptionPlaceholders };
        const auto openAIDescription = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_OpenAIDescription"), openAIDescriptionPlaceholdersSpan);

        AISettings_OpenAIDescriptionPart1().Text(openAIDescription.at(0));
        AISettings_OpenAIDescriptionLinkText().Text(openAIDescription.at(1));
        AISettings_OpenAIDescriptionPart2().Text(openAIDescription.at(2));

        std::array<std::wstring, 2> githubCopilotDescriptionPlaceholders{ RS_(L"AISettings_GithubCopilotSignUpLinkText").c_str(), RS_(L"AISettings_GithubCopilotLearnMoreLinkText").c_str() };
        std::span<std::wstring> githubCopilotDescriptionPlaceholdersSpan{ githubCopilotDescriptionPlaceholders };
        const auto githubCopilotDescription = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_GithubCopilotSignUpAndLearnMore"), githubCopilotDescriptionPlaceholdersSpan);

        AISettings_GithubCopilotSignUpAndLearnMorePart1().Text(githubCopilotDescription.at(0));
        AISettings_GithubCopilotSignUpLinkText().Text(githubCopilotDescription.at(1));
        AISettings_GithubCopilotSignUpAndLearnMorePart2().Text(githubCopilotDescription.at(2));
        AISettings_GithubCopilotLearnMoreLinkText().Text(githubCopilotDescription.at(3));
        AISettings_GithubCopilotSignUpAndLearnMorePart3().Text(githubCopilotDescription.at(4));

        Automation::AutomationProperties::SetName(AzureOpenAIEndpointInputBox(), RS_(L"AISettings_EndpointTextBox/Text"));
        Automation::AutomationProperties::SetName(AzureOpenAIKeyInputBox(), RS_(L"AISettings_KeyTextBox/Text"));
        Automation::AutomationProperties::SetName(OpenAIKeyInputBox(), RS_(L"AISettings_KeyTextBox/Text"));
    }

    void AISettings::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::AISettingsViewModel>();

        TraceLoggingWrite(
            g_hTerminalSettingsEditorProvider,
            "AISettingsPageOpened",
            TraceLoggingDescription("Event emitted when the user navigates to the AI Settings page"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void AISettings::ClearAzureOpenAIKeyAndEndpoint_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.AzureOpenAIEndpoint(winrt::hstring{});
        _ViewModel.AzureOpenAIKey(winrt::hstring{});
    }

    void AISettings::StoreAzureOpenAIKeyAndEndpoint_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        // only store anything if both fields are filled
        if (!AzureOpenAIEndpointInputBox().Text().empty() && !AzureOpenAIKeyInputBox().Password().empty())
        {
            _ViewModel.AzureOpenAIEndpoint(AzureOpenAIEndpointInputBox().Text());
            _ViewModel.AzureOpenAIKey(AzureOpenAIKeyInputBox().Password());
            AzureOpenAIEndpointInputBox().Text(winrt::hstring{});
            AzureOpenAIKeyInputBox().Password(winrt::hstring{});

            TraceLoggingWrite(
                g_hTerminalSettingsEditorProvider,
                "AzureOpenAIEndpointAndKeySaved",
                TraceLoggingDescription("Event emitted when the user stores an Azure OpenAI key and endpoint"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }
    }

    void AISettings::ClearOpenAIKey_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.OpenAIKey(winrt::hstring{});
    }

    void AISettings::StoreOpenAIKey_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        const auto password = OpenAIKeyInputBox().Password();
        if (!password.empty())
        {
            _ViewModel.OpenAIKey(password);
            OpenAIKeyInputBox().Password(winrt::hstring{});

            TraceLoggingWrite(
                g_hTerminalSettingsEditorProvider,
                "OpenAIEndpointAndKeySaved",
                TraceLoggingDescription("Event emitted when the user stores an OpenAI key and endpoint"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }
    }

    void AISettings::ClearGithubCopilotTokens_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.GithubCopilotAuthValues(winrt::hstring{});
    }

    void AISettings::SetAzureOpenAIActive_Check(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.AzureOpenAIActive(true);
    }

    void AISettings::SetAzureOpenAIActive_Uncheck(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.AzureOpenAIActive(false);
    }

    void AISettings::SetOpenAIActive_Check(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.OpenAIActive(true);
    }

    void AISettings::SetOpenAIActive_Uncheck(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.OpenAIActive(false);
    }

    void AISettings::SetGithubCopilotActive_Check(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.GithubCopilotActive(true);
    }

    void AISettings::SetGithubCopilotActive_Uncheck(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.GithubCopilotActive(false);
    }
}
