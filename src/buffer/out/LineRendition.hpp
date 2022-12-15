/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- LineRendition.hpp

Abstract:
- Enumerated type for the VT line rendition attribute. This determines the
  width and height scaling with which each line is rendered.

--*/

#pragma once

enum class LineRendition : uint8_t
{
    SingleWidth,
    DoubleWidth,
    DoubleHeightTop,
    DoubleHeightBottom
};

constexpr til::inclusive_rect ScreenToBufferLine(const til::inclusive_rect& line, const LineRendition lineRendition)
{
    // Use shift right to quickly divide the Left and Right by 2 for double width lines.
    const auto scale = lineRendition == LineRendition::SingleWidth ? 0 : 1;
    return { line.left >> scale, line.top, line.right >> scale, line.bottom };
}

constexpr til::point ScreenToBufferLine(const til::point& line, const LineRendition lineRendition)
{
    // Use shift right to quickly divide the Left and Right by 2 for double width lines.
    const auto scale = lineRendition == LineRendition::SingleWidth ? 0 : 1;
    return { line.x >> scale, line.y };
}

constexpr til::inclusive_rect BufferToScreenLine(const til::inclusive_rect& line, const LineRendition lineRendition)
{
    // Use shift left to quickly multiply the Left and Right by 2 for double width lines.
    const auto scale = lineRendition == LineRendition::SingleWidth ? 0 : 1;
    return { line.left << scale, line.top, (line.right << scale) + scale, line.bottom };
}
