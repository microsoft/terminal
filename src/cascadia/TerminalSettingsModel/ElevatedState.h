/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- ElevatedState.h

Abstract:
- If the CascadiaSettings class were AppData, then this class would be LocalAppData.
  Put anything in here that you wouldn't want to be stored next to user-editable settings.
- Modify ElevatedState.idl and MTSM_ELEVATED_STATE_FIELDS to add new fields.
--*/
#pragma once

#include "BaseApplicationState.h"
#include "ElevatedState.g.h"
#include <inc/cppwinrt_utils.h>

// This macro generates all getters and setters for ElevatedState.
// It provides X with the following arguments:
//   (type, function name, JSON key, ...variadic construction arguments)
#define MTSM_ELEVATED_STATE_FIELDS(X) \
    X(Windows::Foundation::Collections::IVector<hstring>, AllowedCommandlines, "allowedCommandlines")

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ElevatedState : ElevatedStateT<ElevatedState>, public BaseApplicationState
    {
        static Microsoft::Terminal::Settings::Model::ElevatedState SharedInstance();

        ElevatedState(std::filesystem::path path) noexcept;

        void FromJson(const Json::Value& root) const noexcept override;
        Json::Value ToJson() const noexcept override;

        // State getters/setters
#define MTSM_ELEVATED_STATE_GEN(type, name, key, ...) \
    type name() const noexcept;                       \
    void name(const type& value) noexcept;
        MTSM_ELEVATED_STATE_FIELDS(MTSM_ELEVATED_STATE_GEN)
#undef MTSM_ELEVATED_STATE_GEN

    private:
        struct state_t
        {
#define MTSM_ELEVATED_STATE_GEN(type, name, key, ...) std::optional<type> name{ __VA_ARGS__ };
            MTSM_ELEVATED_STATE_FIELDS(MTSM_ELEVATED_STATE_GEN)
#undef MTSM_ELEVATED_STATE_GEN
        };
        til::shared_mutex<state_t> _state;
        virtual void _write() const noexcept override;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ElevatedState);
}
