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
    }

    bool AISettingsViewModel::AreAIKeyAndEndpointSet()
    {
        return !_Settings.AIKey().empty() && !_Settings.AIEndpoint().empty();
    }

    winrt::hstring AISettingsViewModel::AIEndpoint()
    {
        return _Settings.AIEndpoint();
    }

    void AISettingsViewModel::AIEndpoint(winrt::hstring endpoint)
    {
        _Settings.AIEndpoint(endpoint);
        _NotifyChanges(L"AreAIKeyAndEndpointSet");
    }

    winrt::hstring AISettingsViewModel::AIKey()
    {
        return _Settings.AIKey();
    }

    void AISettingsViewModel::AIKey(winrt::hstring key)
    {
        _Settings.AIKey(key);
        _NotifyChanges(L"AreAIKeyAndEndpointSet");
    }
}
