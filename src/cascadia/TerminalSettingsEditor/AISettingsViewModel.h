// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AISettingsViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct AISettingsViewModel : AISettingsViewModelT<AISettingsViewModel>, ViewModelHelper<AISettingsViewModel>
    {
    public:
        AISettingsViewModel(Model::CascadiaSettings settings);

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<AISettingsViewModel>::PropertyChanged;

        bool AreAzureOpenAIKeyAndEndpointSet();
        winrt::hstring AzureOpenAIEndpoint();
        void AzureOpenAIEndpoint(winrt::hstring endpoint);
        winrt::hstring AzureOpenAIKey();
        void AzureOpenAIKey(winrt::hstring key);
        bool AzureOpenAIActive();
        void AzureOpenAIActive(bool active);

        bool IsOpenAIKeySet();
        winrt::hstring OpenAIKey();
        void OpenAIKey(winrt::hstring key);
        bool OpenAIActive();
        void OpenAIActive(bool active);

        bool AreGithubCopilotTokensSet();
        void GithubCopilotAuthToken(winrt::hstring authToken);
        void GithubCopilotRefreshToken(winrt::hstring refreshToken);
        bool GithubCopilotActive();
        void GithubCopilotActive(bool active);
        bool GithubCopilotFeatureEnabled();
        void InitiateGithubAuth_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IInspectable> GithubAuthRequested;

    private:
        Model::CascadiaSettings _Settings;

        winrt::Microsoft::Terminal::Settings::Editor::MainPage::GithubAuthCompleted_revoker _githubAuthCompleteRevoker;

        void _OnGithubAuthCompleted();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(AISettingsViewModel);
}
