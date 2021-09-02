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

#include "BaseApplicationState.h"
#include "ApplicationState.g.h"
#include <inc/cppwinrt_utils.h>

// This macro generates all getters and setters for ApplicationState.
// It provides X with the following arguments:
//   (type, function name, JSON key, ...variadic construction arguments)
#define MTSM_APPLICATION_STATE_FIELDS(X)                                       \
    X(std::unordered_set<winrt::guid>, GeneratedProfiles, "generatedProfiles") \
    X(Windows::Foundation::Collections::IVector<hstring>, RecentCommands, "recentCommands")

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ApplicationState : public BaseApplicationState, ApplicationStateT<ApplicationState>
    {
        static Microsoft::Terminal::Settings::Model::ApplicationState SharedInstance();

        ApplicationState(std::filesystem::path path) noexcept;

        virtual void FromJson(const Json::Value& root) const noexcept override;
        virtual Json::Value ToJson() const noexcept override;

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
        til::shared_mutex<state_t> _state;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ApplicationState);
}
