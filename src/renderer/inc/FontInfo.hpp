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

class FontInfo : public FontInfoBase
{
public:
    FontInfo(_In_ PCWSTR const pwszFaceName,
             const BYTE bFamily,
             const LONG lWeight,
             const COORD coordSize,
             const UINT uiCodePage,
             const bool fSetDefaultRasterFont = false);

    FontInfo(const FontInfo& fiFont);

    COORD GetSize() const;
    COORD GetUnscaledSize() const;

    void SetFromEngine(_In_ PCWSTR const pwszFaceName,
                       const BYTE bFamily,
                       const LONG lWeight,
                       const bool fSetDefaultRasterFont,
                       const COORD coordSize,
                       const COORD coordSizeUnscaled);

    void ValidateFont();

    static void s_SetFontDefaultList(_In_ Microsoft::Console::Render::IFontDefaultList* const pFontDefaultList);

    friend bool operator==(const FontInfo& a, const FontInfo& b);

private:
    void _ValidateCoordSize();

    COORD _coordSize;
    COORD _coordSizeUnscaled;
};

bool operator==(const FontInfo& a, const FontInfo& b);

// SET AND UNSET CONSOLE_OEMFONT_DISPLAY unless we can get rid of the stupid recoding in the conhost side.
