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
static constexpr std::wstring_view PasswordVaultGithubCopilotAuthToken = L"TerminalGithubCopilotAuthToken";
static constexpr std::wstring_view PasswordVaultGithubCopilotRefreshToken = L"TerminalGithubCopilotRefreshToken";

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

winrt::hstring AIConfig::AzureOpenAIEndpoint() noexcept
{
    PasswordVault vault;
    PasswordCredential cred;
    // Retrieve throws an exception if there are no credentials stored under the given resource so we wrap it in a try-catch block
    try
    {
        cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultAIEndpoint);
    }
    catch (...)
    {
        return L"";
    }
    return cred.Password();
}

void AIConfig::AzureOpenAIEndpoint(const winrt::hstring& endpoint) noexcept
{
    PasswordVault vault;
    if (endpoint.empty())
    {
        // an empty string indicates that we should clear the key
        PasswordCredential cred;
        try
        {
            cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultAIEndpoint);
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
        PasswordCredential newCredential{ PasswordVaultResourceName, PasswordVaultAIEndpoint, endpoint };
        vault.Add(newCredential);
    }
}

winrt::hstring AIConfig::AzureOpenAIKey() noexcept
{
    PasswordVault vault;
    PasswordCredential cred;
    // Retrieve throws an exception if there are no credentials stored under the given resource so we wrap it in a try-catch block
    try
    {
        cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultAIKey);
    }
    catch (...)
    {
        return L"";
    }
    return cred.Password();
}

void AIConfig::AzureOpenAIKey(const winrt::hstring& key) noexcept
{
    PasswordVault vault;
    if (key.empty())
    {
        // the user has entered an empty string, that indicates that we should clear the key
        PasswordCredential cred;
        try
        {
            cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultAIKey);
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
        PasswordCredential newCredential{ PasswordVaultResourceName, PasswordVaultAIKey, key };
        vault.Add(newCredential);
    }
}

winrt::hstring AIConfig::OpenAIKey() noexcept
{
    PasswordVault vault;
    PasswordCredential cred;
    // Retrieve throws an exception if there are no credentials stored under the given resource so we wrap it in a try-catch block
    try
    {
        cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultOpenAIKey);
    }
    catch (...)
    {
        return L"";
    }
    return cred.Password();
}

void AIConfig::OpenAIKey(const winrt::hstring& key) noexcept
{
    PasswordVault vault;
    if (key.empty())
    {
        // the user has entered an empty string, that indicates that we should clear the key
        PasswordCredential cred;
        try
        {
            cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultOpenAIKey);
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
        PasswordCredential newCredential{ PasswordVaultResourceName, PasswordVaultOpenAIKey, key };
        vault.Add(newCredential);
    }
}

winrt::hstring AIConfig::GithubCopilotAuthToken() noexcept
{
    PasswordVault vault;
    PasswordCredential cred;
    // Retrieve throws an exception if there are no credentials stored under the given resource so we wrap it in a try-catch block
    try
    {
        cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultGithubCopilotAuthToken);
    }
    catch (...)
    {
        return L"";
    }
    return cred.Password();
}

void AIConfig::GithubCopilotAuthToken(const winrt::hstring& authToken) noexcept
{
    PasswordVault vault;
    if (authToken.empty())
    {
        // the user has entered an empty string, that indicates that we should clear the token
        PasswordCredential cred;
        try
        {
            cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultGithubCopilotAuthToken);
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
        PasswordCredential newCredential{ PasswordVaultResourceName, PasswordVaultGithubCopilotAuthToken, authToken };
        vault.Add(newCredential);
    }
}

winrt::hstring AIConfig::GithubCopilotRefreshToken() noexcept
{
    PasswordVault vault;
    PasswordCredential cred;
    // Retrieve throws an exception if there are no credentials stored under the given resource so we wrap it in a try-catch block
    try
    {
        cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultGithubCopilotRefreshToken);
    }
    catch (...)
    {
        return L"";
    }
    return cred.Password();
}

void AIConfig::GithubCopilotRefreshToken(const winrt::hstring& refreshToken) noexcept
{
    PasswordVault vault;
    if (refreshToken.empty())
    {
        // the user has entered an empty string, that indicates that we should clear the token
        PasswordCredential cred;
        try
        {
            cred = vault.Retrieve(PasswordVaultResourceName, PasswordVaultGithubCopilotRefreshToken);
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
        PasswordCredential newCredential{ PasswordVaultResourceName, PasswordVaultGithubCopilotRefreshToken, refreshToken };
        vault.Add(newCredential);
    }
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
