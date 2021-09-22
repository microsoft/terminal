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
#include "../../types/inc/utils.hpp"

static constexpr std::wstring_view stateFileName{ L"state.json" };
static constexpr std::wstring_view elevatedStateFileName{ L"elevated-state.json" };

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
    ApplicationState::ApplicationState(std::filesystem::path sharedPath,
                                       std::filesystem::path elevatedPath) noexcept :
        _sharedPath{ std::move(sharedPath) },
        _elevatedPath{ std::move(elevatedPath) },
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
        return winrt::hstring{ _sharedPath.wstring() };
    }

    // TODO!
    // Deserializes the state.json at _path into this ApplicationState.
    // * ANY errors during app state will result in the creation of a new empty state.
    // * ANY errors during runtime will result in changes being partially ignored.
    void ApplicationState::_read() const noexcept
    try
    {
        std::string errs;
        std::unique_ptr<Json::CharReader> reader{ Json::CharReaderBuilder::CharReaderBuilder().newCharReader() };

        const auto sharedData = _readSharedContents().value_or(std::string{});
        if (!sharedData.empty())
        {
            Json::Value root;
            if (!reader->parse(sharedData.data(), sharedData.data() + sharedData.size(), &root, &errs))
            {
                throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
            }

            FromJson(root, FileSource::Shared);
        }
        if (::Microsoft::Console::Utils::IsElevated())
        {
            if (const auto elevatedData{ _readElevatedContents().value_or(std::string{}) }; !elevatedData.empty())
            {
                Json::Value root;
                if (!reader->parse(elevatedData.data(), elevatedData.data() + elevatedData.size(), &root, &errs))
                {
                    throw winrt::hresult_error(WEB_E_INVALID_JSON_STRING, winrt::to_hstring(errs));
                }

                FromJson(root, FileSource::Local);
            }
        }
    }
    CATCH_LOG()

    // Serialized this ApplicationState (in `context`) into the state.json at _path.
    // * Errors are only logged.
    // * _state->_writeScheduled is set to false, signaling our
    //   setters that _synchronize() needs to be called again.
    void ApplicationState::_write() const noexcept
    try
    {
        Json::StreamWriterBuilder wbuilder;

        _writeSharedContents(Json::writeString(wbuilder, ToJson(FileSource::Shared)));
        _writeElevatedContents(Json::writeString(wbuilder, ToJson(FileSource::Local)));
    }
    CATCH_LOG()

    // Returns the application-global ApplicationState object.
    Microsoft::Terminal::Settings::Model::ApplicationState ApplicationState::SharedInstance()
    {
        static auto state = winrt::make_self<ApplicationState>(GetBaseSettingsPath() / stateFileName,
                                                               GetBaseSettingsPath() / elevatedStateFileName);
        return *state;
    }

    void ApplicationState::FromJson(const Json::Value& root, FileSource parseSource) const noexcept
    {
        auto state = _state.lock();
        // GetValueForKey() comes in two variants:
        // * take a std::optional<T> reference
        // * return std::optional<T> by value
        // At the time of writing the former version skips missing fields in the json,
        // but we want to explicitly clear state fields that were removed from state.json.
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) \
    if (parseSource == source)                                   \
        state->name = JsonUtils::GetValueForKey<std::optional<type>>(root, key);

        MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
    }

    Json::Value ApplicationState::ToJson(FileSource parseSource) const noexcept
    {
        Json::Value root{ Json::objectValue };

        {
            auto state = _state.lock_shared();
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) \
    if (parseSource == source)                                   \
        JsonUtils::SetValueForKey(root, key, state->name);

            MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
        }
        return root;
    }

    // Generate all getter/setters
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) \
    type ApplicationState::name() const noexcept                 \
    {                                                            \
        const auto state = _state.lock_shared();                 \
        const auto& value = state->name;                         \
        return value ? *value : type{ __VA_ARGS__ };             \
    }                                                            \
                                                                 \
    void ApplicationState::name(const type& value) noexcept      \
    {                                                            \
        {                                                        \
            auto state = _state.lock();                          \
            state->name.emplace(value);                          \
        }                                                        \
                                                                 \
        _throttler();                                            \
    }
    MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

    std::optional<std::string> ApplicationState::_readSharedContents() const
    {
        return ReadUTF8FileIfExists(_sharedPath);
    }

    std::optional<std::string> ApplicationState::_readElevatedContents() const
    {
        return ::Microsoft::Console::Utils::IsElevated() ? ReadUTF8FileIfExists(_elevatedPath, true) : std::nullopt;
    }

    void ApplicationState::_writeSharedContents(const std::string_view content) const
    {
        WriteUTF8FileAtomic(_sharedPath, content);
    }

    void ApplicationState::_writeElevatedContents(const std::string_view content) const
    {
        if (::Microsoft::Console::Utils::IsElevated())
        {
            // DON'T use WriteUTF8FileAtomic, which will write to a temporary file
            // then rename that file to the final filename. That actually lets us
            // overwrite the elevate file's contents even when unelevated, because
            // we're effectively deleting the original file, then renaming a
            // different file in it's place.
            //
            // We're not worried about someone else doing that though, if they do
            // that with the wrong permissions, then we'll just ignore the file and
            // start over.
            WriteUTF8File(_elevatedPath, content, true);
        }
        else
        {
            // do nothing
        }
    }

}
