// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ElevatedState.h"
#include "CascadiaSettings.h"
#include "ElevatedState.g.cpp"

#include "JsonUtils.h"
#include "FileUtils.h"

constexpr std::wstring_view stateFileName{ L"elevated-state.json" };

using namespace ::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    ElevatedState::ElevatedState(std::filesystem::path path) noexcept :
        BaseApplicationState{ path } {}

    // Returns the application-global ElevatedState object.
    Microsoft::Terminal::Settings::Model::ElevatedState ElevatedState::SharedInstance()
    {
        // TODO! place in a totally different file! and path!
        static auto state = winrt::make_self<ElevatedState>(GetBaseSettingsPath() / stateFileName);
        state->Reload();
        return *state;
    }

    void ElevatedState::FromJson(const Json::Value& root) const noexcept
    {
        auto state = _state.lock();
        // GetValueForKey() comes in two variants:
        // * take a std::optional<T> reference
        // * return std::optional<T> by value
        // At the time of writing the former version skips missing fields in the json,
        // but we want to explicitly clear state fields that were removed from state.json.
#define MTSM_ELEVATED_STATE_GEN(type, name, key, ...) state->name = JsonUtils::GetValueForKey<std::optional<type>>(root, key);
        MTSM_ELEVATED_STATE_FIELDS(MTSM_ELEVATED_STATE_GEN)
#undef MTSM_ELEVATED_STATE_GEN
    }
    Json::Value ElevatedState::ToJson() const noexcept
    {
        Json::Value root{ Json::objectValue };

        {
            auto state = _state.lock_shared();
#define MTSM_ELEVATED_STATE_GEN(type, name, key, ...) JsonUtils::SetValueForKey(root, key, state->name);
            MTSM_ELEVATED_STATE_FIELDS(MTSM_ELEVATED_STATE_GEN)
#undef MTSM_ELEVATED_STATE_GEN
        }
        return root;
    }

    // Generate all getter/setters
#define MTSM_ELEVATED_STATE_GEN(type, name, key, ...)    \
    type ElevatedState::name() const noexcept            \
    {                                                    \
        const auto state = _state.lock_shared();         \
        const auto& value = state->name;                 \
        return value ? *value : type{ __VA_ARGS__ };     \
    }                                                    \
                                                         \
    void ElevatedState::name(const type& value) noexcept \
    {                                                    \
        {                                                \
            auto state = _state.lock();                  \
            state->name.emplace(value);                  \
        }                                                \
                                                         \
        _throttler();                                    \
    }
    MTSM_ELEVATED_STATE_FIELDS(MTSM_ELEVATED_STATE_GEN)
#undef MTSM_ELEVATED_STATE_GEN

}
