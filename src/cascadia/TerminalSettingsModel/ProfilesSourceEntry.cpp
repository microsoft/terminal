#include "pch.h"
#include "ProfilesSourceEntry.h"
#include "NewTabMenuEntry.h"
#include "JsonUtils.h"

#include "ProfilesSourceEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

static constexpr std::string_view SourceKey{ "source" };

ProfilesSourceEntry::ProfilesSourceEntry() noexcept :
    ProfilesSourceEntry{ winrt::hstring{} }
{
}

ProfilesSourceEntry::ProfilesSourceEntry(const winrt::hstring& source) noexcept :
    ProfilesSourceEntryT<ProfilesSourceEntry, ProfileCollectionEntry>(NewTabMenuEntryType::Source),
    _Source { source }
{
}

Json::Value ProfilesSourceEntry::ToJson() const
{
    auto json = NewTabMenuEntry::ToJson();

    JsonUtils::SetValueForKey(json, SourceKey, _Source);

    return json;
}

winrt::com_ptr<NewTabMenuEntry> ProfilesSourceEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<ProfilesSourceEntry>();

    JsonUtils::GetValueForKey(json, SourceKey, entry->_Source);

    return entry;
}
