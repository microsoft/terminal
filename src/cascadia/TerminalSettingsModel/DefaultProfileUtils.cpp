// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DefaultProfileUtils.h"
#include "../../types/inc/utils.hpp"

static constexpr std::wstring_view PACKAGED_PROFILE_ICON_PATH{ L"ms-appx:///ProfileIcons/" };
static constexpr std::wstring_view PACKAGED_PROFILE_ICON_EXTENSION{ L".png" };

// Method Description:
// - Helper function for creating a skeleton default profile with a pre-populated
//   guid and name.
// Arguments:
// - name: the name of the new profile.
// Return Value:
// - A Profile, ready to be filled in
winrt::com_ptr<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile> CreateDefaultProfile(const std::wstring_view& source, const std::wstring_view& name)
{
    const winrt::guid profileGuid{ Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID,
                                                                           gsl::as_bytes(gsl::make_span(name))) };

    auto newProfile = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile>(profileGuid);
    newProfile->Source(winrt::hstring{ source });
    newProfile->Name(winrt::hstring{ name });

    std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
    iconPath.append(Microsoft::Console::Utils::GuidToString(profileGuid));
    iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);

    newProfile->Icon(winrt::hstring{ iconPath });
    newProfile->Origin(winrt::Microsoft::Terminal::Settings::Model::OriginTag::Generated);

    return newProfile;
}
