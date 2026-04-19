/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FontInfo.hpp

Abstract:
- This serves as the structure defining font information.  There are three
  relevant classes defined.

- FontInfo - derived from FontInfoBase.  It also has font size
  information - both the width and height of the requested font, as
  well as the measured height and width of L'0' from GDI.  All
  coordinates { X, Y } pair are non zero and always set to some
  reasonable value, even when GDI APIs fail.  This helps avoid
  divide by zero issues while performing various sizing
  calculations.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "FontInfoBase.hpp"

struct CellSizeInDIP
{
    float width = 0;
    float height = 0;

    til::size AsInteger_DoNotUse() const noexcept;
};

struct FontInfo : FontInfoBase
{
    float GetFontSizeInDIP() const noexcept;
    CellSizeInDIP GetCellSizeInDIP() const noexcept;
    til::size GetCellSizeInPhysicalPx() const noexcept;

    // NOTE: Render backends are expected to call all of these setters, plus all FontInfoBase setters.
    void SetFontSizeInPt(float fontSizeInPt) noexcept;
    void SetCellSizeInDIP(CellSizeInDIP cellSizeInDIP) noexcept;
    void SetCellSizeInPhysicalPx(til::size cellSizeInPhysicalPx) noexcept;

    bool IsTrueTypeFont() const noexcept;

private:
    float _fontSizeInPt = 0;
    CellSizeInDIP _cellSizeInDIP;
    til::size _cellSizeInPhysicalPx;
};
