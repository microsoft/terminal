/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SeparatorEntry.h

Abstract:
- An entry in the "new tab" dropdown menu that represents all profiles
    that were not included explicitly elsewhere

Author(s):
- Floris Westerman - August 2022

--*/
#pragma once

#include "pch.h"
#include "ProfileCollectionEntry.h"
#include "ProfilesSourceEntry.g.h"
#include "JsonUtils.h"
#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ProfilesSourceEntry : ProfilesSourceEntryT<ProfilesSourceEntry, ProfileCollectionEntry>
    {
    public:
        ProfilesSourceEntry() noexcept;
        explicit ProfilesSourceEntry(const winrt::hstring& source) noexcept;

        Json::Value ToJson() const override;
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);

        WINRT_PROPERTY(winrt::hstring, Source);
#define COMMA ,
        WINRT_PROPERTY(winrt::Windows::Foundation::Collections::IMap<int COMMA Model::Profile>, Profiles);
#undef COMMA
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ProfilesSourceEntry);
}
