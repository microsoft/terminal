// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ProfileEntry.h"
#include "JsonUtils.h"

#include "ProfileEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view ProfileKey{ "profile" };

ProfileEntry::ProfileEntry() noexcept :
    ProfileEntry{ winrt::hstring{} }
{
}

ProfileEntry::ProfileEntry(const winrt::hstring& profile) noexcept :
    ProfileEntryT<ProfileEntry, NewTabMenuEntry>(NewTabMenuEntryType::Profile),
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

    return json;
}

winrt::com_ptr<NewTabMenuEntry> ProfileEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<ProfileEntry>();

    JsonUtils::GetValueForKey(json, ProfileKey, entry->_ProfileName);

    return entry;
}
