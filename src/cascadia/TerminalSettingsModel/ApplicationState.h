/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ApplicationState.h

Abstract:
- If the CascadiaSettings class were AppData, then this class would be LocalAppData.
  Put anything in here that you wouldn't want to be stored next to user-editable settings.
- Modify ApplicationState.idl and MTSM_APPLICATION_STATE_FIELDS to add new fields.
--*/
#pragma once

#include "ApplicationState.g.h"
#include "WindowLayout.g.h"

#include <inc/cppwinrt_utils.h>
#include <JsonUtils.h>

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    // If a property is Shared, then it'll be stored in `state.json`, and used
    // in both elevated and unelevated instances of the Terminal. If a property
    // is marked Local, then it will have separate values for elevated and
    // unelevated instances.
    enum FileSource : int
    {
        Shared = 0x1,
        Local = 0x2
    };
    DEFINE_ENUM_FLAG_OPERATORS(FileSource);

// This macro generates all getters and setters for ApplicationState.
// It provides X with the following arguments:
//   (source, type, function name, JSON key, ...variadic construction arguments)
#define MTSM_APPLICATION_STATE_FIELDS(X)                                                                                                                                  \
    X(FileSource::Shared, winrt::hstring, SettingsHash, "settingsHash")                                                                                                   \
    X(FileSource::Shared, std::unordered_set<winrt::guid>, GeneratedProfiles, "generatedProfiles")                                                                        \
    X(FileSource::Local, Windows::Foundation::Collections::IVector<Model::WindowLayout>, PersistedWindowLayouts, "persistedWindowLayouts")                                \
    X(FileSource::Shared, Windows::Foundation::Collections::IVector<hstring>, RecentCommands, "recentCommands")                                                           \
    X(FileSource::Shared, Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::InfoBarMessage>, DismissedMessages, "dismissedMessages") \
    X(FileSource::Local, Windows::Foundation::Collections::IVector<hstring>, AllowedCommandlines, "allowedCommandlines")

    struct WindowLayout : WindowLayoutT<WindowLayout>
    {
        static winrt::hstring ToJson(const Model::WindowLayout& layout);
        static Model::WindowLayout FromJson(const winrt::hstring& json);

        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Model::ActionAndArgs>, TabLayout, nullptr);
        WINRT_PROPERTY(winrt::Windows::Foundation::IReference<Model::LaunchPosition>, InitialPosition, nullptr);
        WINRT_PROPERTY(winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Size>, InitialSize, nullptr);
        WINRT_PROPERTY(winrt::Windows::Foundation::IReference<Model::LaunchMode>, LaunchMode, nullptr);

        friend ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<Model::WindowLayout>;
    };

    struct ApplicationState : public ApplicationStateT<ApplicationState>
    {
        static Microsoft::Terminal::Settings::Model::ApplicationState SharedInstance();

        ApplicationState(const std::filesystem::path& stateRoot) noexcept;
        ~ApplicationState();

        // Methods
        void Reload() const noexcept;
        void Reset() noexcept;

        void FromJson(const Json::Value& root, FileSource parseSource) const noexcept;
        Json::Value ToJson(FileSource parseSource) const noexcept;

        // General getters/setters
        bool IsStatePath(const winrt::hstring& filename);

        // State getters/setters
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) \
    type name() const noexcept;                                  \
    void name(const type& value) noexcept;
        MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

    private:
        struct state_t
        {
#define MTSM_APPLICATION_STATE_GEN(source, type, name, key, ...) std::optional<type> name{ __VA_ARGS__ };
            MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
        };
        til::shared_mutex<state_t> _state;
        std::filesystem::path _sharedPath;
        std::filesystem::path _elevatedPath;
        til::throttled_func_trailing<> _throttler;

        void _write() const noexcept;
        void _read() const noexcept;

        Json::Value _toJsonWithBlob(Json::Value& root, FileSource parseSource) const noexcept;

        std::optional<std::string> _readSharedContents() const;
        void _writeSharedContents(const std::string_view content) const;
        std::optional<std::string> _readLocalContents() const;
        void _writeLocalContents(const std::string_view content) const;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(WindowLayout)
    BASIC_FACTORY(ApplicationState);
}
