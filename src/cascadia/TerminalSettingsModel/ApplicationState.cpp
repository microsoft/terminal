// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ApplicationState.h"
#include "CascadiaSettings.h"
#include "ApplicationState.g.cpp"
#include "WindowLayout.g.cpp"
#include "ActionAndArgs.h"
#include "JsonUtils.h"
#include "FileUtils.h"

static constexpr std::wstring_view stateFileName{ L"state.json" };
static constexpr std::string_view TabLayoutKey{ "tabLayout" };
static constexpr std::string_view InitialPositionKey{ "initialPosition" };
static constexpr std::string_view InitialSizeKey{ "initialSize" };

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    using namespace winrt::Microsoft::Terminal::Settings::Model;

    template<>
    struct ConversionTrait<WindowLayout>
    {
        WindowLayout FromJson(const Json::Value& json)
        {
            auto layout = winrt::make_self<implementation::WindowLayout>();

            GetValueForKey(json, TabLayoutKey, layout->_TabLayout);
            GetValueForKey(json, InitialPositionKey, layout->_InitialPosition);
            GetValueForKey(json, InitialSizeKey, layout->_InitialSize);

            return *layout;
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isObject();
        }

        Json::Value ToJson(const WindowLayout& val)
        {
            Json::Value json{ Json::objectValue };

            SetValueForKey(json, TabLayoutKey, val.TabLayout());
            SetValueForKey(json, InitialPositionKey, val.InitialPosition());
            SetValueForKey(json, InitialSizeKey, val.InitialSize());

            return json;
        }

        std::string TypeDescription() const
        {
            return "WindowLayout";
        }
    };
}

using namespace ::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    winrt::hstring WindowLayout::ToJson(const Model::WindowLayout& layout)
    {
        JsonUtils::ConversionTrait<Model::WindowLayout> trait;
        auto json = trait.ToJson(layout);

        Json::StreamWriterBuilder wbuilder;
        const auto content = Json::writeString(wbuilder, json);
        return hstring{ til::u8u16(content) };
    }

    Model::WindowLayout WindowLayout::FromJson(const hstring& str)
    {
        auto data = til::u16u8(str);
        std::string errs;
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

        Json::Value root;
        if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
        {
            throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
        }
        JsonUtils::ConversionTrait<Model::WindowLayout> trait;
        return trait.FromJson(root);
    }

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
            state->name##Changed = true;                    \
        }                                                   \
                                                            \
        _throttler();                                       \
    }
    MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

    Json::Value ApplicationState::_getRoot(const locked_hfile& file) const noexcept
    {
        Json::Value root;
        try
        {
            const auto data = ReadUTF8FileLocked(file);
            if (data.empty())
            {
                return root;
            }

            std::string errs;
            std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

            if (!reader->parse(data.data(), data.data() + data.size(), &root, &errs))
            {
                throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
            }
        }
        CATCH_LOG()

        return root;
    }

    // Deserializes the state.json at _path into this ApplicationState.
    // * ANY errors during app state will result in the creation of a new empty state.
    // * ANY errors during runtime will result in changes being partially ignored.
    void ApplicationState::_read() const noexcept
    try
    {
        auto state = _state.lock();
        const auto file = OpenFileReadSharedLocked(_path);

        auto root = _getRoot(file);
        // GetValueForKey() comes in two variants:
        // * take a std::optional<T> reference
        // * return std::optional<T> by value
        // At the time of writing the former version skips missing fields in the json,
        // but we want to explicitly clear state fields that were removed from state.json.
#define MTSM_APPLICATION_STATE_GEN(type, name, key, ...)                         \
    if (!state->name##Changed)                                                   \
    {                                                                            \
        state->name = JsonUtils::GetValueForKey<std::optional<type>>(root, key); \
    }
        MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
    }
    CATCH_LOG()

    // Serialized this ApplicationState (in `context`) into the state.json at _path.
    // * Errors are only logged.
    // * _state->_writeScheduled is set to false, signaling our
    //   setters that _synchronize() needs to be called again.
    void ApplicationState::_write() noexcept
    try
    {
        // re-read the state so that we can only update the properties that were changed.
        Json::Value root{};
        {
            auto state = _state.lock();
            const auto file = OpenFileRWExclusiveLocked(_path);
            root = _getRoot(file);

#define MTSM_APPLICATION_STATE_GEN(type, name, key, ...)   \
    if (state->name##Changed)                              \
    {                                                      \
        JsonUtils::SetValueForKey(root, key, state->name); \
        state->name##Changed = false;                      \
    }
            MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

            Json::StreamWriterBuilder wbuilder;
            const auto content = Json::writeString(wbuilder, root);
            WriteUTF8FileLocked(file, content);
        }
    }
    CATCH_LOG()
}
