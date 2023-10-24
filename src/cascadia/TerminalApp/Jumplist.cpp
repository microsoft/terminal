// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Jumplist.h"

#include <ShObjIdl.h>
#include <Propkey.h>

#include <WtExeUtils.h>

using namespace winrt::Microsoft::Terminal::Settings::Model;

//  This property key isn't already defined in propkey.h, but is used by UWP Jumplist to determine the icon of the jumplist item.
//  IShellLink's SetIconLocation isn't going to read "ms-appx://" icon paths, so we'll need to use this to set the icon.
DEFINE_PROPERTYKEY(PKEY_AppUserModel_DestListLogoUri, 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3, 29);
#define INIT_PKEY_AppUserModel_DestListLogoUri                                             \
    {                                                                                      \
        { 0x9F4C2855, 0x9F79, 0x4B39, 0xA8, 0xD0, 0xE1, 0xD4, 0x2D, 0xE1, 0xD5, 0xF3 }, 29 \
    }

// Function Description:
// - This function guesses whether a string is a file path.
static constexpr bool _isProbableFilePath(std::wstring_view path)
{
    // "C:X", "C:\X", "\\?", "\\."
    // _this function rejects \??\ as a path_
    if (path.size() >= 3)
    {
        const auto firstColon{ path.find(L':') };
        if (firstColon == 1)
        {
            return true;
        }

        const auto prefix{ path.substr(0, 2) };
        return prefix == LR"(//)" || prefix == LR"(\\)";
    }
    return false;
}

// Function Description:
// - DestListLogoUri cannot take paths that are separated by / unless they're URLs.
//   This function uses std::filesystem to normalize strings that appear to be file
//   paths to have the "correct" slash direction.
static std::wstring _normalizeIconPath(std::wstring_view path)
{
    const auto fullPath{ wil::ExpandEnvironmentStringsW<std::wstring>(path.data()) };
    if (_isProbableFilePath(fullPath))
    {
        std::filesystem::path asPath{ fullPath };
        return asPath.make_preferred().wstring();
    }
    return std::wstring{ fullPath };
}

// Method Description:
// - Updates the items of the Jumplist based on the given settings.
// Arguments:
// - settings - The settings object to update the jumplist with.
// Return Value:
// - <none>
winrt::fire_and_forget Jumplist::UpdateJumplist(const CascadiaSettings& settings) noexcept
{
    if (!settings)
    {
        // By all accounts, this shouldn't be null. Seemingly however (GH
        // #12360), it sometimes is. So just check this case here and log a
        // message.
        TraceLoggingWrite(g_hTerminalAppProvider,
                          "Jumplist_UpdateJumplist_NullSettings",
                          TraceLoggingLevel(WINEVENT_LEVEL_VERBOSE),
                          TraceLoggingKeyword(TIL_KEYWORD_TRACE));

        co_return;
    }

    // make sure to capture the settings _before_ the co_await
    const auto strongSettings = settings;

    co_await winrt::resume_background();

    try
    {
        auto jumplistInstance = winrt::create_instance<ICustomDestinationList>(CLSID_DestinationList, CLSCTX_ALL);

        // Start the Jumplist edit transaction
        uint32_t slots;
        winrt::com_ptr<IObjectCollection> jumplistItems;
        jumplistItems.capture(jumplistInstance, &ICustomDestinationList::BeginList, &slots);

        // Update the list of profiles.
        _updateProfiles(jumplistItems.get(), strongSettings.ActiveProfiles().GetView());

        // TODO GH#1571: Add items from the future customizable new tab dropdown as well.
        // This could either replace the default profiles, or be added alongside them.

        // Add the items to the jumplist Task section.
        // The Tasks section is immutable by the user, unlike the destinations
        // section that can have its items pinned and removed.
        THROW_IF_FAILED(jumplistInstance->AddUserTasks(jumplistItems.get()));

        THROW_IF_FAILED(jumplistInstance->CommitList());
    }
    CATCH_LOG();
}

// Method Description:
// - Creates and adds a ShellLink object to the Jumplist for each profile.
// Arguments:
// - jumplistItems - The jumplist item list
// - profiles - The profiles to add to the jumplist
// Return Value:
// - S_OK or HRESULT failure code.
void Jumplist::_updateProfiles(IObjectCollection* jumplistItems, winrt::Windows::Foundation::Collections::IVectorView<Profile> profiles)
{
    // It's easier to clear the list and re-add everything. The settings aren't
    // updated often, and there likely isn't a huge amount of items to add.
    THROW_IF_FAILED(jumplistItems->Clear());

    for (const auto& profile : profiles)
    {
        // Craft the arguments following "wt.exe"
        auto args = fmt::format(L"-p {}", to_hstring(profile.Guid()));

        // Create the shell link object for the profile
        const auto normalizedIconPath{ _normalizeIconPath(profile.Icon()) };
        const auto shLink = _createShellLink(profile.Name(), normalizedIconPath, args);
        THROW_IF_FAILED(jumplistItems->AddObject(shLink.get()));
    }
}

// Method Description:
// - Creates a ShellLink object. Each item in a jumplist is a ShellLink, which is sort of
//   like a shortcut. It requires the path to the application (wt.exe), the arguments to pass,
//   and the path to the icon for the jumplist item. The path to the application isn't passed
//   into this function, as we'll determine it with GetWtExePath
// Arguments:
// - name: The name of the item displayed in the jumplist.
// - path: The path to the icon for the jumplist item.
// - args: The arguments to pass along with wt.exe
// - shLink: The shell link object to return.
// Return Value:
// - S_OK or HRESULT failure code.
winrt::com_ptr<IShellLinkW> Jumplist::_createShellLink(const std::wstring_view name, const std::wstring_view path, const std::wstring_view args)
{
    auto sh = winrt::create_instance<IShellLinkW>(CLSID_ShellLink, CLSCTX_ALL);

    const auto module{ GetWtExePath() };
    THROW_IF_FAILED(sh->SetPath(module.data()));
    THROW_IF_FAILED(sh->SetArguments(args.data()));
    auto propStore{ sh.as<IPropertyStore>() };

    PROPVARIANT titleProp;
    titleProp.vt = VT_LPWSTR;
    titleProp.pwszVal = const_cast<wchar_t*>(name.data());

    // Check for a comma in the path. If we find one we have an indirect icon. Parse the path into a file path and index/id.
    auto commaPosition = path.find(L",");
    if (commaPosition != std::wstring_view::npos)
    {
        const std::wstring iconPath{ path.substr(0, commaPosition) };

        // We dont want the comma included so add 1 to its position
        int iconIndex = til::to_int(path.substr(commaPosition + 1));
        if (iconIndex != til::to_int_error)
        {
            THROW_IF_FAILED(sh->SetIconLocation(iconPath.data(), iconIndex));
        }
    }
    else if (til::ends_with(path, L"exe") || til::ends_with(path, L"dll"))
    {
        // We have a binary path but no index/id. Default to 0
        THROW_IF_FAILED(sh->SetIconLocation(path.data(), 0));
    }
    else
    {
        PROPVARIANT iconProp;
        iconProp.vt = VT_LPWSTR;
        iconProp.pwszVal = const_cast<wchar_t*>(path.data());

        THROW_IF_FAILED(propStore->SetValue(PKEY_AppUserModel_DestListLogoUri, iconProp));
    }

    THROW_IF_FAILED(propStore->SetValue(PKEY_Title, titleProp));
    THROW_IF_FAILED(propStore->Commit());

    return sh;
}
