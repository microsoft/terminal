// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "WslDistroGenerator.h"
#include "LegacyProfileGeneratorNamespaces.h"

#include "../../types/inc/utils.hpp"
#include "../../inc/DefaultSettings.h"
#include "Utils.h"
#include <io.h>
#include <fcntl.h>
#include "DefaultProfileUtils.h"

static constexpr std::wstring_view DockerDistributionPrefix{ L"docker-desktop" };

// The WSL entries are structured as such:
// HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss
//   ⌞ {distroGuid}
//     ⌞ DistributionName: {the name}
static constexpr wchar_t RegKeyLxss[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss";
static constexpr wchar_t RegKeyDistroName[] = L"DistributionName";

using namespace ::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Settings::Model;

// Legacy GUIDs:
//   - Debian       58ad8b0c-3ef8-5f4d-bc6f-13e4c00f2530
//   - Ubuntu       2c4de342-38b7-51cf-b940-2309a097f518
//   - Alpine       1777cdf0-b2c4-5a63-a204-eb60f349ea7c
//   - Ubuntu-18.04 c6eaf9f4-32a7-5fdc-b5cf-066e8a4b1e40

std::wstring_view WslDistroGenerator::GetNamespace()
{
    return WslGeneratorNamespace;
}

// Method Description:
// -  Enumerates all the installed WSL distros to create profiles for them.
// Arguments:
// - <none>
// Return Value:
// - a vector with all distros for all the installed WSL distros
static std::vector<Profile> legacyGenerate()
{
    std::vector<Profile> profiles;

    wil::unique_handle readPipe;
    wil::unique_handle writePipe;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, true };
    THROW_IF_WIN32_BOOL_FALSE(CreatePipe(&readPipe, &writePipe, &sa, 0));
    STARTUPINFO si{ 0 };
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe.get();
    si.hStdError = writePipe.get();
    wil::unique_process_information pi;
    wil::unique_cotaskmem_string systemPath;
    THROW_IF_FAILED(wil::GetSystemDirectoryW(systemPath));
    std::wstring command(systemPath.get());
    command += L"\\wsl.exe --list";

    THROW_IF_WIN32_BOOL_FALSE(CreateProcessW(nullptr,
                                             const_cast<LPWSTR>(command.c_str()),
                                             nullptr,
                                             nullptr,
                                             TRUE,
                                             CREATE_NO_WINDOW,
                                             nullptr,
                                             nullptr,
                                             &si,
                                             &pi));
    switch (WaitForSingleObject(pi.hProcess, 2000))
    {
    case WAIT_OBJECT_0:
        break;
    case WAIT_ABANDONED:
    case WAIT_TIMEOUT:
        return profiles;
    case WAIT_FAILED:
        THROW_LAST_ERROR();
    default:
        THROW_HR(ERROR_UNHANDLED_EXCEPTION);
    }
    DWORD exitCode;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode))
    {
        THROW_HR(E_INVALIDARG);
    }
    else if (exitCode != 0)
    {
        return profiles;
    }
    DWORD bytesAvailable;
    THROW_IF_WIN32_BOOL_FALSE(PeekNamedPipe(readPipe.get(), nullptr, NULL, nullptr, &bytesAvailable, nullptr));
    // "The _open_osfhandle call transfers ownership of the Win32 file handle to the file descriptor."
    // (https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/open-osfhandle?view=vs-2019)
    // so, we detach_from_smart_pointer it -- but...
    // "File descriptors passed into _fdopen are owned by the returned FILE * stream.
    // If _fdopen is successful, do not call _close on the file descriptor.
    // Calling fclose on the returned FILE * also closes the file descriptor."
    // https://docs.microsoft.com/en-us/cpp/c-runtime-library/reference/fdopen-wfdopen?view=vs-2019
    FILE* stdioPipeHandle = _wfdopen(_open_osfhandle((intptr_t)wil::detach_from_smart_pointer(readPipe), _O_WTEXT | _O_RDONLY), L"r");
    auto closeFile = wil::scope_exit([&]() { fclose(stdioPipeHandle); });

    std::wfstream pipe{ stdioPipeHandle };

    std::wstring wline;
    std::getline(pipe, wline); // remove the header from the output.
    while (pipe.tellp() < bytesAvailable)
    {
        std::getline(pipe, wline);
        std::wstringstream wlinestream(wline);
        if (wlinestream)
        {
            std::wstring distName;
            std::getline(wlinestream, distName, L'\r');

            if (distName.substr(0, std::min(distName.size(), DockerDistributionPrefix.size())) == DockerDistributionPrefix)
            {
                // Docker for Windows creates some utility distributions to handle Docker commands.
                // Pursuant to GH#3556, because they are _not_ user-facing we want to hide them.
                continue;
            }

            const size_t firstChar = distName.find_first_of(L"( ");
            // Some localizations don't have a space between the name and "(Default)"
            // https://github.com/microsoft/terminal/issues/1168#issuecomment-500187109
            if (firstChar < distName.size())
            {
                distName.resize(firstChar);
            }
            auto WSLDistro{ CreateDefaultProfile(distName) };

            WSLDistro.Commandline(L"wsl.exe -d " + distName);
            WSLDistro.DefaultAppearance().ColorSchemeName(L"Campbell");
            WSLDistro.StartingDirectory(DEFAULT_STARTING_DIRECTORY);
            WSLDistro.Icon(L"ms-appx:///ProfileIcons/{9acb9455-ca41-5af7-950f-6bca1bc9722f}.png");
            profiles.emplace_back(WSLDistro);
        }
    }

    return profiles;
}

