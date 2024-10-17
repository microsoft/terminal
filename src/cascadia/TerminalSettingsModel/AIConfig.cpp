// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AIConfig.h"
#include "AIConfig.g.cpp"

#include "TerminalSettingsSerializationHelpers.h"
#include "JsonUtils.h"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::Security::Credentials;

static constexpr std::string_view AIConfigKey{ "aiConfig" };
static constexpr std::wstring_view PasswordVaultResourceName = L"TerminalAI";
static constexpr std::wstring_view PasswordVaultAIKey = L"TerminalAIKey";
static constexpr std::wstring_view PasswordVaultAIEndpoint = L"TerminalAIEndpoint";
static constexpr std::wstring_view PasswordVaultOpenAIKey = L"TerminalOpenAIKey";

winrt::com_ptr<AIConfig> AIConfig::CopyAIConfig(const AIConfig* source)
{
    auto aiConfig{ winrt::make_self<AIConfig>() };

#define AI_SETTINGS_COPY(type, name, jsonKey, ...) \
    aiConfig->_##name = source->_##name;
    MTSM_AI_SETTINGS(AI_SETTINGS_COPY)
#undef AI_SETTINGS_COPY

    return aiConfig;
}

Json::Value AIConfig::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

#define AI_SETTINGS_TO_JSON(type, name, jsonKey, ...) \
    JsonUtils::SetValueForKey(json, jsonKey, _##name);
    MTSM_AI_SETTINGS(AI_SETTINGS_TO_JSON)
#undef AI_SETTINGS_TO_JSON

    return json;
}

void AIConfig::LayerJson(const Json::Value& json)
{
    const auto aiConfigJson = json[JsonKey(AIConfigKey)];

#define AI_SETTINGS_LAYER_JSON(type, name, jsonKey, ...) \
    JsonUtils::GetValueForKey(aiConfigJson, jsonKey, _##name);
    MTSM_AI_SETTINGS(AI_SETTINGS_LAYER_JSON)
#undef AI_SETTINGS_LAYER_JSON
}

static winrt::event<winrt::Microsoft::Terminal::Settings::Model::AzureOpenAISettingChangedHandler> _azureOpenAISettingChangedHandlers;

winrt::event_token AIConfig::AzureOpenAISettingChanged(const winrt::Microsoft::Terminal::Settings::Model::AzureOpenAISettingChangedHandler& handler) { return _azureOpenAISettingChangedHandlers.add(handler); };
void AIConfig::AzureOpenAISettingChanged(const winrt::event_token& token) { _azureOpenAISettingChangedHandlers.remove(token); };

winrt::hstring AIConfig::AzureOpenAIEndpoint() noexcept
{
    return _RetrieveCredential(PasswordVaultAIEndpoint);
}

void AIConfig::AzureOpenAIEndpoint(const winrt::hstring& endpoint) noexcept
{
    _SetCredential(PasswordVaultAIEndpoint, endpoint);
    _azureOpenAISettingChangedHandlers();
}

winrt::hstring AIConfig::AzureOpenAIKey() noexcept
{
    return _RetrieveCredential(PasswordVaultAIKey);
}

void AIConfig::AzureOpenAIKey(const winrt::hstring& key) noexcept
{
    _SetCredential(PasswordVaultAIKey, key);
    _azureOpenAISettingChangedHandlers();
}

static winrt::event<winrt::Microsoft::Terminal::Settings::Model::OpenAISettingChangedHandler> _openAISettingChangedHandlers;

winrt::event_token AIConfig::OpenAISettingChanged(const winrt::Microsoft::Terminal::Settings::Model::OpenAISettingChangedHandler& handler) { return _openAISettingChangedHandlers.add(handler); };
void AIConfig::OpenAISettingChanged(const winrt::event_token& token) { _openAISettingChangedHandlers.remove(token); };

winrt::hstring AIConfig::OpenAIKey() noexcept
{
    return _RetrieveCredential(PasswordVaultOpenAIKey);
}

void AIConfig::OpenAIKey(const winrt::hstring& key) noexcept
{
    _SetCredential(PasswordVaultOpenAIKey, key);
    _openAISettingChangedHandlers();
}

winrt::Microsoft::Terminal::Settings::Model::LLMProvider AIConfig::ActiveProvider()
{
    const auto val{ _getActiveProviderImpl() };
    if (val)
    {
        // an active provider was explicitly set, return that
        return *val;
    }
    else if (!AzureOpenAIEndpoint().empty() && !AzureOpenAIKey().empty())
    {
        // no explicitly set provider but we have an azure open ai key and endpoint, use that
        return LLMProvider::AzureOpenAI;
    }
    else if (!OpenAIKey().empty())
    {
        // no explicitly set provider but we have an open ai key, use that
        return LLMProvider::OpenAI;
    }
    else
    {
        return LLMProvider{};
    }
}

void AIConfig::ActiveProvider(const LLMProvider& provider)
{
    _ActiveProvider = provider;
}

winrt::hstring AIConfig::_RetrieveCredential(const std::wstring_view credential)
{
    PasswordVault vault;
    PasswordCredential cred;
    // Retrieve throws an exception if there are no credentials stored under the given resource so we wrap it in a try-catch block
    try
    {
        cred = vault.Retrieve(PasswordVaultResourceName, credential);
    }
    catch (...)
    {
        return L"";
    }
    return cred.Password();
}

void AIConfig::_SetCredential(const std::wstring_view credential, const winrt::hstring& value)
{
    PasswordVault vault;
    if (value.empty())
    {
        // the user has entered an empty string, that indicates that we should clear the value
        PasswordCredential cred;
        try
        {
            cred = vault.Retrieve(PasswordVaultResourceName, credential);
        }
        catch (...)
        {
            // there was nothing to remove, just return
            return;
        }
        vault.Remove(cred);
    }
    else
    {
        PasswordCredential newCredential{ PasswordVaultResourceName, credential, value };
        vault.Add(newCredential);
    }
}
