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

float FontInfo::GetFontSizeInPt() const noexcept
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

void FontInfo::SetFromEngine(std::wstring faceName,
                             unsigned char family,
                             unsigned int weight,
                             unsigned int codePage,
                             float fontSizeInPt,
                             CellSizeInDIP cellSizeInDIP,
                             til::size cellSizeInPhysicalPx) noexcept
{
    _faceName = std::move(faceName);
    _family = family;
    _weight = weight;
    _codePage = codePage;
    _fontSizeInPt = fontSizeInPt;
    _cellSizeInDIP = cellSizeInDIP;
    _cellSizeInPhysicalPx = cellSizeInPhysicalPx;
}

bool FontInfo::IsTrueTypeFont() const noexcept
{
    return WI_IsFlagSet(_family, TMPF_TRUETYPE);
}
