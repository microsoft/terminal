// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "DynamicProfileUtils.h"
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
winrt::com_ptr<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile> CreateDynamicProfile(const std::wstring_view& name)
{
    const auto profileGuid = Microsoft::Console::Utils::CreateV5Uuid(TERMINAL_PROFILE_NAMESPACE_GUID, std::as_bytes(std::span{ name }));

    std::wstring iconPath{ PACKAGED_PROFILE_ICON_PATH };
    iconPath.append(Microsoft::Console::Utils::GuidToString(profileGuid));
    iconPath.append(PACKAGED_PROFILE_ICON_EXTENSION);

    auto profile = winrt::make_self<winrt::Microsoft::Terminal::Settings::Model::implementation::Profile>(profileGuid);
    profile->Name(winrt::hstring{ name });
    profile->Icon(winrt::hstring{ iconPath });
    return profile;
}
