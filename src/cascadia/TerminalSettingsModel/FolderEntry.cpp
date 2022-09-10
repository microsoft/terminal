// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FolderEntry.h"
#include "JsonUtils.h"

#include "FolderEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IconKey{ "icon" };
static constexpr std::string_view EntriesKey{ "entries" };

FolderEntry::FolderEntry() noexcept :
    FolderEntry{ winrt::hstring{} }
{
}

FolderEntry::FolderEntry(const winrt::hstring& name) noexcept :
    FolderEntryT<FolderEntry, NewTabMenuEntry>(NewTabMenuEntryType::Folder),
    _Name{ name }
{
}

Json::Value FolderEntry::ToJson() const
{
    auto json = NewTabMenuEntry::ToJson();

    JsonUtils::SetValueForKey(json, NameKey, _Name);
    JsonUtils::SetValueForKey(json, IconKey, _Icon);
    JsonUtils::SetValueForKey(json, EntriesKey, _Entries);

    return json;
}

winrt::com_ptr<NewTabMenuEntry> FolderEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<FolderEntry>();

    JsonUtils::GetValueForKey(json, NameKey, entry->_Name);
    JsonUtils::GetValueForKey(json, IconKey, entry->_Icon);
    JsonUtils::GetValueForKey(json, EntriesKey, entry->_Entries);

    return entry;
}
