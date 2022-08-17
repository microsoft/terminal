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
#include "NewTabMenuEntry.h"
#include "ProfileCollectionEntry.g.h"
#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ProfileCollectionEntry : ProfileCollectionEntryT<ProfileCollectionEntry, NewTabMenuEntry>
    {
    public:
#define COMMA ,
        WINRT_PROPERTY(winrt::Windows::Foundation::Collections::IMap<int COMMA Model::Profile>, Profiles);
#undef COMMA

    protected:
        explicit ProfileCollectionEntry(const NewTabMenuEntryType type) noexcept;
    };
}
