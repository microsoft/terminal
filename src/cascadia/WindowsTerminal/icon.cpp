// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "resource.h"

static int _GetActiveAppIconResource()
{
    auto iconResource{ IDI_APPICON };

    HIGHCONTRASTW hcInfo{};
    hcInfo.cbSize = sizeof(hcInfo);

    if (SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hcInfo), &hcInfo, 0))
    {
        if (WI_IsFlagSet(hcInfo.dwFlags, HCF_HIGHCONTRASTON))
        {
            iconResource = IDI_APPICON_HC_BLACK;

            if (0x00FFFFFF == GetSysColor(COLOR_WINDOW)) // white window color == white high contrast
            {
                iconResource = IDI_APPICON_HC_WHITE;
            }
        }
    }

    return iconResource;
}

// There's only two possible sizes - ICON_SMALL and ICON_BIG.
// So, use true for smallIcon if you want small and false for big.
HANDLE GetActiveAppIconHandle(bool smallIcon)
{
    auto iconResource{ MAKEINTRESOURCEW(_GetActiveAppIconResource()) };

    const auto smXIcon = smallIcon ? SM_CXSMICON : SM_CXICON;
    const auto smYIcon = smallIcon ? SM_CYSMICON : SM_CYICON;

    // These handles are loaded with LR_SHARED, so they are safe to "leak".
    auto hIcon{ LoadImageW(wil::GetModuleInstanceHandle(), iconResource, IMAGE_ICON, GetSystemMetrics(smXIcon), GetSystemMetrics(smYIcon), LR_SHARED) };
    LOG_LAST_ERROR_IF_NULL(hIcon);

    return hIcon;
}

void UpdateWindowIconForActiveMetrics(HWND window)
{
    if (auto smallIcon = GetActiveAppIconHandle(true))
    {
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }
    if (auto largeIcon = GetActiveAppIconHandle(false))
    {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
    }
}
