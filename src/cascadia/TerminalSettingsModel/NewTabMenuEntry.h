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
        virtual Json::Value ToJson() const;

        WINRT_PROPERTY(NewTabMenuEntryType, Type, NewTabMenuEntryType::Invalid);

    protected:
        explicit NewTabMenuEntry(const NewTabMenuEntryType type) noexcept;
    };
}

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    using namespace winrt::Microsoft::Terminal::Settings::Model;

    template<>
    struct ConversionTrait<winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntry>
    {
        winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntry FromJson(const Json::Value& json)
        {
            return *implementation::NewTabMenuEntry::FromJson(json);
        }

        bool CanConvert(const Json::Value& json) const
        {
            return json.isObject();
        }

        Json::Value ToJson(const winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntry& val)
        {
            return winrt::get_self<implementation::NewTabMenuEntry>(val)->ToJson();
        }

        std::string TypeDescription() const
        {
            return "NewTabMenuEntry";
        }
    };
}
