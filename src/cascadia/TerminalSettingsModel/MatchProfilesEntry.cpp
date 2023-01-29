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
    // We use an optional here instead of a simple bool directly, since there is no
    // sensible default value for the desired semantics: the first property we want
    // to match on should always be applied (so one would set "true" as a default),
    // but if none of the properties are set, the default return value should be false
    // since this entry type is expected to behave like a positive match/whitelist.
    //
    // The easiest way to deal with this neatly is to use an optional, then for any
    // property to match we consider a null value to be "true", and for the return
    // value of the function we consider the null value to be "false".
    auto isMatching = std::optional<bool>{};

    if (!_Name.empty())
    {
        isMatching = { isMatching.value_or(true) && _Name == profile.Name() };
    }

    if (!_Source.empty())
    {
        isMatching = { isMatching.value_or(true) && _Source == profile.Source() };
    }

    if (!_Commandline.empty())
    {
        isMatching = { isMatching.value_or(true) && _Commandline == profile.Commandline() };
    }

    return isMatching.value_or(false);
}
