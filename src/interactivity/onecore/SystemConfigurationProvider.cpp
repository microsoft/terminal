// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "SystemConfigurationProvider.hpp"

using namespace Microsoft::Console::Interactivity::OneCore;

UINT SystemConfigurationProvider::GetCaretBlinkTime() noexcept
{
    return 530; // milliseconds
}

bool SystemConfigurationProvider::IsCaretBlinkingEnabled() noexcept
{
    return true;
}

int SystemConfigurationProvider::GetNumberOfMouseButtons() noexcept
{
    if (IsGetSystemMetricsPresent())
    {
        return GetSystemMetrics(SM_CMOUSEBUTTONS);
    }
    else
    {
        return 3;
    }
}

ULONG SystemConfigurationProvider::GetCursorWidth() noexcept
{
    return 1;
}

ULONG SystemConfigurationProvider::GetNumberOfWheelScrollLines() noexcept
{
    return 3;
}

ULONG SystemConfigurationProvider::GetNumberOfWheelScrollCharacters() noexcept
{
    return 3;
}

void SystemConfigurationProvider::GetSettingsFromLink(
    _Inout_ Settings* pLinkSettings,
    _Inout_updates_bytes_(*pdwTitleLength) LPWSTR /*pwszTitle*/,
    _Inout_ PDWORD /*pdwTitleLength*/,
    _In_ PCWSTR /*pwszCurrDir*/,
    _In_ PCWSTR /*pwszAppName*/,
    _Inout_opt_ IconInfo* /*iconInfo*/)
{
    // While both OneCore console renderers use TrueType fonts, there is no
    // advanced font support on that platform. Namely, there is no way to pick
    // neither the font nor the font size. Since this choice of TrueType font
    // is made implicitly by the renderers, the rest of the console is not aware
    // of it and the renderer procedure goes on to translate output text so that
    // it be renderable with raster fonts, which messes up the final output.
    // Hence, we make it seem like the console is in fact configured to use a
    // TrueType font by the user.

#pragma warning(suppress : 26485) // This isn't even really _supposed to be_ an array-to-pointer decay: it's passed as a string view.
    pLinkSettings->SetFaceName(DEFAULT_TT_FONT_FACENAME);
    pLinkSettings->SetFontFamily(TMPF_TRUETYPE);

    return;
}
