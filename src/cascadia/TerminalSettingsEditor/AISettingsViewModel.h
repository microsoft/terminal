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

        bool IsOpenAIKeySet();
        winrt::hstring OpenAIKey();
        void OpenAIKey(winrt::hstring key);

        bool AreGithubCopilotTokensSet();
        void GithubCopilotAuthToken(winrt::hstring authToken);
        void GithubCopilotRefreshToken(winrt::hstring refreshToken);

        bool AzureOpenAIIsActive();
        bool OpenAIIsActive();
        bool GithubCopilotIsActive();
        void InitiateGithubAuth_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        til::typed_event<Windows::Foundation::IInspectable, Windows::Foundation::IInspectable> GithubAuthRequested;

    private:
        Model::CascadiaSettings _Settings;
        bool _awaitingGithubAuth{ false };
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(AISettingsViewModel);
}
