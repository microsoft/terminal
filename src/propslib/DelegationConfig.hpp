/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- DelegationConfig.hpp

Abstract:
- This module is used for looking up delegation handlers for the launch of the default console hosting environment

Author(s):
- Michael Niksa (MiNiksa) 31-Aug-2020

--*/

#pragma once

class DelegationConfig
{
public:
    struct PkgVersion
    {
        unsigned short major;
        unsigned short minor;
        unsigned short build;
        unsigned short revision;

        bool operator==(const PkgVersion& other) const
        {
            return major == other.major &&
                   minor == other.minor &&
                   build == other.build &&
                   revision == other.revision;
        }
    };

    struct DelegationBase
    {
        CLSID clsid;
        std::wstring name;
        std::wstring author;
        std::wstring pfn;
        std::wstring logo;
        PkgVersion version;

        bool IsFromSamePackage(const DelegationBase& other) const
        {
            return name == other.name &&
                   author == other.author &&
                   pfn == other.pfn &&
                   version == other.version;
        }

        bool operator==(const DelegationBase& other) const
        {
            return clsid == other.clsid &&
                   name == other.name &&
                   author == other.author &&
                   pfn == other.pfn &&
                   version == other.version;
        }
    };

    struct DelegationConsole : public DelegationBase
    {
    };

    struct DelegationTerminal : public DelegationBase
    {
    };

    struct DelegationPackage
    {
        DelegationConsole console;
        DelegationTerminal terminal;

        bool operator==(const DelegationPackage& other) const
        {
            return console == other.console &&
                   terminal == other.terminal;
        }
    };

    [[nodiscard]] static HRESULT s_GetAvailablePackages(std::vector<DelegationPackage>& packages, DelegationPackage& def) noexcept;

    [[nodiscard]] static HRESULT s_SetDefaultByPackage(const DelegationPackage& pkg, const bool useRegExe = false) noexcept;

    [[nodiscard]] static HRESULT s_GetDefaultConsoleId(IID& iid) noexcept;
    [[nodiscard]] static HRESULT s_GetDefaultTerminalId(IID& iid) noexcept;

private:
    [[nodiscard]] static HRESULT s_GetAvailableConsoles(std::vector<DelegationConsole>& consoles) noexcept;
    [[nodiscard]] static HRESULT s_GetAvailableTerminals(std::vector<DelegationTerminal>& terminals) noexcept;

    [[nodiscard]] static HRESULT s_SetDefaultConsoleById(const IID& iid, const bool useRegExe) noexcept;
    [[nodiscard]] static HRESULT s_SetDefaultTerminalById(const IID& iid, const bool useRegExe) noexcept;

    [[nodiscard]] static HRESULT s_Get(PCWSTR value, IID& iid) noexcept;
    [[nodiscard]] static HRESULT s_Set(PCWSTR value, const CLSID clsid, const bool useRegExe) noexcept;
};
