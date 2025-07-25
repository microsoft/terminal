/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FolderEntry.h

Abstract:
- A profile entry in the "new tab" dropdown menu, referring to
    a single profile.

Author(s):
- Floris Westerman - August 2022

--*/
#pragma once

#include "NewTabMenuEntry.h"
#include "ProfileEntry.g.h"
#include "MediaResourceSupport.h"

#include "Profile.h"

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct ProfileEntry : ProfileEntryT<ProfileEntry, NewTabMenuEntry, IPathlessMediaResourceContainer>
    {
    public:
        ProfileEntry() noexcept;
        explicit ProfileEntry(const winrt::hstring& profile) noexcept;

        Model::NewTabMenuEntry Copy() const override;

        Json::Value ToJson() const override;
        static com_ptr<NewTabMenuEntry> FromJson(const Json::Value& json);

        // In JSON, only a profile name (guid or string) can be set;
        // but the consumers of this class would like to have direct access
        // to the appropriate Model::Profile. Therefore, we have a read-only
        // property ProfileName that corresponds to the JSON value, and
        // then CascadiaSettings::_resolveNewTabMenuProfiles() will populate
        // the Profile and ProfileIndex properties appropriately
        winrt::hstring ProfileName() const noexcept { return _ProfileName; };
        void ResolveMediaResourcesWithBasePath(const winrt::hstring& basePath, const Model::MediaResourceResolver& resolver) override;

        IMediaResource Icon() const noexcept
        {
            return _icon ? _icon : MediaResource::Empty();
        }

        void Icon(const IMediaResource& val)
        {
            _icon = val;
        }

        WINRT_PROPERTY(Model::Profile, Profile);
        WINRT_PROPERTY(int, ProfileIndex);

    private:
        winrt::hstring _ProfileName;
        IMediaResource _icon;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(ProfileEntry);
}
