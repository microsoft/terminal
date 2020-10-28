// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Jumplist.h"

#include <ShObjIdl.h>
#include <Propkey.h>

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

// Function Description:
// - Helper function for getting the path to the appropriate executable to use
//   for this instance of the jumplist. For the dev build, it should be `wtd.exe`,
//   but if we're preview or release, we want to make sure to get the correct
//   `wt.exe` that corresponds to _us_.
// - If we're unpackaged, this needs to get us `WindowsTerminal.exe`, because
//   the `wt*exe` alias won't have been installed for this install.
// Arguments:
// - <none>
// Return Value:
// - the full path to the exe, one of `wt.exe`, `wtd.exe`, or `WindowsTerminal.exe`.
static std::wstring_view _getExePath()
{
    static constexpr std::wstring_view WtExe{ L"wt.exe" };
    static constexpr std::wstring_view WindowsTerminalExe{ L"WindowsTerminal.exe" };
    static constexpr std::wstring_view WtdExe{ L"wtd.exe" };

    static constexpr std::wstring_view LocalAppDataAppsPath{ L"%LOCALAPPDATA%\\Microsoft\\WindowsApps\\" };

    // use C++11 magic statics to make sure we only do this once.
    static const std::wstring exePath = []() -> std::wstring {
        // First, check a packaged location for the exe. If we've got a package
        // family name, that means we're one of the packaged Dev build, packaged
        // Release build, or packaged Preview build.
        //
        // If we're the preview or release build, there's no way of knowing if the
        // `wt.exe` on the %PATH% is us or not. Fortunately, _our_ execution alias
        // is located in "%LOCALAPPDATA%\Microsoft\WindowsApps\<our package family
        // name>", _always_, so we can use that to look up the exe easier.
        try
        {
            const auto package{ winrt::Windows::ApplicationModel::Package::Current() };
            const auto id{ package.Id() };
            const std::wstring pfn{ id.FamilyName() };
            const auto isDevPackage{ pfn.rfind(L"WindowsTerminalDev") == 0 };
            if (!pfn.empty())
            {
                const std::filesystem::path windowsAppsPath{ wil::ExpandEnvironmentStringsW<std::wstring>(LocalAppDataAppsPath.data()) };
                const std::filesystem::path wtPath{ windowsAppsPath / pfn / (isDevPackage ? WtdExe : WtExe) };
                return wtPath;
            }
        }
        CATCH_LOG();

        // If we're here, then we couldn't resolve our exe from the package. This
        // means we're running unpackaged. We should just use the
        // WindowsTerminal.exe that's sitting in the directory next to us.
        try
        {
            std::filesystem::path module{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
            module.replace_filename(WindowsTerminalExe);
            return module;
        }
        CATCH_LOG();

        return std::wstring{ WtExe };
    }();
    return exePath;
}

// Method Description:
// - Updates the items of the Jumplist based on the given settings.
// Arguments:
// - settings - The settings object to update the jumplist with.
// Return Value:
// - <none>
winrt::fire_and_forget Jumplist::UpdateJumplist(const CascadiaSettings& settings) noexcept
{
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

        // It's easier to clear the list and re-add everything. The settings aren't
        // updated often, and there likely isn't a huge amount of items to add.
        THROW_IF_FAILED(jumplistItems->Clear());

        // Update the list of profiles.
        THROW_IF_FAILED(_updateProfiles(jumplistItems.get(), strongSettings.Profiles().GetView()));

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
[[nodiscard]] HRESULT Jumplist::_updateProfiles(IObjectCollection* jumplistItems, winrt::Windows::Foundation::Collections::IVectorView<Profile> profiles) noexcept
{
    try
    {
        for (const auto& profile : profiles)
        {
            // Craft the arguments following "wt.exe"
            auto args = fmt::format(L"-p {}", to_hstring(profile.Guid()));

            // Create the shell link object for the profile
            winrt::com_ptr<IShellLinkW> shLink;
            const auto normalizedIconPath{ _normalizeIconPath(profile.Icon()) };
            RETURN_IF_FAILED(_createShellLink(profile.Name(), normalizedIconPath, args, shLink.put()));

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
//   into this function, as we'll determine it with _getExePath
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

        const auto module{ _getExePath() };
        RETURN_IF_FAILED(sh->SetPath(module.data()));
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
