#include "pch.h"
#include "RemainingProfilesEntry.h"
#include "JsonUtils.h"

#include "RemainingProfilesEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

implementation::RemainingProfilesEntry::RemainingProfilesEntry() noexcept :
    RemainingProfilesEntryT<implementation::RemainingProfilesEntry, implementation::NewTabMenuEntry>(NewTabMenuEntryType::RemainingProfiles)
{
}

Json::Value implementation::RemainingProfilesEntry::ToJson() const
{
    auto json = implementation::NewTabMenuEntry::ToJson();
    return json;
}

winrt::com_ptr<implementation::NewTabMenuEntry> implementation::RemainingProfilesEntry::FromJson(const Json::Value&)
{
    return winrt::make_self<implementation::RemainingProfilesEntry>();
}
