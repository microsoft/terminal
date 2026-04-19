// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/FontInfo.hpp"

// Truncates this size in DIPs to integers.
//
// "Do Not Use" because the conversion is lossy and doesn't roundtrip.
// It exists because we have legacy code and this is a discoverable "marker".
til::size CellSizeInDIP::AsInteger_DoNotUse() const noexcept
{
    return { til::math::rounding, width, height };
}

float FontInfo::GetFontSizeInDIP() const noexcept
{
    return _fontSizeInPt;
}

CellSizeInDIP FontInfo::GetCellSizeInDIP() const noexcept
{
    return _cellSizeInDIP;
}

til::size FontInfo::GetCellSizeInPhysicalPx() const noexcept
{
    return _cellSizeInPhysicalPx;
}

void FontInfo::SetFontSizeInPt(float fontSizeInPt) noexcept
{
    _fontSizeInPt = fontSizeInPt;
}

void FontInfo::SetCellSizeInDIP(CellSizeInDIP cellSizeInDIP) noexcept
{
    _cellSizeInDIP = cellSizeInDIP;
}

void FontInfo::SetCellSizeInPhysicalPx(til::size cellSizeInPhysicalPx) noexcept
{
    _cellSizeInPhysicalPx = cellSizeInPhysicalPx;
}

bool FontInfo::IsTrueTypeFont() const noexcept
{
    return WI_IsFlagSet(_family, TMPF_TRUETYPE);
}
