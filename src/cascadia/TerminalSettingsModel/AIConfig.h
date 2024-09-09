/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AIConfig

Abstract:
- The implementation of the AIConfig winrt class. Provides settings related
  to the AI settings of the terminal

Author(s):
- Pankaj Bhojwani - June 2024

--*/

#pragma once

#include "pch.h"
#include "AIConfig.g.h"
#include "IInheritable.h"
#include "JsonUtils.h"
#include "MTSMSettings.h"
#include <DefaultSettings.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct AIConfig : AIConfigT<AIConfig>, IInheritable<AIConfig>
    {
    public:
        AIConfig() = default;
        static winrt::com_ptr<AIConfig> CopyAIConfig(const AIConfig* source);
        Json::Value ToJson() const;
        void LayerJson(const Json::Value& json);

        // Key and endpoint storage
        // These are not written to the json, they are stored in the Windows Security Storage Vault
        winrt::hstring AzureOpenAIEndpoint() noexcept;
        void AzureOpenAIEndpoint(const winrt::hstring& endpoint) noexcept;
        winrt::hstring AzureOpenAIKey() noexcept;
        void AzureOpenAIKey(const winrt::hstring& key) noexcept;
        static winrt::event_token AzureOpenAISettingChanged(const winrt::Microsoft::Terminal::Settings::Model::AzureOpenAISettingChangedHandler& handler);
        static void AzureOpenAISettingChanged(const winrt::event_token& token);

        winrt::hstring OpenAIKey() noexcept;
        void OpenAIKey(const winrt::hstring& key) noexcept;
        static winrt::event_token OpenAISettingChanged(const winrt::Microsoft::Terminal::Settings::Model::OpenAISettingChangedHandler& handler);
        static void OpenAISettingChanged(const winrt::event_token& token);

        // we cannot just use INHERITABLE_SETTING here because we try to be smart about what the ActiveProvider is
        // i.e. even if there's no ActiveProvider explicitly set, if there's only the key stored for one of the providers
        // then that is the active one
        LLMProvider ActiveProvider();
        void ActiveProvider(const LLMProvider& provider);
        _BASE_INHERITABLE_SETTING(Model::AIConfig, std::optional<LLMProvider>, ActiveProvider);

    private:
        winrt::hstring _RetrieveCredential(const std::wstring_view credential);
        void _SetCredential(const std::wstring_view credential, const winrt::hstring& value);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(AIConfig);
}
