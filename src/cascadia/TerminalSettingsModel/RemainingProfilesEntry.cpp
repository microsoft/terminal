#include "pch.h"
#include "RemainingProfilesEntry.h"
#include "NewTabMenuEntry.h"
#include "JsonUtils.h"

#include "RemainingProfilesEntry.g.cpp"

using namespace Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model::implementation;

RemainingProfilesEntry::RemainingProfilesEntry() noexcept :
    RemainingProfilesEntryT<RemainingProfilesEntry, ProfileCollectionEntry>(NewTabMenuEntryType::RemainingProfiles)
{
}

Json::Value RemainingProfilesEntry::ToJson() const
{
    auto json = NewTabMenuEntry::ToJson();
    return json;
}

winrt::com_ptr<NewTabMenuEntry> RemainingProfilesEntry::FromJson(const Json::Value&)
{
    return winrt::make_self<RemainingProfilesEntry>();
}
