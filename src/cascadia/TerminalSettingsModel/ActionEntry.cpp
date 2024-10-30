// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionEntry.h"
#include "JsonUtils.h"

#include "ActionEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view ActionIdKey{ "id" };
static constexpr std::string_view IconKey{ "icon" };

ActionEntry::ActionEntry() noexcept :
    ActionEntryT<ActionEntry, NewTabMenuEntry>(NewTabMenuEntryType::Action)
{
}

Json::Value ActionEntry::ToJson() const
{
    auto json = NewTabMenuEntry::ToJson();

    JsonUtils::SetValueForKey(json, ActionIdKey, _ActionId);
    JsonUtils::SetValueForKey(json, IconKey, _Icon);

    return json;
}

winrt::com_ptr<NewTabMenuEntry> ActionEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<ActionEntry>();

    JsonUtils::GetValueForKey(json, ActionIdKey, entry->_ActionId);
    JsonUtils::GetValueForKey(json, IconKey, entry->_Icon);

    return entry;
}
