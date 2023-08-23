// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "FolderEntry.h"
#include "JsonUtils.h"
#include "TerminalSettingsSerializationHelpers.h"

#include "FolderEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;
using namespace winrt::Windows::Foundation::Collections;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IconKey{ "icon" };
static constexpr std::string_view EntriesKey{ "entries" };
static constexpr std::string_view InliningKey{ "inline" };
static constexpr std::string_view AllowEmptyKey{ "allowEmpty" };

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
    JsonUtils::SetValueForKey(json, InliningKey, _Inlining);
    JsonUtils::SetValueForKey(json, AllowEmptyKey, _AllowEmpty);

    return json;
}

winrt::com_ptr<NewTabMenuEntry> FolderEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<FolderEntry>();

    JsonUtils::GetValueForKey(json, NameKey, entry->_Name);
    JsonUtils::GetValueForKey(json, IconKey, entry->_Icon);
    JsonUtils::GetValueForKey(json, EntriesKey, entry->_Entries);
    JsonUtils::GetValueForKey(json, InliningKey, entry->_Inlining);
    JsonUtils::GetValueForKey(json, AllowEmptyKey, entry->_AllowEmpty);

    return entry;
}

// A FolderEntry should only expose the entries to actually render to WinRT,
// to keep the logic for collapsing/expanding more centralised.
using NewTabMenuEntryModel = winrt::Microsoft::Terminal::Settings::Model::NewTabMenuEntry;
IVector<NewTabMenuEntryModel> FolderEntry::Entries() const
{
    // We filter the full list of entries from JSON to just include the
    // non-empty ones.
    IVector<NewTabMenuEntryModel> result{ winrt::single_threaded_vector<NewTabMenuEntryModel>() };
    if (_Entries == nullptr)
    {
        return result;
    }

    for (const auto& entry : _Entries)
    {
        if (entry == nullptr)
        {
            continue;
        }

        switch (entry.Type())
        {
        case NewTabMenuEntryType::Invalid:
            continue;

        // A profile is filtered out if it is not valid, so if it was not resolved
        case NewTabMenuEntryType::Profile:
        {
            const auto profileEntry = entry.as<ProfileEntry>();
            if (profileEntry.Profile() == nullptr)
            {
                continue;
            }
            break;
        }

        // Any profile collection is filtered out if there are no results
        case NewTabMenuEntryType::RemainingProfiles:
        case NewTabMenuEntryType::MatchProfiles:
        {
            const auto profileCollectionEntry = entry.as<ProfileCollectionEntry>();
            if (profileCollectionEntry.Profiles().Size() == 0)
            {
                continue;
            }
            break;
        }

        // A folder is filtered out if it has an effective size of 0 (calling
        // this filtering method recursively), and if it is not allowed to be
        // empty, or if it should auto-inline.
        case NewTabMenuEntryType::Folder:
        {
            const auto folderEntry = entry.as<Model::FolderEntry>();
            if (folderEntry.Entries().Size() == 0 && (!folderEntry.AllowEmpty() || folderEntry.Inlining() == FolderEntryInlining::Auto))
            {
                continue;
            }
            break;
        }
        }

        result.Append(entry);
    }

    return result;
}
