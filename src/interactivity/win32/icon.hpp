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

        [[nodiscard]] HRESULT GetIcons(_Out_opt_ HICON* const phIcon, _Out_opt_ HICON* const phSmIcon);
        [[nodiscard]] HRESULT SetIcons(const HICON hIcon, const HICON hSmIcon);

        [[nodiscard]] HRESULT LoadIconsFromPath(_In_ PCWSTR pwszIconLocation, const int nIconIndex);

        [[nodiscard]] HRESULT ApplyWindowMessageWorkaround(const HWND hwnd);

    protected:
        Icon();
        ~Icon();
        Icon(Icon const&) = delete;
        void operator=(Icon const&) = delete;

    private:
        [[nodiscard]] HRESULT _Initialize();

        void _DestroyNonDefaultIcons();

        // Helper methods
        [[nodiscard]] HRESULT _GetAvailableIconFromReference(_In_ HICON& hIconRef, _In_ HICON& hDefaultIconRef, _Out_ HICON* const phIcon);
        [[nodiscard]] HRESULT _GetDefaultIconFromReference(_In_ HICON& hIconRef, _Out_ HICON* const phIcon);
        [[nodiscard]] HRESULT _SetIconFromReference(_In_ HICON& hIconRef, const HICON hNewIcon);
        void _FreeIconFromReference(_In_ HICON& hIconRef);

        bool _fInitialized;
        HICON _hDefaultIcon;
        HICON _hDefaultSmIcon;
        HICON _hIcon;
        HICON _hSmIcon;
    };
}
