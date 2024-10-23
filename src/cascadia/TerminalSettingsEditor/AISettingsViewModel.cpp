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

static constexpr std::wstring_view lockGlyph{ L"\uE72E" };

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
        _NotifyChanges(L"AreAzureOpenAIKeyAndEndpointSet", L"AzureOpenAIStatus");
    }

    winrt::hstring AISettingsViewModel::AzureOpenAIKey()
    {
        return _Settings.GlobalSettings().AIInfo().AzureOpenAIKey();
    }

    void AISettingsViewModel::AzureOpenAIKey(winrt::hstring key)
    {
        _Settings.GlobalSettings().AIInfo().AzureOpenAIKey(key);
        _NotifyChanges(L"AreAzureOpenAIKeyAndEndpointSet", L"AzureOpenAIStatus");
    }

    bool AISettingsViewModel::AzureOpenAIAllowed() const noexcept
    {
        return WI_IsFlagSet(AIConfig::AllowedLMProviders(), EnabledLMProviders::AzureOpenAI);
    }

    winrt::hstring AISettingsViewModel::AzureOpenAIStatus()
    {
        return _getStatusHelper(Model::LLMProvider::AzureOpenAI);
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
        _NotifyChanges(L"IsOpenAIKeySet", L"OpenAIStatus");
    }

    bool AISettingsViewModel::OpenAIAllowed() const noexcept
    {
        return WI_IsFlagSet(AIConfig::AllowedLMProviders(), EnabledLMProviders::OpenAI);
    }

    winrt::hstring AISettingsViewModel::OpenAIStatus()
    {
        return _getStatusHelper(Model::LLMProvider::OpenAI);
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
        }
        else
        {
            _Settings.GlobalSettings().AIInfo().ActiveProvider(Model::LLMProvider::None);
        }
        _NotifyChanges(L"AzureOpenAIActive", L"OpenAIActive", L"GithubCopilotActive", L"AzureOpenAIStatus", L"OpenAIStatus", L"GithubCopilotStatus");
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
        }
        else
        {
            _Settings.GlobalSettings().AIInfo().ActiveProvider(Model::LLMProvider::None);
        }
        _NotifyChanges(L"AzureOpenAIActive", L"OpenAIActive", L"GithubCopilotActive", L"AzureOpenAIStatus", L"OpenAIStatus", L"GithubCopilotStatus");
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
        _NotifyChanges(L"AreGithubCopilotTokensSet", L"GithubCopilotStatus");
    }

    void AISettingsViewModel::GithubCopilotRefreshToken(winrt::hstring refreshToken)
    {
        _Settings.GlobalSettings().AIInfo().GithubCopilotRefreshToken(refreshToken);
        _NotifyChanges(L"AreGithubCopilotTokensSet", L"GithubCopilotStatus");
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
        }
        else
        {
            _Settings.GlobalSettings().AIInfo().ActiveProvider(Model::LLMProvider::None);
        }
        _NotifyChanges(L"AzureOpenAIActive", L"OpenAIActive", L"GithubCopilotActive", L"AzureOpenAIStatus", L"OpenAIStatus", L"GithubCopilotStatus");
    }

    bool AISettingsViewModel::GithubCopilotAllowed() const noexcept
    {
        return Feature_GithubCopilot::IsEnabled() && WI_IsFlagSet(AIConfig::AllowedLMProviders(), EnabledLMProviders::GithubCopilot);
    }

    winrt::hstring AISettingsViewModel::GithubCopilotStatus()
    {
        return _getStatusHelper(Model::LLMProvider::GithubCopilot);
    }

    void AISettingsViewModel::InitiateGithubAuth_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _githubCopilotAuthMessage = RS_(L"AISettings_WaitingForGithubAuth");
        _NotifyChanges(L"GithubCopilotAuthMessage");
        GithubAuthRequested.raise(nullptr, nullptr);
        TraceLoggingWrite(
            g_hSettingsEditorProvider,
            "GithubAuthInitiated",
            TraceLoggingDescription("Event emitted when the user clicks the button to initiate the GitHub auth flow"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_CRITICAL_DATA),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    void AISettingsViewModel::_OnGithubAuthCompleted(const winrt::hstring& message)
    {
        _githubCopilotAuthMessage = message;
        _NotifyChanges(L"AreGithubCopilotTokensSet", L"GithubCopilotAuthMessage", L"GithubCopilotStatus");
    }

    winrt::hstring AISettingsViewModel::_getStatusHelper(const Model::LLMProvider provider)
    {
        bool allowed;
        bool active;
        bool loggedIn;
        switch (provider)
        {
        case LLMProvider::AzureOpenAI:
            allowed = AzureOpenAIAllowed();
            active = AzureOpenAIActive();
            loggedIn = AreAzureOpenAIKeyAndEndpointSet();
            break;
        case LLMProvider::OpenAI:
            allowed = OpenAIAllowed();
            active = OpenAIActive();
            loggedIn = IsOpenAIKeySet();
            break;
        case LLMProvider::GithubCopilot:
            allowed = GithubCopilotAllowed();
            active = GithubCopilotActive();
            loggedIn = AreGithubCopilotTokensSet();
            break;
        default:
            return L"";
        }

        if (!allowed)
        {
            // not allowed, display the lock glyph
            return winrt::hstring{ lockGlyph } + L" " + RS_(L"AISettings_ProviderNotAllowed");
        }
        else if (active && loggedIn)
        {
            // active provider and logged in
            return RS_(L"AISettings_ActiveLoggedIn");
        }
        else if (active)
        {
            // active provider, not logged in
            return RS_(L"AISettings_Active");
        }
        else if (loggedIn)
        {
            // logged in, not active provider
            return RS_(L"AISettings_LoggedIn");
        }
        return L"";
    }
}
