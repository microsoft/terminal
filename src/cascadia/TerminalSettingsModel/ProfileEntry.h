/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FolderEntry.h

Abstract:
- A profile entry in the "new tab" dropdown menu, referring

Author(s):
- Floris Westerman - August 2022

--*/
#pragma once

#include "pch.h"
#include "NewTabMenuEntry.h"
#include "ProfileEntry.g.h"

#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ProfileEntry : ProfileEntryT<ProfileEntry, NewTabMenuEntry>
    {
    public:
        ProfileEntry() noexcept;
        explicit ProfileEntry(const winrt::hstring& profile) noexcept;

        Json::Value ToJson() const override;
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);

        winrt::hstring ProfileName() const noexcept { return _ProfileName; };

        WINRT_PROPERTY(Model::Profile, Profile);
        WINRT_PROPERTY(int, ProfileIndex);

    private:
        winrt::hstring _ProfileName;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ProfileEntry);
}
