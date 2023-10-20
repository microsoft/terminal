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
    enum class DelegationPairKind : uint32_t
    {
        Undecided,
        Default,
        Conhost,
        Custom,
    };

    struct DelegationPair
    {
        // state contains a "pre parsed" idea of what console/terminal mean.
        // This might help make some code more readable.
        // If either are CLSID_Default, state will be Default.
        // If either are CLSID_Conhost, state will be Conhost.
        // Otherwise it'll be Custom.
        DelegationPairKind kind = DelegationPairKind::Undecided;
        CLSID console{};
        CLSID terminal{};

        constexpr bool IsUndecided() const noexcept { return kind == DelegationPairKind::Undecided; }
        constexpr bool IsDefault() const noexcept { return kind == DelegationPairKind::Default; }
        constexpr bool IsConhost() const noexcept { return kind == DelegationPairKind::Conhost; }
        constexpr bool IsCustom() const noexcept { return kind == DelegationPairKind::Custom; }

        constexpr bool operator==(const DelegationPair& other) const noexcept
        {
            static_assert(std::has_unique_object_representations_v<DelegationPair>);
            return __builtin_memcmp(this, &other, sizeof(*this)) == 0;
        }
    };

    struct PkgVersion
    {
        unsigned short major = 0;
        unsigned short minor = 0;
        unsigned short build = 0;
        unsigned short revision = 0;

        constexpr bool operator==(const PkgVersion& other) const noexcept
        {
            static_assert(std::has_unique_object_representations_v<PkgVersion>);
            return __builtin_memcmp(this, &other, sizeof(*this)) == 0;
        }
    };

    struct PackageInfo
    {
        std::wstring name;
        std::wstring author;
        std::wstring pfn;
        std::wstring logo;
        PkgVersion version;

        bool IsFromSamePackage(const PackageInfo& other) const noexcept
        {
            return name == other.name &&
                   author == other.author &&
                   pfn == other.pfn &&
                   version == other.version;
        }
    };

    struct DelegationBase
    {
        CLSID clsid{};
        PackageInfo info;
    };

    struct DelegationPackage
    {
        DelegationPair pair;
        PackageInfo info;

        bool operator==(const DelegationPackage& other) const noexcept
        {
            return pair == other.pair;
        }
    };

    [[nodiscard]] static HRESULT s_GetAvailablePackages(std::vector<DelegationPackage>& packages, DelegationPackage& def) noexcept;

    [[nodiscard]] static HRESULT s_SetDefaultByPackage(const DelegationPackage& pkg) noexcept;

    static constexpr CLSID CLSID_Default{};
    static constexpr CLSID CLSID_Conhost{ 0xb23d10c0, 0xe52e, 0x411e, { 0x9d, 0x5b, 0xc0, 0x9f, 0xdf, 0x70, 0x9c, 0x7d } };
    static constexpr CLSID CLSID_WindowsTerminalConsole{ 0x2eaca947, 0x7f5f, 0x4cfa, { 0xba, 0x87, 0x8f, 0x7f, 0xbe, 0xef, 0xbe, 0x69 } };
    static constexpr CLSID CLSID_WindowsTerminalTerminal{ 0xe12cff52, 0xa866, 0x4c77, { 0x9a, 0x90, 0xf5, 0x70, 0xa7, 0xaa, 0x2c, 0x6b } };
    static constexpr CLSID CLSID_WindowsTerminalConsoleDev{ 0x1f9f2bf5, 0x5bc3, 0x4f17, { 0xb0, 0xe6, 0x91, 0x24, 0x13, 0xf1, 0xf4, 0x51 } };
    static constexpr CLSID CLSID_WindowsTerminalTerminalDev{ 0x051f34ee, 0xc1fd, 0x4b19, { 0xaf, 0x75, 0x9b, 0xa5, 0x46, 0x48, 0x43, 0x4c } };
    static constexpr DelegationPair DefaultDelegationPair{ DelegationPairKind::Default, CLSID_Default, CLSID_Default };
    static constexpr DelegationPair ConhostDelegationPair{ DelegationPairKind::Conhost, CLSID_Conhost, CLSID_Conhost };
    static constexpr DelegationPair TerminalDelegationPair{ DelegationPairKind::Custom, CLSID_WindowsTerminalConsole, CLSID_WindowsTerminalTerminal };

    [[nodiscard]] static DelegationPair s_GetDelegationPair() noexcept;

private:
    [[nodiscard]] static HRESULT s_SetDefaultConsoleById(const IID& iid) noexcept;
    [[nodiscard]] static HRESULT s_SetDefaultTerminalById(const IID& iid) noexcept;

    [[nodiscard]] static HRESULT s_Set(PCWSTR value, const CLSID clsid) noexcept;
};
