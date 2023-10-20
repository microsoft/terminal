// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "WslDistroGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"
#include "../../inc/DefaultSettings.h"

#include "DynamicProfileUtils.h"

static constexpr std::wstring_view WslHomeDirectory{ L"~" };
static constexpr std::wstring_view DockerDistributionPrefix{ L"docker-desktop" };
static constexpr std::wstring_view RancherDistributionPrefix{ L"rancher-desktop" };

// The WSL entries are structured as such:
// HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss
//   ⌞ {distroGuid}
//     ⌞ DistributionName: {the name}
static constexpr wchar_t RegKeyLxss[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss";
static constexpr wchar_t RegKeyDistroName[] = L"DistributionName";

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static bool isWslDashDashCdAvailableForLinuxPaths() noexcept
{
    OSVERSIONINFOEXW osver{};
    osver.dwOSVersionInfoSize = sizeof(osver);
    osver.dwBuildNumber = 19041;

    DWORDLONG dwlConditionMask = 0;
    VER_SET_CONDITION(dwlConditionMask, VER_BUILDNUMBER, VER_GREATER_EQUAL);

    return VerifyVersionInfoW(&osver, VER_BUILDNUMBER, dwlConditionMask) != FALSE;
}

// Legacy GUIDs:
//   - Debian       58ad8b0c-3ef8-5f4d-bc6f-13e4c00f2530
//   - Ubuntu       2c4de342-38b7-51cf-b940-2309a097f518
//   - Alpine       1777cdf0-b2c4-5a63-a204-eb60f349ea7c
//   - Ubuntu-18.04 c6eaf9f4-32a7-5fdc-b5cf-066e8a4b1e40

std::wstring_view WslDistroGenerator::GetNamespace() const noexcept
{
    return WslGeneratorNamespace;
}

static winrt::com_ptr<implementation::Profile> makeProfile(const std::wstring& distName)
{
    const auto WSLDistro{ CreateDynamicProfile(distName) };
    // GH#11096 - make sure the WSL path starts explicitly with
    // C:\Windows\System32. Don't want someone path hijacking wsl.exe.
    std::wstring command{};
    THROW_IF_FAILED(wil::GetSystemDirectoryW<std::wstring>(command));
    WSLDistro->Commandline(winrt::hstring{ command + L"\\wsl.exe -d " + distName });
    WSLDistro->DefaultAppearance().DarkColorSchemeName(L"Campbell");
    WSLDistro->DefaultAppearance().LightColorSchemeName(L"Campbell");
    if (isWslDashDashCdAvailableForLinuxPaths())
    {
        WSLDistro->StartingDirectory(winrt::hstring{ WslHomeDirectory });
    }
    else
    {
        WSLDistro->StartingDirectory(winrt::hstring{ DEFAULT_STARTING_DIRECTORY });
    }
    WSLDistro->Icon(L"ms-appx:///ProfileIcons/{9acb9455-ca41-5af7-950f-6bca1bc9722f}.png");
    return WSLDistro;
}

// Function Description:
// - Create a list of Profiles for each distro listed in names.
// - Skips distros that are utility distros for docker (see GH#3556)
// Arguments:
// - names: a list of distro names to turn into profiles
// Return Value:
// - the list of profiles we've generated.
static void namesToProfiles(const std::vector<std::wstring>& names, std::vector<winrt::com_ptr<implementation::Profile>>& profiles)
{
    for (const auto& distName : names)
    {
        if (til::starts_with(distName, DockerDistributionPrefix) || til::starts_with(distName, RancherDistributionPrefix))
        {
            // Docker for Windows and Rancher for Windows creates some utility distributions to handle Docker commands.
            // Pursuant to GH#3556, because they are _not_ user-facing we want to hide them.
            continue;
        }

        profiles.emplace_back(makeProfile(distName));
    }
}

// Function Description:
// - Open the reg key the root of the WSL data, in HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss
// Arguments:
// - <none>
// Return Value:
// - the HKEY if it exists and we can read it, else nullptr
static wil::unique_hkey openWslRegKey()
{
    HKEY hKey{ nullptr };
    if (RegOpenKeyEx(HKEY_CURRENT_USER, RegKeyLxss, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        return wil::unique_hkey{ hKey };
    }
    return nullptr;
}

// Function Description:
// - Open the reg key for a single distro, underneath the root WSL key.
// Arguments:
// - wslRootKey: the HKEY for the Lxss node.
// - guid: the string representation of the GUID for the distro to inspect
// Return Value:
// - the HKEY if it exists and we can read it, else nullptr
static wil::unique_hkey openDistroKey(const wil::unique_hkey& wslRootKey, const std::wstring& guid)
{
    HKEY hKey{ nullptr };
    if (RegOpenKeyEx(wslRootKey.get(), guid.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        return wil::unique_hkey{ hKey };
    }
    return nullptr;
}

// Function Description:
// - Get the list of all the guids of all the WSL distros from the registry. If
//   we fail to open or read the root reg key, we'll return false.
//   Places the guids of all the distros into the "guidStrings" param.
// Arguments:
// - wslRootKey: the HKEY for the Lxss node.
// - names: a vector that receives all the guids of the installed distros.
// Return Value:
// - false if we failed to enumerate all the WSL distros
static bool getWslGuids(const wil::unique_hkey& wslRootKey,
                        std::vector<std::wstring>& guidStrings)
{
    if (!wslRootKey)
    {
        return false;
    }

    wchar_t buffer[39]; // a {GUID} is 38 chars long
    for (DWORD i = 0;; i++)
    {
        DWORD length = 39;
        const auto result = RegEnumKeyEx(wslRootKey.get(), i, &buffer[0], &length, nullptr, nullptr, nullptr, nullptr);
        if (result == ERROR_NO_MORE_ITEMS)
        {
            break;
        }

        if (result == ERROR_SUCCESS &&
            length == 38 &&
            buffer[0] == L'{' &&
            buffer[37] == L'}')
        {
            guidStrings.emplace_back(&buffer[0], length);
        }
    }

    return true;
}

// Function Description:
// - Get the list of all the names of all the WSL distros from the registry. If
//   we fail to open any regkey for the GUID of a distro, we'll just skip it.
//   Places the names of all the distros into the "names" param.
// Arguments:
// - wslRootKey: the HKEY for the Lxss node.
// - guidStrings: A list of all the GUIDs of the installed distros
// - names: a vector that receives all the names of the installed distros.
// Return Value:
// - false if the root key was invalid, else true.
static bool getWslNames(const wil::unique_hkey& wslRootKey,
                        const std::vector<std::wstring>& guidStrings,
                        std::vector<std::wstring>& names)
{
    if (!wslRootKey)
    {
        return false;
    }
    for (const auto& guid : guidStrings)
    {
        auto distroKey{ openDistroKey(wslRootKey, guid) };
        if (!distroKey)
        {
            continue;
        }

        std::wstring buffer;
        auto result = wil::AdaptFixedSizeToAllocatedResult<std::wstring, 256>(buffer, [&](PWSTR value, size_t valueLength, size_t* valueLengthNeededWithNull) -> HRESULT {
            auto length = gsl::narrow<DWORD>(valueLength * sizeof(wchar_t));
            const auto status = RegQueryValueExW(distroKey.get(), RegKeyDistroName, 0, nullptr, reinterpret_cast<BYTE*>(value), &length);
            // length will receive the number of bytes including trailing null byte. Convert to a number of wchar_t's.
            // AdaptFixedSizeToAllocatedResult will then resize buffer to valueLengthNeededWithNull.
            // We're rounding up to prevent infinite loops if the data isn't a REG_SZ and length isn't divisible by 2.
            *valueLengthNeededWithNull = (length + sizeof(wchar_t) - 1) / sizeof(wchar_t);
            return status == ERROR_MORE_DATA ? S_OK : HRESULT_FROM_WIN32(status);
        });

        if (result != S_OK)
        {
            continue;
        }
        names.emplace_back(std::move(buffer));
    }
    return true;
}

// Method Description:
// - Generate a list of profiles for each on the installed WSL distros. This
//   will first try to read the installed distros from the registry. If that
//   fails, we'll assume there are no WSL distributions installed.
//   Reading the registry is slightly more stable (see GH#7199, GH#9905),
//   but it is certainly BODGY
// Arguments:
// - <none>
// Return Value:
// - A list of WSL profiles.
void WslDistroGenerator::GenerateProfiles(std::vector<winrt::com_ptr<implementation::Profile>>& profiles) const
{
    auto wslRootKey{ openWslRegKey() };
    if (wslRootKey)
    {
        std::vector<std::wstring> guidStrings{};
        if (getWslGuids(wslRootKey, guidStrings))
        {
            std::vector<std::wstring> names{};
            names.reserve(guidStrings.size());
            if (getWslNames(wslRootKey, guidStrings, names))
            {
                return namesToProfiles(names, profiles);
            }
        }
    }
}
