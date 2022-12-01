// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MatchProfilesEntry.h"
#include "JsonUtils.h"

#include "MatchProfilesEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view CommandlineKey{ "commandline" };
static constexpr std::string_view SourceKey{ "source" };

MatchProfilesEntry::MatchProfilesEntry() noexcept :
    MatchProfilesEntryT<MatchProfilesEntry, ProfileCollectionEntry>(NewTabMenuEntryType::MatchProfiles)
{
}

Json::Value MatchProfilesEntry::ToJson() const
{
    auto json = NewTabMenuEntry::ToJson();

    JsonUtils::SetValueForKey(json, NameKey, _Name);
    JsonUtils::SetValueForKey(json, CommandlineKey, _Commandline);
    JsonUtils::SetValueForKey(json, SourceKey, _Source);

    return json;
}

winrt::com_ptr<NewTabMenuEntry> MatchProfilesEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<MatchProfilesEntry>();

    JsonUtils::GetValueForKey(json, NameKey, entry->_Name);
    JsonUtils::GetValueForKey(json, CommandlineKey, entry->_Commandline);
    JsonUtils::GetValueForKey(json, SourceKey, entry->_Source);

    return entry;
}

bool MatchProfilesEntry::MatchesProfile(const Model::Profile& profile)
{
    auto isMatching = false;

    if (!_Name.empty())
    {
        isMatching = isMatching || _Name == profile.Name();
    }

    if (!_Source.empty())
    {
        isMatching = isMatching || _Source == profile.Source();
    }

    if (!_Commandline.empty())
    {
        isMatching = isMatching || _Commandline == profile.Commandline();
    }

    return isMatching;
}
