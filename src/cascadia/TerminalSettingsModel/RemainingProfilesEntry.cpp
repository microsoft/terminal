// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

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

winrt::com_ptr<NewTabMenuEntry> RemainingProfilesEntry::FromJson(const Json::Value&)
{
    return winrt::make_self<RemainingProfilesEntry>();
}
