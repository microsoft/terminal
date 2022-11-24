// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NewTabMenuEntry.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"
#include "SeparatorEntry.h"
#include "FolderEntry.h"
#include "ProfileEntry.h"
#include "RemainingProfilesEntry.h"
#include "MatchProfilesEntry.h"

#include "NewTabMenuEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using NewTabMenuEntryType = winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntryType;

static constexpr std::string_view TypeKey{ "type" };

NewTabMenuEntry::NewTabMenuEntry(const NewTabMenuEntryType type) noexcept :
    _Type{ type }
{
}

// This method will be overridden by the subclasses, which will then call this
// parent implementation for a "base" json object.
Json::Value NewTabMenuEntry::ToJson() const
{
    Json::Value json{ Json::ValueType::objectValue };

    JsonUtils::SetValueForKey(json, TypeKey, _Type);

    return json;
}

// Deserialize the JSON object based on the given type. We use the map from above for that.
winrt::com_ptr<NewTabMenuEntry> NewTabMenuEntry::FromJson(const Json::Value& json)
{
    const auto type = JsonUtils::GetValueForKey<NewTabMenuEntryType>(json, TypeKey);

    switch (type)
    {
    case NewTabMenuEntryType::Separator:
        return SeparatorEntry::FromJson(json);
    case NewTabMenuEntryType::Folder:
        return FolderEntry::FromJson(json);
    case NewTabMenuEntryType::Profile:
        return ProfileEntry::FromJson(json);
    case NewTabMenuEntryType::RemainingProfiles:
        return RemainingProfilesEntry::FromJson(json);
    case NewTabMenuEntryType::MatchProfiles:
        return MatchProfilesEntry::FromJson(json);
    default:
        return nullptr;
    }
}
