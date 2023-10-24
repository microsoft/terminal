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

void UpdateWindowIconForActiveMetrics(HWND window)
{
    auto iconResource{ MAKEINTRESOURCEW(_GetActiveAppIconResource()) };

    // These handles are loaded with LR_SHARED, so they are safe to "leak".
    HANDLE smallIcon{ LoadImageW(wil::GetModuleInstanceHandle(), iconResource, IMAGE_ICON, GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED) };
    LOG_LAST_ERROR_IF_NULL(smallIcon);

    HANDLE largeIcon{ LoadImageW(wil::GetModuleInstanceHandle(), iconResource, IMAGE_ICON, GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_SHARED) };
    LOG_LAST_ERROR_IF_NULL(largeIcon);

    if (smallIcon)
    {
        SendMessageW(window, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
    }
    if (largeIcon)
    {
        SendMessageW(window, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(largeIcon));
    }
}
