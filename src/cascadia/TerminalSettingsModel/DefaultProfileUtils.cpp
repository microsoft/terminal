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
// - source: the source of the new profile.
// Return Value:
// - A Profile, ready to be filled in
winrt::Microsoft::Terminal::Settings::Model::Profile CreateDefaultProfile(const std::wstring_view name, const std::wstring_view source)
{
    const winrt::guid profileGuid{ winrt::Microsoft::Terminal::Settings::Model::implementation::Profile::GenerateGuidForProfile(winrt::hstring{ name }, winrt::hstring{ source }) };

    auto newProfile = winrt::make<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile>(profileGuid);
    newProfile.Name(name);

    std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
    iconPath.append(Microsoft::Console::Utils::GuidToString(profileGuid));
    iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);

    newProfile.Icon(iconPath);

    return newProfile;
}
