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

    bool AISettingsViewModel::AreGithubCopilotTokensSet()
    {
        return !_Settings.GlobalSettings().AIInfo().GithubCopilotAuthToken().empty() && !_Settings.GlobalSettings().AIInfo().GithubCopilotRefreshToken().empty();
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

    bool AISettingsViewModel::AzureOpenAIIsActive()
    {
        return _Settings.GlobalSettings().AIInfo().ActiveProvider() == Model::LLMProvider::AzureOpenAI;
    }

    bool AISettingsViewModel::OpenAIIsActive()
    {
        return _Settings.GlobalSettings().AIInfo().ActiveProvider() == Model::LLMProvider::OpenAI;
    }

    bool AISettingsViewModel::GithubCopilotIsActive()
    {
        return _Settings.GlobalSettings().AIInfo().ActiveProvider() == Model::LLMProvider::GithubCopilot;
    }

    void AISettingsViewModel::InitiateGithubAuth_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _awaitingGithubAuth = true;
        GithubAuthRequested.raise(nullptr, nullptr);
    }
}
