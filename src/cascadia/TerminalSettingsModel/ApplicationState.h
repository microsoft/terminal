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
#include <til/mutex.h>
#include <til/throttled_func.h>
#include <JsonUtils.h>

// This macro generates all getters and setters for ApplicationState.
// It provides X with the following arguments:
//   (type, function name, JSON key, ...variadic construction arguments)
namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
#define MTSM_APPLICATION_STATE_FIELDS(X)                                                                                \
    X(std::unordered_set<winrt::guid>, GeneratedProfiles, "generatedProfiles")                                          \
    X(Windows::Foundation::Collections::IVector<Model::WindowLayout>, PersistedWindowLayouts, "persistedWindowLayouts") \
    X(Windows::Foundation::Collections::IVector<hstring>, RecentCommands, "recentCommands")                             \
    X(Windows::Foundation::Collections::IVector<winrt::Microsoft::Terminal::Settings::Model::InfoBarMessage>, DismissedMessages, "dismissedMessages")

    struct WindowLayout : WindowLayoutT<WindowLayout>
    {
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Model::ActionAndArgs>, TabLayout, nullptr);
        WINRT_PROPERTY(winrt::Windows::Foundation::IReference<Model::LaunchPosition>, InitialPosition, nullptr);
        WINRT_PROPERTY(winrt::Windows::Foundation::IReference<winrt::Windows::Foundation::Size>, InitialSize, nullptr);

        friend ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<Model::WindowLayout>;
    };

    struct ApplicationState : ApplicationStateT<ApplicationState>
    {
        static Microsoft::Terminal::Settings::Model::ApplicationState SharedInstance();

        ApplicationState(std::filesystem::path path) noexcept;
        ~ApplicationState();

        // Methods
        void Reload() const noexcept;

        // General getters/setters
        winrt::hstring FilePath() const noexcept;

        // State getters/setters
#define MTSM_APPLICATION_STATE_GEN(type, name, key, ...) \
    type name() const noexcept;                          \
    void name(const type& value) noexcept;
        MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN

    private:
        struct state_t
        {
#define MTSM_APPLICATION_STATE_GEN(type, name, key, ...) std::optional<type> name{ __VA_ARGS__ };
            MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
        };

        void _write() const noexcept;
        void _read() const noexcept;

        std::filesystem::path _path;
        til::shared_mutex<state_t> _state;
        til::throttled_func_trailing<> _throttler;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(WindowLayout)
    BASIC_FACTORY(ApplicationState);
}
