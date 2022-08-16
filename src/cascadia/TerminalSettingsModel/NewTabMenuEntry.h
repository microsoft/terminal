/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- NewTabMenuEntry.h

Abstract:
- An entry in the "new tab" dropdown menu. These entries exist in a few varieties,
    such as separators, folders, or profile links.

Author(s):
- Floris Westerman - August 2022

--*/
#pragma once

#include "pch.h"
#include "NewTabMenuEntry.g.h"
#include "JsonUtils.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct NewTabMenuEntry : NewTabMenuEntryT<NewTabMenuEntry>
    {
    public:
        //virtual com_ptr<NewTabMenuEntry> Copy() const = 0;

        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);
        Json::Value ToJson() const;

        WINRT_PROPERTY(NewTabMenuEntryType, Type, NewTabMenuEntryType::Invalid);

    protected:
        explicit NewTabMenuEntry(const NewTabMenuEntryType type) noexcept;

        friend ::Microsoft::Terminal::Settings::Model::JsonUtils::ConversionTrait<Model::NewTabMenuEntry>;
    };
}

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    using namespace winrt::Microsoft::Terminal::Settings::Model;

    static constexpr std::string_view TypeKey{ "type" };

    template<>
    struct ConversionTrait<winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntry>
    {
        winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntry FromJson(const Json::Value& json)
        {
            auto type = JsonUtils::GetValueForKey<NewTabMenuEntryType>(json, TypeKey);
            //auto entry = winrt::make_self<NewTabMenuEntry>(type);
            if (type == NewTabMenuEntryType::Separator)
            {
                //return winrt::make_self<implementation::SeparatorEntry>();
                //return entry;
            }
            return nullptr;
        }

        bool CanConvert(const Json::Value& json) const
        {
            return json.isObject();
        }

        Json::Value ToJson(const winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntry& val)
        {
            Json::Value json{ Json::ValueType::objectValue };

            SetValueForKey(json, "type", val.Type());

            return json;
        }

        std::string TypeDescription() const
        {
            return "NewTabMenuEntry";
        }
    };
}
