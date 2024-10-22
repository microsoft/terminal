// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AISettingsViewModel.h"
#include "AISettingsViewModel.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>
#include <WtExeUtils.h>
#include <shellapi.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::Foundation::Collections;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    AISettingsViewModel::AISettingsViewModel(Model::CascadiaSettings settings) :
        _Settings{ settings }
    {
        _githubAuthCompleteRevoker = MainPage::GithubAuthCompleted(winrt::auto_revoke, { this, &AISettingsViewModel::_OnGithubAuthCompleted });
    }

    bool AISettingsViewModel::AreAzureOpenAIKeyAndEndpointSet()
    {
        return !_Settings.GlobalSettings().AIInfo().AzureOpenAIKey().empty() && !_Settings.GlobalSettings().AIInfo().AzureOpenAIEndpoint().empty();
    }

    winrt::hstring AISettingsViewModel::AzureOpenAIEndpoint()
    {
        return _Settings.GlobalSettings().AIInfo().AzureOpenAIEndpoint();
    }

    void AISettingsViewModel::AzureOpenAIEndpoint(winrt::hstring endpoint)
    {
        _Settings.GlobalSettings().AIInfo().AzureOpenAIEndpoint(endpoint);
        _NotifyChanges(L"AreAzureOpenAIKeyAndEndpointSet");
    }

    winrt::hstring AISettingsViewModel::AzureOpenAIKey()
    {
        return _Settings.GlobalSettings().AIInfo().AzureOpenAIKey();
    }

    void AISettingsViewModel::AzureOpenAIKey(winrt::hstring key)
    {
        _Settings.GlobalSettings().AIInfo().AzureOpenAIKey(key);
        _NotifyChanges(L"AreAzureOpenAIKeyAndEndpointSet");
    }

    bool AISettingsViewModel::AzureOpenAIAllowed() const noexcept
    {
        return WI_IsFlagSet(_Settings.GlobalSettings().AIInfo().AllowedLMProviders(), Model::EnabledLMProviders::AzureOpenAI);
    }

    bool AISettingsViewModel::IsOpenAIKeySet()
    {
        return !_Settings.GlobalSettings().AIInfo().OpenAIKey().empty();
    }

    winrt::hstring AISettingsViewModel::OpenAIKey()
    {
        return _Settings.GlobalSettings().AIInfo().OpenAIKey();
    }

    void AISettingsViewModel::OpenAIKey(winrt::hstring key)
    {
        _Settings.GlobalSettings().AIInfo().OpenAIKey(key);
        _NotifyChanges(L"IsOpenAIKeySet");
    }

    bool AISettingsViewModel::OpenAIAllowed() const noexcept
    {
        return WI_IsFlagSet(_Settings.GlobalSettings().AIInfo().AllowedLMProviders(), Model::EnabledLMProviders::OpenAI);
    }

    bool AISettingsViewModel::AzureOpenAIActive()
    {
        return _Settings.GlobalSettings().AIInfo().ActiveProvider() == Model::LLMProvider::AzureOpenAI;
    }

    void AISettingsViewModel::AzureOpenAIActive(bool active)
    {
        if (active)
        {
            _Settings.GlobalSettings().AIInfo().ActiveProvider(Model::LLMProvider::AzureOpenAI);
            _NotifyChanges(L"AzureOpenAIActive", L"OpenAIActive", L"GithubCopilotActive");
        }
    }

    bool AISettingsViewModel::OpenAIActive()
    {
        return _Settings.GlobalSettings().AIInfo().ActiveProvider() == Model::LLMProvider::OpenAI;
    }

    void AISettingsViewModel::OpenAIActive(bool active)
    {
        if (active)
        {
            _Settings.GlobalSettings().AIInfo().ActiveProvider(Model::LLMProvider::OpenAI);
            _NotifyChanges(L"AzureOpenAIActive", L"OpenAIActive", L"GithubCopilotActive");
        }
    }

    bool AISettingsViewModel::AreGithubCopilotTokensSet()
    {
        return !_Settings.GlobalSettings().AIInfo().GithubCopilotAuthToken().empty() && !_Settings.GlobalSettings().AIInfo().GithubCopilotRefreshToken().empty();
    }

    winrt::hstring AISettingsViewModel::GithubCopilotAuthMessage()
    {
        return _githubCopilotAuthMessage;
    }

    void AISettingsViewModel::GithubCopilotAuthToken(winrt::hstring authToken)
    {
        _Settings.GlobalSettings().AIInfo().GithubCopilotAuthToken(authToken);
        _NotifyChanges(L"AreGithubCopilotTokensSet");
    }

    void AISettingsViewModel::GithubCopilotRefreshToken(winrt::hstring refreshToken)
    {
        _Settings.GlobalSettings().AIInfo().GithubCopilotRefreshToken(refreshToken);
        _NotifyChanges(L"AreGithubCopilotTokensSet");
    }

    bool AISettingsViewModel::GithubCopilotActive()
    {
        return _Settings.GlobalSettings().AIInfo().ActiveProvider() == Model::LLMProvider::GithubCopilot;
    }

    void AISettingsViewModel::GithubCopilotActive(bool active)
    {
        if (active)
        {
            _Settings.GlobalSettings().AIInfo().ActiveProvider(Model::LLMProvider::GithubCopilot);
            _NotifyChanges(L"AzureOpenAIActive", L"OpenAIActive", L"GithubCopilotActive");
        }
    }

    bool AISettingsViewModel::GithubCopilotAllowed() const noexcept
    {
        return Feature_GithubCopilot::IsEnabled() && WI_IsFlagSet(_Settings.GlobalSettings().AIInfo().AllowedLMProviders(), Model::EnabledLMProviders::GithubCopilot);
    }

    void AISettingsViewModel::InitiateGithubAuth_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _githubCopilotAuthMessage = RS_(L"AISettings_WaitingForGithubAuth");
        _NotifyChanges(L"GithubCopilotAuthMessage");
        GithubAuthRequested.raise(nullptr, nullptr);
        TraceLoggingWrite(
            g_hSettingsEditorProvider,
            "GithubAuthInitiated",
            TraceLoggingDescription("Event emitted when the user clicks the button to initiate the Github auth flow"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void AISettingsViewModel::_OnGithubAuthCompleted(const winrt::hstring& message)
    {
        _githubCopilotAuthMessage = message;
        _NotifyChanges(L"AreGithubCopilotTokensSet", L"GithubCopilotAuthMessage");
    }
}
