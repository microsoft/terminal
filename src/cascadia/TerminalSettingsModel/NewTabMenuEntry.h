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

#include "NewTabMenuEntry.g.h"
#include "JsonUtils.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct NewTabMenuEntry : NewTabMenuEntryT<NewTabMenuEntry>
    {
    public:
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);
        virtual Json::Value ToJson() const;

        WINRT_PROPERTY(NewTabMenuEntryType, Type, NewTabMenuEntryType::Invalid);

        // We have a protected/hidden constructor so consumers cannot instantiate
        // this base class directly and need to go through either FromJson
        // or one of the subclasses.
    protected:
        explicit NewTabMenuEntry(const NewTabMenuEntryType type) noexcept;
    };
}

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    using namespace winrt::Microsoft::Terminal::Settings::Model;

    template<>
    struct ConversionTrait<NewTabMenuEntry>
    {
        NewTabMenuEntry FromJson(const Json::Value& json)
        {
            const auto entry = implementation::NewTabMenuEntry::FromJson(json);
            if (entry == nullptr)
            {
                return nullptr;
            }

            return *entry;
        }

        bool CanConvert(const Json::Value& json) const
        {
            return json.isObject();
        }

        Json::Value ToJson(const NewTabMenuEntry& val)
        {
            return winrt::get_self<implementation::NewTabMenuEntry>(val)->ToJson();
        }

        std::string TypeDescription() const
        {
            return "NewTabMenuEntry";
        }
    };
}
