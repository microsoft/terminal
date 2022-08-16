#include "pch.h"
#include "FolderEntry.h"
#include "JsonUtils.h"

#include "FolderEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static constexpr std::string_view NameKey{ "name" };
static constexpr std::string_view IconKey{ "icon" };
static constexpr std::string_view EntriesKey{ "entries" };

implementation::FolderEntry::FolderEntry() noexcept :
    implementation::FolderEntry{ winrt::hstring{} }
{
}

implementation::FolderEntry::FolderEntry(const winrt::hstring& name) noexcept :
    FolderEntryT<implementation::FolderEntry, implementation::NewTabMenuEntry>(NewTabMenuEntryType::Folder),
    _Name{ name }
{
}

Json::Value implementation::FolderEntry::ToJson() const
{
    auto json = implementation::NewTabMenuEntry::ToJson();

    JsonUtils::SetValueForKey(json, NameKey, _Name);
    JsonUtils::SetValueForKey(json, IconKey, _Icon);
    JsonUtils::SetValueForKey(json, EntriesKey, _Entries);

    return json;
}

winrt::com_ptr<implementation::NewTabMenuEntry> implementation::FolderEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<implementation::FolderEntry>();

    JsonUtils::GetValueForKey(json, NameKey, entry->_Name);
    JsonUtils::GetValueForKey(json, IconKey, entry->_Icon);
    JsonUtils::GetValueForKey(json, EntriesKey, entry->_Entries);

    return entry;
}
