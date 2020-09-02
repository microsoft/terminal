// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Jumplist.h"

#include <Propkey.h>

using namespace winrt::TerminalApp;

//  This property key isn't already defined, but is used by UWP Jumplist to determine the icon of the jumplist item.
//  We need this because our icon paths are 
//  Name:     System.AppUserModel.DestinationListLogoUri -- PKEY_AppUserModel_DestListLogoUri
//  Type:     String -- VT_LPWSTR
//  FormatID: {9F4C2855-9F79-4B39-A8D0-E1D42DE1D5F3}, 29
DEFINE_PROPERTYKEY(PKEY_AppUserModel_DestListLogoUri, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 29);
#define INIT_PKEY_AppUserModel_DestListLogoUri                                         \
{                                                                                      \
    { 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 }, 29 \
}

// Method Description:
// - Updates the items of the Jumplist based on the given settings.
// Arguments:
// - settings - The settings object to update the jumplist with.
// Return Value:
// - <none>
void Jumplist::UpdateJumplist(std::shared_ptr<::TerminalApp::CascadiaSettings> settings)
{
    winrt::com_ptr<ICustomDestinationList> jumplistInstance;
    if (FAILED(CoCreateInstance(CLSID_DestinationList, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&jumplistInstance))))
    {
        return;
    }

    // Start the Jumplist edit transaction
    uint32_t slots;
    winrt::com_ptr<IObjectCollection> jumplistItems;
    if (FAILED(jumplistInstance->BeginList(&slots, IID_PPV_ARGS(&jumplistItems))))
    {
        return;
    }

    // TODO: It's just easier to clear the list and re-add everything. The settings aren't
    // updated often, and there likely isn't a huge number of profiles to add.
    jumplistItems->Clear();

    // Update the list of profiles.
    if (FAILED(_updateProfiles(jumplistItems, settings->GetProfiles())))
    {
        return;
    }

    // TODO: Add items from the future customizable new tab dropdown as well.

    // Add the items to the jumplist Task section.
    // The Tasks section is immutable by the user, unlike the destinations
    // section that can have its items pinned and removed.
    jumplistInstance->AddUserTasks(jumplistItems.get());

    if (FAILED(jumplistInstance->CommitList()))
    {
        return;
    }
}

// Method Description:
// - Creates and adds a ShellLink object to the Jumplist for each profile.
// Arguments:
// - jumplistItems - The jumplist item list
// - profiles - The profiles to add to the jumplist
// Return Value:
// - S_OK or HRESULT failure code.
HRESULT Jumplist::_updateProfiles(winrt::com_ptr<IObjectCollection>& jumplistItems, gsl::span<const Profile> profiles)
{
    HRESULT result = S_OK;

    for (const auto& profile : profiles)
    {
        // Craft the arguments following "wt.exe"
        auto profileName = profile.Name();
        auto args = fmt::format(L"-p \"{}\"", profileName);

        // Create the shell link object for the profile
        winrt::com_ptr<IShellLink> shLink;
        auto result = _createShellLink(profileName, profile.GetExpandedIconPath(), args, shLink);
        if (FAILED(result))
        {
            return result;
        }

        jumplistItems->AddObject(shLink.get());
    }

    return result;
}

// Method Description:
// - Creates a ShellLink object. Each item in a jumplist is a ShellLink, which is sort of
//   like a shortcut. It requires the path to the application (wt.exe), the arguments to pass,
//   and the path to the icon for the jumplist item. The path to the application isn't passed
//   into this function, as we'll determine it with GetModuleFileName.
// Arguments:
// - name: The name of the item displayed in the jumplist.
// - path: The path to the icon for the jumplist item.
// - args: The arguments to pass along with wt.exe
// Return Value:
// - S_OK or HRESULT failure code.
HRESULT Jumplist::_createShellLink(const std::wstring_view& name,
                                   const std::wstring_view& path,
                                   const std::wstring_view& args,
                                   winrt::com_ptr<IShellLink>& shLink)
{
    // Create a shell link object
    auto result = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&shLink));
    if (FAILED(result))
    {
        return result;
    }

    wchar_t wtExe[MAX_PATH];
    // Passing null gives us the path of the executable file of the current process.
    GetModuleFileName(NULL, wtExe, ARRAYSIZE(wtExe));
    shLink->SetPath(wtExe);
    shLink->SetArguments(args.data());

    PROPVARIANT titleProp;
    titleProp.vt = VT_LPWSTR;
    titleProp.pwszVal = const_cast<wchar_t*>(name.data());

    PROPVARIANT iconProp;
    iconProp.vt = VT_LPWSTR;
    iconProp.pwszVal = const_cast<wchar_t*>(path.data());

    winrt::com_ptr<IPropertyStore> propStore = shLink.as<IPropertyStore>();
    propStore->SetValue(PKEY_Title, titleProp);
    propStore->SetValue(PKEY_AppUserModel_DestListLogoUri, iconProp);

    result = propStore->Commit();
    if (FAILED(result))
    {
        return result;
    }

    return result;
}
