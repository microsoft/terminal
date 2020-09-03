// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Jumplist.h"

#include <ShObjIdl.h>
#include <Propkey.h>

using namespace winrt::TerminalApp;

//  This property key isn't already defined in propkey.h, but is used by UWP Jumplist to determine the icon of the jumplist item.
//  IShellLink's SetIconLocation isn't going to read "ms-appx://" icon paths, so we'll need to use this to set the icon.
DEFINE_PROPERTYKEY(PKEY_AppUserModel_DestListLogoUri, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 29);
#define INIT_PKEY_AppUserModel_DestListLogoUri                                             \
    {                                                                                      \
        { 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 }, 29 \
    }

// Method Description:
// - Updates the items of the Jumplist based on the given settings.
// Arguments:
// - settings - The settings object to update the jumplist with.
// Return Value:
// - <none>
HRESULT Jumplist::UpdateJumplist(const TerminalApp::CascadiaSettings& settings) noexcept
{
    try
    {
        auto jumplistInstance = winrt::create_instance<ICustomDestinationList>(CLSID_DestinationList, CLSCTX_ALL);

        // Start the Jumplist edit transaction
        uint32_t slots;
        winrt::com_ptr<IObjectCollection> jumplistItems;
        jumplistItems.capture(jumplistInstance, &ICustomDestinationList::BeginList, &slots);

        // It's easier to clear the list and re-add everything. The settings aren't
        // updated often, and there likely isn't a huge amount of items to add.
        RETURN_IF_FAILED(jumplistItems->Clear());

        // Update the list of profiles.
        RETURN_IF_FAILED(_updateProfiles(jumplistItems.get(), settings.GetProfiles()));

        // TODO GH#1571: Add items from the future customizable new tab dropdown as well.
        // This could either replace the default profiles, or be added alongside them.

        // Add the items to the jumplist Task section.
        // The Tasks section is immutable by the user, unlike the destinations
        // section that can have its items pinned and removed.
        RETURN_IF_FAILED(jumplistInstance->AddUserTasks(jumplistItems.get()));

        RETURN_IF_FAILED(jumplistInstance->CommitList());

        return S_OK;
    }
    CATCH_RETURN();
}

// Method Description:
// - Creates and adds a ShellLink object to the Jumplist for each profile.
// Arguments:
// - jumplistItems - The jumplist item list
// - profiles - The profiles to add to the jumplist
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Jumplist::_updateProfiles(IObjectCollection* jumplistItems, const gsl::span<const Profile>& profiles) noexcept
{
    try
    {
        for (const auto& profile : profiles)
        {
            // Craft the arguments following "wt.exe"
            auto args = fmt::format(L"-p {}", to_hstring(profile.Guid()));

            // Create the shell link object for the profile
            winrt::com_ptr<IShellLinkW> shLink;
            RETURN_IF_FAILED(_createShellLink(profile.Name(), profile.GetExpandedIconPath(), args, shLink.put()));

            RETURN_IF_FAILED(jumplistItems->AddObject(shLink.get()));
        }

        return S_OK;
    }
    CATCH_RETURN();
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
// - shLink: The shell link object to return.
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Jumplist::_createShellLink(const std::wstring_view name,
                                                 const std::wstring_view path,
                                                 const std::wstring_view args,
                                                 IShellLinkW** shLink) noexcept
{
    try
    {
        auto sh = winrt::create_instance<IShellLinkW>(CLSID_ShellLink, CLSCTX_ALL);

        std::filesystem::path module{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
        RETURN_IF_FAILED(sh->SetPath(module.c_str()));
        RETURN_IF_FAILED(sh->SetArguments(args.data()));

        PROPVARIANT titleProp;
        titleProp.vt = VT_LPWSTR;
        titleProp.pwszVal = const_cast<wchar_t*>(name.data());

        PROPVARIANT iconProp;
        iconProp.vt = VT_LPWSTR;
        iconProp.pwszVal = const_cast<wchar_t*>(path.data());

        auto propStore{ sh.as<IPropertyStore>() };
        RETURN_IF_FAILED(propStore->SetValue(PKEY_Title, titleProp));
        RETURN_IF_FAILED(propStore->SetValue(PKEY_AppUserModel_DestListLogoUri, iconProp));

        RETURN_IF_FAILED(propStore->Commit());

        *shLink = sh.detach();

        return S_OK;
    }
    CATCH_RETURN();
}
