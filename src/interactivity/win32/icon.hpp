/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- icon.hpp

Abstract:
- This module is used for managing icons

Author(s):
- Michael Niksa (miniksa) 14-Oct-2014
- Paul Campbell (paulcam) 14-Oct-2014
--*/

#pragma once

namespace Microsoft::Console::Interactivity::Win32
{
    class Icon sealed
    {
    public:
        static Icon& Instance();

        [[nodiscard]] HRESULT GetIcons(int dpi, _Out_opt_ HICON* const phIcon, _Out_opt_ HICON* const phSmIcon);
        [[nodiscard]] HRESULT LoadIconsFromPath(_In_ PCWSTR pwszIconLocation, const int nIconIndex);
        [[nodiscard]] HRESULT ApplyIconsToWindow(const HWND hwnd);

    protected:
        Icon();
        ~Icon() = default;
        Icon(const Icon&) = delete;
        void operator=(const Icon&) = delete;

    private:
        [[nodiscard]] HRESULT LoadIconsForDpi(int dpi);
        // We are not using unique_hicon for these, as they are loaded from our mapped executable image.
        HICON _hDefaultIcon{ nullptr };
        HICON _hDefaultSmIcon{ nullptr };

        std::unordered_map<int, std::pair<wil::unique_hicon, wil::unique_hicon>> _iconHandlesPerDpi;
        std::pair<std::wstring, int> _iconPathAndIndex;
    };
}
