#include "pch.h"
#include "ProfilesSourceEntry.h"
#include "NewTabMenuEntry.h"
#include "JsonUtils.h"

#include "ProfilesSourceEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static constexpr std::string_view SourceKey{ "source" };

implementation::ProfilesSourceEntry::ProfilesSourceEntry() noexcept :
    implementation::ProfilesSourceEntry{ winrt::hstring{} }
{
}

implementation::ProfilesSourceEntry::ProfilesSourceEntry(const winrt::hstring& source) noexcept :
    ProfilesSourceEntryT<implementation::ProfilesSourceEntry, implementation::ProfileCollectionEntry>(NewTabMenuEntryType::Source),
    _Source { source }
{
}

Json::Value implementation::ProfilesSourceEntry::ToJson() const
{
    auto json = implementation::NewTabMenuEntry::ToJson();

    JsonUtils::SetValueForKey(json, SourceKey, _Source);

    return json;
}

winrt::com_ptr<implementation::NewTabMenuEntry> implementation::ProfilesSourceEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<implementation::ProfilesSourceEntry>();

    JsonUtils::GetValueForKey(json, SourceKey, entry->_Source);

    return entry;
}
