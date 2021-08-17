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
std::vector<Profile> _legacyGenerate()
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
std::vector<Profile> _namesToProfiles(const std::vector<std::wstring>& names)
{
    std::vector<Profile> profiles;
    for (const auto& distName : names)
    {
        if (distName.substr(0, std::min(distName.size(), DockerDistributionPrefix.size())) == DockerDistributionPrefix)
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

// The WSL entries are structured as such:
// HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss
//   ⌞ {distroGuid}
//     ⌞ DistributionName: {the name}
const wchar_t RegKeyLxss[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Lxss";
const wchar_t RegKeyDistroName[] = L"DistributionName";

// Function Description:
// - Open the reg key the root of the WSL data, in HKCU\Software\Microsoft\Windows\CurrentVersion\Lxss
// Arguments:
// - <none>
// Return Value:
// - the HKEY if it exists and we can read it, else nullptr
HKEY _openWslRegKey()
{
    HKEY hKey{ nullptr };
    if (RegOpenKeyEx(HKEY_CURRENT_USER, RegKeyLxss, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
    {
        return hKey;
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
HKEY _openDistroKey(const wil::unique_hkey& wslRootKey, const std::wstring& guid)
{
    HKEY hKey{ nullptr };
    if (RegOpenKeyEx(wslRootKey.get(), guid.c_str(), 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
    {
        return hKey;
    }
    return nullptr;
}

// Function Description:
// - Get the list of all the guids of all the WSL distros from the registry. If
//   we fail to open or read the root reg key, we'll return false.
//   Places the guids of all the distros into the "guidStrings" param.
// Arguments:
// - wslRootKey: the HKEY for the Lxss node.
// - names: a vector that recieves all the guids of the installed distros.
// Return Value:
// - false if we failed to enumerate all the WSL distros
bool _getWslGuids(const wil::unique_hkey& wslRootKey,
                  std::vector<std::wstring>& guidStrings)
{
    if (!wslRootKey)
    {
        return false;
    }

    // Figure out how many subkeys we have, and what the longest name of these subkeys is.
    DWORD dwNumSubKeys = 0;
    DWORD maxSubKeyLen = 0;
    if (RegQueryInfoKey(wslRootKey.get(), // hKey,
                        nullptr, // lpClass,
                        nullptr, // lpcchClass,
                        nullptr, // lpReserved,
                        &dwNumSubKeys, // lpcSubKeys,
                        &maxSubKeyLen, // lpcbMaxSubKeyLen,
                        nullptr, // lpcbMaxClassLen,
                        nullptr, // lpcValues,
                        nullptr, // lpcbMaxValueNameLen,
                        nullptr, // lpcbMaxValueLen,
                        nullptr, // lpcbSecurityDescriptor,
                        nullptr // lpftLastWriteTime
                        ) != ERROR_SUCCESS)
    {
        return false;
    }

    // lpcbMaxSubKeyLen does not include trailing space
    std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>(maxSubKeyLen + 1);

    // reserve enough space for all the GUIDs we're about to enumerate
    guidStrings.reserve(dwNumSubKeys);
    for (DWORD i = 0; i < dwNumSubKeys; i++)
    {
        auto cbName = maxSubKeyLen + 1;
        const auto result = RegEnumKeyEx(wslRootKey.get(), i, buffer.get(), &cbName, nullptr, nullptr, nullptr, nullptr);
        if (result == ERROR_SUCCESS)
        {
            guidStrings.emplace_back(buffer.get(), cbName);
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
// - names: a vector that recieves all the names of the installed distros.
// Return Value:
// - false if the root key was invalid, else true.
bool _getWslNames(const wil::unique_hkey& wslRootKey,
                  std::vector<std::wstring>& guidStrings,
                  std::vector<std::wstring>& names)
{
    if (!wslRootKey)
    {
        return false;
    }
    for (const auto& guid : guidStrings)
    {
        wil::unique_hkey distroKey{ _openDistroKey(wslRootKey, guid) };
        if (!distroKey)
        {
            // return false;
            continue;
        }

        DWORD bufferCapacity = 0;
        auto result = RegQueryValueEx(distroKey.get(), RegKeyDistroName, 0, nullptr, nullptr, &bufferCapacity);
        // request regkey binary buffer capacity only
        if (result != ERROR_SUCCESS)
        {
            // return false;
            continue;
        }
        std::unique_ptr<wchar_t[]> buffer = std::make_unique<wchar_t[]>(bufferCapacity);
        // request regkey binary content
        result = RegQueryValueEx(distroKey.get(), RegKeyDistroName, 0, nullptr, reinterpret_cast<BYTE*>(buffer.get()), &bufferCapacity);
        if (result != ERROR_SUCCESS)
        {
            // return false;
            continue;
        }

        names.emplace_back(buffer.get());
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
    static wil::unique_hkey wslRootKey{ _openWslRegKey() };
    if (wslRootKey)
    {
        std::vector<std::wstring> guidStrings{};
        if (_getWslGuids(wslRootKey, guidStrings))
        {
            std::vector<std::wstring> names{};
            names.reserve(guidStrings.size());
            if (_getWslNames(wslRootKey, guidStrings, names))
            {
                return _namesToProfiles(names);
            }
        }
    }

    return _legacyGenerate();
}
