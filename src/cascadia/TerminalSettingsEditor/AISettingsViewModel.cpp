// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AISettingsViewModel.h"
#include "AISettingsViewModel.g.cpp"
#include "EnumEntry.h"

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
    AISettingsViewModel::AISettingsViewModel(Model::CascadiaSettings settings) :
        _Settings{ settings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(ActiveProvider, LLMProvider, Model::LLMProvider, L"Globals_LLMProvider", L"Content");
    }

    bool AISettingsViewModel::AreAzureOpenAIKeyAndEndpointSet()
    {
        return !_Settings.AIKey().empty() && !_Settings.AIEndpoint().empty();
    }

    winrt::hstring AISettingsViewModel::AzureOpenAIEndpoint()
    {
        return _Settings.AIEndpoint();
    }

    void AISettingsViewModel::AzureOpenAIEndpoint(winrt::hstring endpoint)
    {
        _Settings.AIEndpoint(endpoint);
        _NotifyChanges(L"AreAzureOpenAIKeyAndEndpointSet");
    }

    winrt::hstring AISettingsViewModel::AzureOpenAIKey()
    {
        return _Settings.AIKey();
    }

    void AISettingsViewModel::AzureOpenAIKey(winrt::hstring key)
    {
        _Settings.AIKey(key);
        _NotifyChanges(L"AreAzureOpenAIKeyAndEndpointSet");
    }

    bool AISettingsViewModel::IsOpenAIKeySet()
    {
        return !_Settings.OpenAIKey().empty();
    }

    winrt::hstring AISettingsViewModel::OpenAIKey()
    {
        return _Settings.OpenAIKey();
    }

    void AISettingsViewModel::OpenAIKey(winrt::hstring key)
    {
        _Settings.OpenAIKey(key);
        _NotifyChanges(L"IsOpenAIKeySet");
    }
}
