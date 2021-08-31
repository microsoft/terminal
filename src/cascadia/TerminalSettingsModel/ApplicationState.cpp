// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ApplicationState.h"
#include "CascadiaSettings.h"
#include "ApplicationState.g.cpp"

#include "JsonUtils.h"
#include "FileUtils.h"

constexpr std::wstring_view stateFileName{ L"state.json" };

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
    // Returns the application-global ApplicationState object.
    Microsoft::Terminal::Settings::Model::ApplicationState ApplicationState::SharedInstance()
    {
        static auto state = winrt::make_self<ApplicationState>(GetBaseSettingsPath() / stateFileName);
        return *state;
    }

    ApplicationState::ApplicationState(std::filesystem::path path) noexcept :
        _path{ std::move(path) },
        _throttler{ std::chrono::seconds(1), [this]() { _write(); } }
    {
        _read();
    }

    // The destructor ensures that the last write is flushed to disk before returning.
    ApplicationState::~ApplicationState()
    {
        // This will ensure that we not just cancel the last outstanding timer,
        // but instead force it to run as soon as possible and wait for it to complete.
        _throttler.flush();
    }

    // Re-read the state.json from disk.
    void ApplicationState::Reload() const noexcept
    {
        _read();
    }

    // Returns the state.json path on the disk.
    winrt::hstring ApplicationState::FilePath() const noexcept
    {
        return winrt::hstring{ _path.wstring() };
    }

    // Generate all getter/setters
#define MTSM_APPLICATION_STATE_GEN(type, name, key, ...)    \
    type ApplicationState::name() const noexcept            \
    {                                                       \
        const auto state = _state.lock_shared();            \
        const auto& value = state->name;                    \
        return value ? *value : type{ __VA_ARGS__ };        \
    }                                                       \
                                                            \
    void ApplicationState::name(const type& value) noexcept \
    {                                                       \
        {                                                   \
            auto state = _state.lock();                     \
            state->name.emplace(value);                     \
        }                                                   \
                                                            \
        _throttler();                                       \
    }
    MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

    // Deserializes the state.json at _path into this ApplicationState.
    // * ANY errors during app state will result in the creation of a new empty state.
    // * ANY errors during runtime will result in changes being partially ignored.
    void ApplicationState::_read() const noexcept
    try
    {
        const auto data = ReadUTF8FileIfExists(_path).value_or(std::string{});
        if (data.empty())
        {
            return;
        }

        std::string errs;
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

        Json::Value root;
        if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
        {
            throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
        }

        auto state = _state.lock();
        // GetValueForKey() comes in two variants:
        // * take a std::optional<T> reference
        // * return std::optional<T> by value
        // At the time of writing the former version skips missing fields in the json,
        // but we want to explicitly clear state fields that were removed from state.json.
#define MTSM_APPLICATION_STATE_GEN(type, name, key, ...) state->name = JsonUtils::GetValueForKey<std::optional<type>>(root, key);
        MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
    }
    CATCH_LOG()

    // Serialized this ApplicationState (in `context`) into the state.json at _path.
    // * Errors are only logged.
    // * _state->_writeScheduled is set to false, signaling our
    //   setters that _synchronize() needs to be called again.
    void ApplicationState::_write() const noexcept
    try
    {
        Json::Value root{ Json::objectValue };

        {
            auto state = _state.lock_shared();
#define MTSM_APPLICATION_STATE_GEN(type, name, key, ...) JsonUtils::SetValueForKey(root, key, state->name);
            MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
        }

        Json::StreamWriterBuilder wbuilder;
        const auto content = Json::writeString(wbuilder, root);
        WriteUTF8FileAtomic(_path, content);
    }
    CATCH_LOG()
}