// Function Description:
// - Create a list of Profiles for each distro listed in names.
// - Skips distros that are utility distros for docker (see GH#3556)
// Arguments:
// - names: a list of distro names to turn into profiles
// Return Value:
// - the list of profiles we've generated.
static std::vector<Profile> namesToProfiles(const std::vector<std::wstring>& names)
{
    std::vector<Profile> profiles;
    for (const auto& distName : names)
    {
        if (til::starts_with(distName, DockerDistributionPrefix))
        {
            // Docker for Windows creates some utility distributions to handle Docker commands.
            // Pursuant to GH#3556, because they are _not_ user-facing we want to hide them.
            continue;
        }

        auto WSLDistro{ CreateDefaultProfile(distName) };

        WSLDistro.Commandline(L"wsl.exe -d " + distName);
        WSLDistro.DefaultAppearance().ColorSchemeName(L"Campbell");
        WSLDistro.StartingDirectory(DEFAULT_STARTING_DIRECTORY);
        WSLDistro.Icon(L"ms-appx:///ProfileIcons/{9acb9455-ca41-5af7-950f-6bca1bc9722f}.png");
        profiles.emplace_back(WSLDistro);
    }
    return profiles;
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
        wil::unique_hkey distroKey{ openDistroKey(wslRootKey, guid) };
        if (!distroKey)
        {
            continue;
        }

        std::wstring buffer;
        auto result = wil::AdaptFixedSizeToAllocatedResult<std::wstring, 256>(buffer, [&](PWSTR value, size_t valueLength, size_t* valueLengthNeededWithNull) -> HRESULT {
            auto length = static_cast<DWORD>(valueLength);
            const auto status = RegQueryValueExW(distroKey.get(), RegKeyDistroName, 0, nullptr, reinterpret_cast<BYTE*>(value), &length);
            // length will receive the number of bytes - convert to a number of
            // wchar_t's. AdaptFixedSizeToAllocatedResult will resize buffer to
            // valueLengthNeededWithNull
            *valueLengthNeededWithNull = (length / sizeof(wchar_t));
            // If you add one for another trailing null, then there'll actually
            // be _two_ trailing nulls in the buffer.
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
//   fails, we'll fall back to the legacy way of launching WSL.exe to read the
//   distros from the commandline. Reading the registry is slightly more stable
//   (see GH#7199, GH#9905), but it is certainly BODGY
// Arguments:
// - <none>
// Return Value:
// - A list of WSL profiles.
std::vector<Profile> WslDistroGenerator::GenerateProfiles()
{
    wil::unique_hkey wslRootKey{ openWslRegKey() };
    if (wslRootKey)
    {
        std::vector<std::wstring> guidStrings{};
        if (getWslGuids(wslRootKey, guidStrings))
        {
            std::vector<std::wstring> names{};
            names.reserve(guidStrings.size());
            if (getWslNames(wslRootKey, guidStrings, names))
            {
                return namesToProfiles(names);
            }
        }
    }

    return legacyGenerate();
}
