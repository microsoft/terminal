// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

//
// TODO! This is me trying to merge 5ff9a24 and 75e2b5f and LAWD it's
// impossible. But we don't really care, because we're gonna just toss out the
// locking from #11083 and instead move ApplicationState to the Monarch (in the
// future)
//

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

    ApplicationState::ApplicationState(const std::filesystem::path& stateRoot) noexcept :
        _sharedPath{ stateRoot / stateFileName },
        _userPath{ stateRoot / unelevatedStateFileName },
        _elevatedPath{ stateRoot / elevatedStateFileName },
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

    bool ApplicationState::IsStatePath(const winrt::hstring& filename)
    {
        static const auto sharedPath{ _sharedPath.filename() };
        static const auto elevatedPath{ _elevatedPath.filename() };
        static const auto userPath{ _userPath.filename() };
        return filename == sharedPath || filename == elevatedPath || filename == userPath;
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
            state->name##Changed = true;                         \
        }                                                        \
                                                                 \
        _throttler();                                            \
    }
    MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

    // Method Description:
    // - Read the contents of our "shared" state - state that should be shared
    //   for elevated and unelevated instances. This is things like the list of
    //   generated profiles, the command palette commandlines.
    std::optional<std::string> ApplicationState::_readSharedContents() const
    {
        const auto file = OpenFileReadSharedLocked(_sharedPath);

        return ReadUTF8FileLocked(file);
    }

    // Method Description:
    // - Read the contents of our "local" state - state that should be kept in
    //   separate files for elevated and unelevated instances. This is things
    //   like the persisted window state, and the approved commandlines (though,
    //   those don't matter when unelevated).
    // - When elevated, this will DELETE `elevated-state.json` if it has bad
    //   permissions, so we don't potentially read malicious data.
    std::optional<std::string> ApplicationState::_readLocalContents() const
    {
        const auto elevated{ ::Microsoft::Console::Utils::IsElevated() };
        const auto file = OpenFileReadSharedLocked(elevated ? _elevatedPath : _userPath);
        return ReadUTF8FileLocked(file, elevated);
    }

    // Method Description:
    // - Write the contents of our "shared" state - state that should be shared
    //   for elevated and unelevated instances. This will atomically write to
    //   `state.json`
    void ApplicationState::_writeSharedContents(const std::string_view content) const
    {
        WriteUTF8FileAtomic(_sharedPath, content);
    }

    // Method Description:
    // - Write the contents of our "local" state - state that should be kept in
    //   separate files for elevated and unelevated instances. When elevated,
    //   this will write to `elevated-state.json`, and when unelevated, this
    //   will atomically write to `user-state.json`
    void ApplicationState::_writeLocalContents(const std::string_view content) const
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
            WriteUTF8FileAtomic(_userPath, content);
        }
    }

    Json::Value ApplicationState::_getRoot(const std::string& data) const noexcept
    {
        Json::Value root;
        try
        {
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

        // First get shared state out of `state.json` into us
        const auto sharedData = _readSharedContents().value_or(std::string{});
        if (!sharedData.empty())
        {
            FromJson(_getRoot(sharedData), FileSource::Shared);
        }
        // Then, try and get anything in user-state/elevated-state
        if (const auto localData{ _readLocalContents().value_or(std::string{}) }; !localData.empty())
        {
            FromJson(_getRoot(localData), FileSource::Local);
        }
    }
    CATCH_LOG()

    // Serialized this ApplicationState (in `context`) into the state.json at _path.
    // * Errors are only logged.
    // * _state->_writeScheduled is set to false, signaling our
    //   setters that _synchronize() needs to be called again.
    void ApplicationState::_write() noexcept
    try
    {

<<<<<<< HEAD
        Json::StreamWriterBuilder wbuilder;

        _writeSharedContents(Json::writeString(wbuilder, ToJson(FileSource::Shared)));
        _writeLocalContents(Json::writeString(wbuilder, ToJson(FileSource::Local)));
=======
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
>>>>>>> origin/main
    }
    CATCH_LOG()

    // Method Description:
    // - Loads data from the given json blob. Will only read the data that's in
    //   the specified parseSource - so if we're reading the Local state file,
    //   we won't destroy previously parsed Shared data.
    // - READ: there's no layering for app state.
    void ApplicationState::FromJson(const Json::Value& root, FileSource parseSource) const noexcept
    {
        auto state = _state.lock();
        // GetValueForKey() comes in two variants:
        // * take a std::optional<T> reference
        // * return std::optional<T> by value
        // At the time of writing the former version skips missing fields in the json,
        // but we want to explicitly clear state fields that were removed from state.json.
        //
        // Only parse this property if:
        // * it is in the source we're looking for (so if we're looking for
        //   Shared settings, ignore Local ones) (GH#11222)
        // * the property hasn't changed since the last read (GH#11083)
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) \
    if (parseSource == source && !state->name##Changed)          \
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
    if (WI_IsFlagSet(parseSource, source))                       \
        JsonUtils::SetValueForKey(root, key, state->name);

            MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
        }
        return root;
    }
}
