/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FontInfo.hpp

Abstract:
- This serves as the structure defining font information.

- FontInfoDesired - derived from FontInfoBase.  It also contains
  a desired size { X, Y }, to be supplied to the GDI's LOGFONT
  structure.  Unlike FontInfo, both desired X and Y can be zero.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "FontInfoBase.hpp"
#include "FontInfo.hpp"

class FontInfoDesired : public FontInfoBase
{
public:
    FontInfoDesired(const std::wstring_view faceName,
                    const unsigned char family,
                    const unsigned int weight,
                    const COORD coordSizeDesired,
                    const unsigned int uiCodePage);

    FontInfoDesired(const FontInfo& fiFont);

    COORD GetEngineSize() const;
    bool IsDefaultRasterFont() const;

    friend bool operator==(const FontInfoDesired& a, const FontInfoDesired& b);

private:
    COORD _coordSizeDesired;
};

bool operator==(const FontInfoDesired& a, const FontInfoDesired& b);
