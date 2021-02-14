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
    struct DelegationBase
    {
        CLSID clsid;
        std::wstring name;
        std::wstring author;
    };

    struct DelegationConsole : public DelegationBase
    {
    };

    struct DelegationTerminal : public DelegationBase
    {
    };

    [[nodiscard]] static HRESULT s_GetAvailableConsoles(std::vector<DelegationConsole>& consoles) noexcept;
    [[nodiscard]] static HRESULT s_GetAvailableTerminals(std::vector<DelegationTerminal>& terminals) noexcept;

    [[nodiscard]] static HRESULT s_SetConsole(const DelegationConsole& console) noexcept;
    [[nodiscard]] static HRESULT s_SetTerminal(const DelegationTerminal& terminal) noexcept;

    [[nodiscard]] static HRESULT s_GetConsole(IID& iid) noexcept;
    [[nodiscard]] static HRESULT s_GetTerminal(IID& iid) noexcept;

private:
    [[nodiscard]] static HRESULT s_Get(PCWSTR value, IID& iid) noexcept;
    [[nodiscard]] static HRESULT s_Set(PCWSTR value, const CLSID clsid) noexcept;
};
