// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ElevatedState.h"
#include "CascadiaSettings.h"
#include "ElevatedState.g.cpp"

#include "JsonUtils.h"
#include "FileUtils.h"

constexpr std::wstring_view stateFileName{ L"elevated-state.json" };

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    // This trait exists in order to serialize the std::unordered_set for GeneratedProfiles.
    template<typename T>
    struct ConversionTrait<std::unordered_set<T>>
    {
        std::unordered_set<T> FromJson(const Json::Value& json) const
        {
            ConversionTrait<T> trait;
            std::unordered_set<T> val;
            val.reserve(json.size());

            for (const auto& element : json)
            {
                val.emplace(trait.FromJson(element));
            }

            return val;
        }

        bool CanConvert(const Json::Value& json) const
        {
            ConversionTrait<T> trait;
            return json.isArray() && std::all_of(json.begin(), json.end(), [trait](const auto& json) -> bool { return trait.CanConvert(json); });
        }

        Json::Value ToJson(const std::unordered_set<T>& val)
        {
            ConversionTrait<T> trait;
            Json::Value json{ Json::arrayValue };

            for (const auto& key : val)
            {
                json.append(trait.ToJson(key));
            }

            return json;
        }

        std::string TypeDescription() const
        {
            return fmt::format("{}[]", ConversionTrait<GUID>{}.TypeDescription());
        }
    };

    template<typename T>
    struct ConversionTrait<winrt::Windows::Foundation::Collections::IVector<T>>
    {
        winrt::Windows::Foundation::Collections::IVector<T> FromJson(const Json::Value& json) const
        {
            ConversionTrait<T> trait;
            std::vector<T> val;
            val.reserve(json.size());

            for (const auto& element : json)
            {
                val.push_back(trait.FromJson(element));
            }

            return winrt::single_threaded_vector(move(val));
        }

        bool CanConvert(const Json::Value& json) const
        {
            ConversionTrait<T> trait;
            return json.isArray() && std::all_of(json.begin(), json.end(), [trait](const auto& json) -> bool { return trait.CanConvert(json); });
        }

        Json::Value ToJson(const winrt::Windows::Foundation::Collections::IVector<T>& val)
        {
            ConversionTrait<T> trait;
            Json::Value json{ Json::arrayValue };

            for (const auto& key : val)
            {
                json.append(trait.ToJson(key));
            }

            return json;
        }

        std::string TypeDescription() const
        {
            return fmt::format("vector ({})", ConversionTrait<T>{}.TypeDescription());
        }
    };
}
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
