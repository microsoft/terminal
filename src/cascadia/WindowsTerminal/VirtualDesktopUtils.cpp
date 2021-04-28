// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Shamelessly copied from microsoft/PowerToys, at
// https://github.com/microsoft/PowerToys/blob/master/src/modules/fancyzones/lib/VirtualDesktopUtils.cpp
//
// The code style is left (relatively) untouched, as to make contributions
// from/to the upstream source easier. `NewGetCurrentDesktopId` was added in
// April 2021.

#include "pch.h"

#include "VirtualDesktopUtils.h"

// Non-Localizable strings
namespace NonLocalizable
{
    const wchar_t RegCurrentVirtualDesktop[] = L"CurrentVirtualDesktop";
    const wchar_t RegVirtualDesktopIds[] = L"VirtualDesktopIDs";
    const wchar_t RegKeyVirtualDesktops[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\VirtualDesktops";
    const wchar_t RegKeyVirtualDesktopsFromSession[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\SessionInfo\\%d\\VirtualDesktops";
}

namespace VirtualDesktopUtils
{
    // Look for the guid stored as the value `CurrentVirtualDesktop` under the
    // key
    // `HKCU\Software\Microsoft\Windows\CurrentVersion\Explorer\VirtualDesktops`
    bool NewGetCurrentDesktopId(GUID* desktopId)
    {
        wil::unique_hkey key{};
        if (RegOpenKeyExW(HKEY_CURRENT_USER, NonLocalizable::RegKeyVirtualDesktops, 0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS)
        {
            GUID value{};
            DWORD size = sizeof(GUID);
            if (RegQueryValueExW(key.get(), NonLocalizable::RegCurrentVirtualDesktop, 0, nullptr, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS)
            {
                *desktopId = value;
                return true;
            }
        }
        return false;
    }

    bool GetDesktopIdFromCurrentSession(GUID* desktopId)
    {
        DWORD sessionId;
        if (!ProcessIdToSessionId(GetCurrentProcessId(), &sessionId))
        {
            return false;
        }

        wchar_t sessionKeyPath[256]{};
        if (FAILED(StringCchPrintfW(sessionKeyPath, ARRAYSIZE(sessionKeyPath), NonLocalizable::RegKeyVirtualDesktopsFromSession, sessionId)))
        {
            return false;
        }

        wil::unique_hkey key{};
        if (RegOpenKeyExW(HKEY_CURRENT_USER, sessionKeyPath, 0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS)
        {
            GUID value{};
            DWORD size = sizeof(GUID);
            if (RegQueryValueExW(key.get(), NonLocalizable::RegCurrentVirtualDesktop, 0, nullptr, reinterpret_cast<BYTE*>(&value), &size) == ERROR_SUCCESS)
            {
                *desktopId = value;
                return true;
            }
        }
        return false;
    }

    bool GetVirtualDesktopIds(HKEY hKey, std::vector<GUID>& ids)
    {
        if (!hKey)
        {
            return false;
        }
        DWORD bufferCapacity;
        // request regkey binary buffer capacity only
        if (RegQueryValueExW(hKey, NonLocalizable::RegVirtualDesktopIds, 0, nullptr, nullptr, &bufferCapacity) != ERROR_SUCCESS)
        {
            return false;
        }
        std::unique_ptr<BYTE[]> buffer = std::make_unique<BYTE[]>(bufferCapacity);
        // request regkey binary content
        if (RegQueryValueExW(hKey, NonLocalizable::RegVirtualDesktopIds, 0, nullptr, buffer.get(), &bufferCapacity) != ERROR_SUCCESS)
        {
            return false;
        }
        const size_t guidSize = sizeof(GUID);
        std::vector<GUID> temp;
        temp.reserve(bufferCapacity / guidSize);
        for (size_t i = 0; i < bufferCapacity; i += guidSize)
        {
            GUID* guid = reinterpret_cast<GUID*>(buffer.get() + i);
            temp.push_back(*guid);
        }
        ids = std::move(temp);
        return true;
    }

    HKEY OpenVirtualDesktopsRegKey()
    {
        HKEY hKey{ nullptr };
        if (RegOpenKeyEx(HKEY_CURRENT_USER, NonLocalizable::RegKeyVirtualDesktops, 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
        {
            return hKey;
        }
        return nullptr;
    }

    HKEY GetVirtualDesktopsRegKey()
    {
        static wil::unique_hkey virtualDesktopsKey{ OpenVirtualDesktopsRegKey() };
        return virtualDesktopsKey.get();
    }
    bool GetVirtualDesktopIds(std::vector<GUID>& ids)
    {
        return GetVirtualDesktopIds(GetVirtualDesktopsRegKey(), ids);
    }

    bool GetVirtualDesktopIds(std::vector<std::wstring>& ids)
    {
        std::vector<GUID> guids{};
        if (GetVirtualDesktopIds(guids))
        {
            for (auto& guid : guids)
            {
                wil::unique_cotaskmem_string guidString;
                if (SUCCEEDED(StringFromCLSID(guid, &guidString)))
                {
                    ids.push_back(guidString.get());
                }
            }
            return true;
        }
        return false;
    }

    bool GetCurrentVirtualDesktopId(GUID* desktopId)
    {
        // BODGY
        // On newer Windows builds, the current virtual desktop is persisted to
        // a totally different reg key. Look there first.
        if (NewGetCurrentDesktopId(desktopId))
        {
            return true;
        }

        // Explorer persists current virtual desktop identifier to registry on a per session basis, but only
        // after first virtual desktop switch happens. If the user hasn't switched virtual desktops in this
        // session, value in registry will be empty.
        if (GetDesktopIdFromCurrentSession(desktopId))
        {
            return true;
        }
        // Fallback scenario is to get array of virtual desktops stored in registry, but not kept per session.
        // Note that we are taking first element from virtual desktop array, which is primary desktop.
        // If user has more than one virtual desktop, previous function should return correct value, as desktop
        // switch occurred in current session.
        else
        {
            std::vector<GUID> ids{};
            if (GetVirtualDesktopIds(ids) && ids.size() > 0)
            {
                *desktopId = ids[0];
                return true;
            }
        }
        return false;
    }
}
