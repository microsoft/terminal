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

    void AISettings::ClearAzureOpenAIKeyAndEndpoint_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.AzureOpenAIEndpoint(L"");
        _ViewModel.AzureOpenAIKey(L"");
    }

    void AISettings::StoreAzureOpenAIKeyAndEndpoint_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        // only store anything if both fields are filled
        if (!AzureOpenAIEndpointInputBox().Text().empty() && !AzureOpenAIKeyInputBox().Password().empty())
        {
            _ViewModel.AzureOpenAIEndpoint(AzureOpenAIEndpointInputBox().Text());
            _ViewModel.AzureOpenAIKey(AzureOpenAIKeyInputBox().Password());
            AzureOpenAIEndpointInputBox().Text(L"");
            AzureOpenAIKeyInputBox().Password(L"");

            TraceLoggingWrite(
                g_hSettingsEditorProvider,
                "AzureOpenAIEndpointAndKeySaved",
                TraceLoggingDescription("Event emitted when the user stores an Azure OpenAI key and endpoint"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }
    }

    void AISettings::ClearOpenAIKey_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.OpenAIKey(L"");
    }

    void AISettings::StoreOpenAIKey_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        if (!OpenAIKeyInputBox().Password().empty())
        {
            _ViewModel.OpenAIKey(OpenAIKeyInputBox().Password());
            OpenAIKeyInputBox().Password(L"");

            TraceLoggingWrite(
                g_hSettingsEditorProvider,
                "OpenAIEndpointAndKeySaved",
                TraceLoggingDescription("Event emitted when the user stores an OpenAI key and endpoint"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }
    }

    void AISettings::SetAzureOpenAIActive_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.AzureOpenAIActive(true);
    }

    void AISettings::SetOpenAIActive_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.OpenAIActive(true);
    }
}
