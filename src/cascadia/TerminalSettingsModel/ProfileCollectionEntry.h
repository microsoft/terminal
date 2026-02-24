/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SeparatorEntry.h

Abstract:
- An entry in the "new tab" dropdown menu that represents some collection
    of profiles. This is an abstract class that has concretizations like
    "all profiles from a source" or "all remaining profiles"

Author(s):
- Floris Westerman - August 2022

--*/
#pragma once

#include "NewTabMenuEntry.h"
#include "ProfileCollectionEntry.g.h"
#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ProfileCollectionEntry : ProfileCollectionEntryT<ProfileCollectionEntry, NewTabMenuEntry>
    {
    public:
        // Since a comma does not work very nicely in a macro and we need one
        // for our map definition, we use a macro te define a comma.
#define COMMA ,
        WINRT_PROPERTY(winrt::Windows::Foundation::Collections::IMap<int COMMA Model::Profile>, Profiles);
#undef COMMA

        // We have a protected/hidden constructor so consumers cannot instantiate
        // this class directly and need to go through one of the subclasses.
    protected:
        explicit ProfileCollectionEntry(const NewTabMenuEntryType type) noexcept;
    };
}
