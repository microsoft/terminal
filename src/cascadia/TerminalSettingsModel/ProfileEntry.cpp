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

    // We always store the GUID of the profile since that is less
    // error prone in the long term.
    JsonUtils::SetValueForKey(json, ProfileKey, _Profile.Guid());

    return json;
}

winrt::com_ptr<NewTabMenuEntry> ProfileEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<ProfileEntry>();

    JsonUtils::GetValueForKey(json, ProfileKey, entry->_ProfileName);

    return entry;
}
