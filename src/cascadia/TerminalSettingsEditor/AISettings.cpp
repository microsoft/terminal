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

        std::unordered_map<size_t, std::wstring> disclaimerPlaceholderToStringMap;
        disclaimerPlaceholderToStringMap.insert(std::pair<size_t, std::wstring>(0, RS_(L"AISettings_DisclaimerLinkText")));
        const auto disclaimerParts = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_Disclaimer").c_str(), disclaimerPlaceholderToStringMap);

        AISettings_DisclaimerPart1().Text(disclaimerParts.at(0));
        AISettings_DisclaimerLinkText().Text(disclaimerParts.at(1));
        AISettings_DisclaimerPart2().Text(disclaimerParts.at(2));

        std::unordered_map<size_t, std::wstring> prerequisite1PlaceholderToStringMap;
        prerequisite1PlaceholderToStringMap.insert(std::pair<size_t, std::wstring>(0, RS_(L"AISettings_AzureOpenAIPrerequisite1LinkText")));
        const auto prerequisite1Parts = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_AzureOpenAIPrerequisite1").c_str(), prerequisite1PlaceholderToStringMap);

        AISettings_AzureOpenAIPrerequisite1Part1().Text(prerequisite1Parts.at(0));
        AISettings_AzureOpenAIPrerequisite1LinkText().Text(prerequisite1Parts.at(1));
        AISettings_AzureOpenAIPrerequisite1Part2().Text(prerequisite1Parts.at(2));

        std::unordered_map<size_t, std::wstring> prerequisite3PlaceholderToStringMap;
        prerequisite3PlaceholderToStringMap.insert(std::pair<size_t, std::wstring>(0, RS_(L"AISettings_AzureOpenAIPrerequisite3LinkText")));
        const auto prerequisite3Parts = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_AzureOpenAIPrerequisite3").c_str(), prerequisite3PlaceholderToStringMap);

        AISettings_AzureOpenAIPrerequisite3Part1().Text(prerequisite3Parts.at(0));
        AISettings_AzureOpenAIPrerequisite3LinkText().Text(prerequisite3Parts.at(1));
        AISettings_AzureOpenAIPrerequisite3Part2().Text(prerequisite3Parts.at(2));

        std::unordered_map<size_t, std::wstring> productTermsPlaceholderToStringMap;
        productTermsPlaceholderToStringMap.insert(std::pair<size_t, std::wstring>(0, RS_(L"AISettings_AzureOpenAIProductTermsLinkText")));
        const auto productTermsParts = ::Microsoft::Console::Utils::SplitResourceStringWithPlaceholders(RS_(L"AISettings_AzureOpenAIProductTerms").c_str(), productTermsPlaceholderToStringMap);

        AISettings_AzureOpenAIProductTermsPart1().Text(productTermsParts.at(0));
        AISettings_AzureOpenAIProductTermsLinkText().Text(productTermsParts.at(1));
        AISettings_AzureOpenAIProductTermsPart2().Text(productTermsParts.at(2));
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
