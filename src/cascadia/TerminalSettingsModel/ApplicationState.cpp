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
    ApplicationState::ApplicationState(std::filesystem::path path) noexcept :
        BaseApplicationState{ path } {}

    // Returns the application-global ApplicationState object.
    Microsoft::Terminal::Settings::Model::ApplicationState ApplicationState::SharedInstance()
    {
        static auto state = winrt::make_self<ApplicationState>(GetBaseSettingsPath() / stateFileName);
        state->Reload();
        return *state;
    }

    void ApplicationState::FromJson(const Json::Value& root) const noexcept
    {
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
    Json::Value ApplicationState::ToJson() const noexcept
    {
        Json::Value root{ Json::objectValue };

        {
            auto state = _state.lock_shared();
#define MTSM_APPLICATION_STATE_GEN(type, name, key, ...) JsonUtils::SetValueForKey(root, key, state->name);
            MTSM_APPLICATION_STATE_FIELDS(MTSM_APPLICATION_STATE_GEN)
#undef MTSM_APPLICATION_STATE_GEN
        }
        return root;
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

}
