/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- windowtheme.hpp

Abstract:
- This module is used for abstracting calls to set window themes.

Author(s):
- Michael Niksa (MiNiksa) Oct-2018
--*/
#pragma once

namespace Microsoft::Console::Interactivity::Win32
{
    class WindowTheme final
    {
    public:
        WindowTheme();

        [[nodiscard]] HRESULT TrySetDarkMode(HWND hwnd) const noexcept;

    private:
        bool _IsDarkMode() const noexcept;

        bool _IsHighContrast() const noexcept;
        bool _ShouldAppsUseDarkMode() const noexcept;

        wil::unique_hmodule _module;
    };
}
