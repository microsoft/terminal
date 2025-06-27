// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ProfileIcon.g.h"
#include "TerminalWarnings.h"
#include "Command.g.h"
#include "TerminalWarnings.h"
#include "Profile.h"
#include "ActionAndArgs.h"
#include "SettingsTypes.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    static constexpr std::string_view LightModeKey{ "light" };
    static constexpr std::string_view DarkModeKey{ "dark" };

    struct ProfileIcon : public ProfileIconT<ProfileIcon>
    {
    public:
        static Json::Value ToJson(const Model::ProfileIcon& val);
        static com_ptr<ProfileIcon> FromJson(const Json::Value& json);
        ProfileIcon() noexcept;
        WINRT_PROPERTY(winrt::hstring, Dark);
        WINRT_PROPERTY(winrt::hstring, Light);
    private:
        bool _layerJson(const Json::Value& json);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ProfileIcon);
}

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    using namespace winrt::Microsoft::Terminal::Settings::Model;

    template<>
    struct ConversionTrait<ProfileIcon>
    {
        ProfileIcon FromJson(const Json::Value& json)
        {
            return *implementation::ProfileIcon::FromJson(json);
        }

        bool CanConvert(const Json::Value& json) const
        {
            return json.isObject();
        }

        Json::Value ToJson(const ProfileIcon& val)
        {
            return implementation::ProfileIcon::ToJson(val);
        }

        std::string TypeDescription() const
        {
            return "ProfileIcon";
        }
    };
}
