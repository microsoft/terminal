// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "SystemConfigurationProvider.hpp"

using namespace Microsoft::Console::Interactivity::OneCore;

UINT SystemConfigurationProvider::GetCaretBlinkTime() noexcept
{
    return s_DefaultCaretBlinkTime;
}

bool SystemConfigurationProvider::IsCaretBlinkingEnabled() noexcept
{
    return s_DefaultIsCaretBlinkingEnabled;
}

int SystemConfigurationProvider::GetNumberOfMouseButtons() noexcept
{
    if (IsGetSystemMetricsPresent())
    {
        return GetSystemMetrics(SM_CMOUSEBUTTONS);
    }
    else
    {
        return s_DefaultNumberOfMouseButtons;
    }
}

ULONG SystemConfigurationProvider::GetCursorWidth() noexcept
{
    return s_DefaultCursorWidth;
}

ULONG SystemConfigurationProvider::GetNumberOfWheelScrollLines() noexcept
{
    return s_DefaultNumberOfWheelScrollLines;
}

ULONG SystemConfigurationProvider::GetNumberOfWheelScrollCharacters() noexcept
{
    return s_DefaultNumberOfWheelScrollCharacters;
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

    pLinkSettings->SetFaceName(DEFAULT_TT_FONT_FACENAME);
    pLinkSettings->SetFontFamily(TMPF_TRUETYPE);

    return;
}
