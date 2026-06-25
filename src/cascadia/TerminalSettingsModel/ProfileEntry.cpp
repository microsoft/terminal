// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ProfileEntry.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "ProfileEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;

static constexpr std::string_view ProfileKey{ "profile" };
static constexpr std::string_view IconKey{ "icon" };

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    ProfileEntry::ProfileEntry() noexcept :
        ProfileEntry{ winrt::hstring{} }
    {
    }

    ProfileEntry::ProfileEntry(const winrt::hstring& profile) noexcept :
        ProfileEntryT<ProfileEntry, NewTabMenuEntry, IPathlessMediaResourceContainer>(NewTabMenuEntryType::Profile),
        _ProfileName{ profile }
    {
    }

    Json::Value ProfileEntry::ToJson() const
    {
        auto json = NewTabMenuEntry::ToJson();

        // We will now return a profile reference to the JSON representation. Logic is
        // as follows:
        // - When Profile is null, this is typically because an existing profile menu entry
        //   in the JSON config is invalid (nonexistent or hidden profile). Then, we store
        //   the original profile string value as read from JSON, to improve portability
        //   of the settings file and limit modifications to the JSON.
        // - Otherwise, we always store the GUID of the profile. This will effectively convert
        //   all name-matched profiles from the settings file to GUIDs. This might be unexpected
        //   to some users, but is less error-prone and will continue to work when profile
        //   names are changed.
        if (_Profile == nullptr)
        {
            JsonUtils::SetValueForKey(json, ProfileKey, _ProfileName);
        }
        else
        {
            JsonUtils::SetValueForKey(json, ProfileKey, _Profile.Guid());
        }
        JsonUtils::SetValueForKey(json, IconKey, _icon);

        return json;
    }

    winrt::com_ptr<NewTabMenuEntry> ProfileEntry::FromJson(const Json::Value& json)
    {
        auto entry = winrt::make_self<ProfileEntry>();

        JsonUtils::GetValueForKey(json, ProfileKey, entry->_ProfileName);
        JsonUtils::GetValueForKey(json, IconKey, entry->_icon);

        return entry;
    }

    Model::NewTabMenuEntry ProfileEntry::Copy() const
    {
        auto entry{ winrt::make_self<ProfileEntry>() };
        entry->_Profile = _Profile;
        entry->_ProfileIndex = _ProfileIndex;
        entry->_ProfileName = _ProfileName;
        entry->_icon = _icon;
        return *entry;
    }

    void ProfileEntry::ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const Model::MediaResourceResolver& resolver)
    {
        if (_icon)
        {
            // TODO GH#19191 (Hardcoded Origin, since that's the only place it could have come from)
            ResolveIconMediaResource(OriginTag::User, basePath, _icon, resolver);
        }
    }
}
