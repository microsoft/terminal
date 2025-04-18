// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MatchProfilesEntry.h"
#include "JsonUtils.h"

#include "MatchProfilesEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view CommandlineKey{ "commandline" };
static constexpr std::string_view SourceKey{ "source" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
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
        entry->_validateName();

        JsonUtils::GetValueForKey(json, CommandlineKey, entry->_Commandline);
        entry->_validateCommandline();

        JsonUtils::GetValueForKey(json, SourceKey, entry->_Source);
        entry->_validateSource();

        return entry;
    }

    // Returns true if all regexes are valid, false otherwise
    bool MatchProfilesEntry::ValidateRegexes() const
    {
        return !(_invalidName || _invalidCommandline || _invalidSource);
    }

    bool MatchProfilesEntry::MatchesProfile(const Model::Profile& profile)
    {
        auto isMatch = [](const std::wregex& regex, std::wstring_view text) {
            if (text.empty())
            {
                return false;
            }
            return std::regex_match(text.cbegin(), text.cend(), regex);
        };

        if (!_Name.empty() && isMatch(_NameRegex, profile.Name()))
        {
            return true;
        }
        else if (!_Source.empty() && isMatch(_SourceRegex, profile.Source()))
        {
            return true;
        }
        else if (!_Commandline.empty() && isMatch(_CommandlineRegex, profile.Commandline()))
        {
            return true;
        }
        return false;
    }

    Model::NewTabMenuEntry MatchProfilesEntry::Copy() const
    {
        auto entry = winrt::make_self<MatchProfilesEntry>();
        entry->_Name = _Name;
        entry->_Commandline = _Commandline;
        entry->_Source = _Source;
        return *entry;
    }
}
