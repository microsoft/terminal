// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionEntry.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "ActionEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;

static constexpr std::string_view ActionIdKey{ "id" };
static constexpr std::string_view IconKey{ "icon" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{

    ActionEntry::ActionEntry() noexcept :
        ActionEntryT<ActionEntry, NewTabMenuEntry, IPathlessMediaResourceContainer>(NewTabMenuEntryType::Action)
    {
    }

    Json::Value ActionEntry::ToJson() const
    {
        auto json = NewTabMenuEntry::ToJson();

        JsonUtils::SetValueForKey(json, ActionIdKey, _ActionId);
        JsonUtils::SetValueForKey(json, IconKey, _icon);

        return json;
    }

    winrt::com_ptr<NewTabMenuEntry> ActionEntry::FromJson(const Json::Value& json)
    {
        auto entry = winrt::make_self<ActionEntry>();

        JsonUtils::GetValueForKey(json, ActionIdKey, entry->_ActionId);
        JsonUtils::GetValueForKey(json, IconKey, entry->_icon);

        return entry;
    }

    Model::NewTabMenuEntry ActionEntry::Copy() const
    {
        auto entry = winrt::make_self<ActionEntry>();
        entry->_ActionId = _ActionId;
        entry->_icon = _icon;
        return *entry;
    }

    void ActionEntry::ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const Model::MediaResourceResolver& resolver)
    {
        if (_icon)
        {
            // TODO GH#19191 (Hardcoded Origin, since that's the only place it could have come from)
            ResolveIconMediaResource(OriginTag::User, basePath, _icon, resolver);
        }
    }
}
