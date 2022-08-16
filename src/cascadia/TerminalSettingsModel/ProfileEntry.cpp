#include "pch.h"
#include "ProfileEntry.h"
#include "JsonUtils.h"

#include "ProfileEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static constexpr std::string_view ProfileKey{ "profile" };

implementation::ProfileEntry::ProfileEntry() noexcept :
    implementation::ProfileEntry{ winrt::hstring{} }
{
}

implementation::ProfileEntry::ProfileEntry(const winrt::hstring& profile) noexcept :
    ProfileEntryT<implementation::ProfileEntry, implementation::NewTabMenuEntry>(NewTabMenuEntryType::Profile),
    _Profile{ profile }
{
}

Json::Value implementation::ProfileEntry::ToJson() const
{
    auto json = implementation::NewTabMenuEntry::ToJson();

    JsonUtils::SetValueForKey(json, ProfileKey, _Profile);

    return json;
}

winrt::com_ptr<implementation::NewTabMenuEntry> implementation::ProfileEntry::FromJson(const Json::Value& json)
{
    auto entry = winrt::make_self<implementation::ProfileEntry>();

    JsonUtils::GetValueForKey(json, ProfileKey, entry->_Profile);

    return entry;
}
