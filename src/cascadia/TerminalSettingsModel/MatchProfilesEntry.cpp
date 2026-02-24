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
        entry->_validateAndPopulateNameRegex();

        JsonUtils::GetValueForKey(json, CommandlineKey, entry->_Commandline);
        entry->_validateAndPopulateCommandlineRegex();

        JsonUtils::GetValueForKey(json, SourceKey, entry->_Source);
        entry->_validateAndPopulateSourceRegex();

        return entry;
    }

    // Returns true if all regexes are valid, false otherwise
    bool MatchProfilesEntry::ValidateRegexes() const
    {
        return !(_invalidName || _invalidCommandline || _invalidSource);
    }

#define DEFINE_VALIDATE_FUNCTION(name)                                    \
    void MatchProfilesEntry::_validateAndPopulate##name##Regex() noexcept \
    {                                                                     \
        _invalid##name = false;                                           \
        if (_##name.empty())                                              \
        {                                                                 \
            /* empty field is valid*/                                     \
            return;                                                       \
        }                                                                 \
        UErrorCode status = U_ZERO_ERROR;                                 \
        _##name##Regex = til::ICU::CreateRegex(_##name, 0, &status);      \
        if (U_FAILURE(status))                                            \
        {                                                                 \
            _invalid##name = true;                                        \
            _##name##Regex.reset();                                       \
        }                                                                 \
    }

    DEFINE_VALIDATE_FUNCTION(Name);
    DEFINE_VALIDATE_FUNCTION(Commandline);
    DEFINE_VALIDATE_FUNCTION(Source);

    bool MatchProfilesEntry::MatchesProfile(const Model::Profile& profile)
    {
        auto isMatch = [](const til::ICU::unique_uregex& regex, std::wstring_view text) {
            if (text.empty())
            {
                return false;
            }
            UErrorCode status = U_ZERO_ERROR;
            uregex_setText(regex.get(), reinterpret_cast<const UChar*>(text.data()), static_cast<int32_t>(text.size()), &status);
            const auto match = uregex_matches(regex.get(), 0, &status);
            return status == U_ZERO_ERROR && match;
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
