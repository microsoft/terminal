/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- MatchProfilesEntry.h

Abstract:
- An entry in the "new tab" dropdown menu that represents a collection
    of profiles that match a specified name, source, or command line.

Author(s):
- Floris Westerman - November 2022

--*/
#pragma once

#include "ProfileCollectionEntry.h"
#include "MatchProfilesEntry.g.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct MatchProfilesEntry : MatchProfilesEntryT<MatchProfilesEntry, ProfileCollectionEntry>
    {
    public:
        MatchProfilesEntry() noexcept;

        Json::Value ToJson() const override;
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);

        bool MatchesProfile(const Model::Profile& profile);

        WINRT_PROPERTY(winrt::hstring, Name);
        WINRT_PROPERTY(winrt::hstring, Commandline);
        WINRT_PROPERTY(winrt::hstring, Source);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(MatchProfilesEntry);
}
