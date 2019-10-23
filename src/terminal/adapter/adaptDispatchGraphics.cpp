// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include <precomp.h>

#include "adaptDispatch.hpp"
#include "conGetSet.hpp"
#include "../../types/inc/utils.hpp"

#define ENABLE_INTSAFE_SIGNED_FUNCTIONS
#include <intsafe.h>

using namespace Microsoft::Console::VirtualTerminal;
using namespace Microsoft::Console::VirtualTerminal::DispatchTypes;

// Routine Description:
// - Small helper to disable all color flags within a given font attributes field
// Arguments:
// - pAttr - Pointer to font attributes field to adjust
// - fIsForeground - True if we're modifying the FOREGROUND colors. False if we're doing BACKGROUND.
// Return Value:
// - <none>
void AdaptDispatch::s_DisableAllColors(_Inout_ WORD* const pAttr, const bool fIsForeground)
{
    if (fIsForeground)
    {
        *pAttr &= ~(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
    }
    else
    {
        *pAttr &= ~(BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY);
    }
}

// Routine Description:
// - Small helper to help mask off the appropriate foreground/background bits in the colors bitfield.
// Arguments:
// - pAttr - Pointer to font attributes field to adjust
// - wApplyThis - Color values to apply to the low or high word of the font attributes field.
// - fIsForeground - TRUE = foreground color. FALSE = background color.
//   Specifies which half of the bit field to reset and then apply wApplyThis
//   upon.
// Return Value:
// - <none>
void AdaptDispatch::s_ApplyColors(_Inout_ WORD* const pAttr, const WORD wApplyThis, const bool fIsForeground)
{
    // Copy the new attribute to apply
    WORD wNewColors = wApplyThis;

    // Mask off only the foreground or background
    if (fIsForeground)
    {
        *pAttr &= ~(FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
        wNewColors &= (FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED | FOREGROUND_INTENSITY);
    }
    else
    {
        *pAttr &= ~(BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY);
        wNewColors &= (BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED | BACKGROUND_INTENSITY);
    }

    // Apply appropriate flags.
    *pAttr |= wNewColors;
}

// Routine Description:
// - Helper to apply the actual flags to each text attributes field.
// - Placed as a helper so it can be recursive/re-entrant for some of the
//   convenience flag methods that perform similar/multiple operations in one
//   command.
// Arguments:
// - opt - Graphics option sent to us by the parser/requestor.
// - pAttr - Pointer to the font attribute field to adjust
// Return Value:
// - <none>
void AdaptDispatch::_SetGraphicsOptionHelper(const DispatchTypes::GraphicsOptions opt, _Inout_ WORD* const pAttr)
{
    switch (opt)
    {
    case DispatchTypes::GraphicsOptions::Off:
        FAIL_FAST_MSG("GraphicsOptions::Off should be handled by _SetDefaultColorHelper");
        break;
    // MSFT:16398982 - These two are now handled by _SetBoldColorHelper
    // case DispatchTypes::GraphicsOptions::BoldBright:
    // case DispatchTypes::GraphicsOptions::UnBold:
    case DispatchTypes::GraphicsOptions::Negative:
        *pAttr |= COMMON_LVB_REVERSE_VIDEO;
        _fChangedMetaAttrs = true;
        break;
    case DispatchTypes::GraphicsOptions::Underline:
        // TODO:GH#2915 Treat underline separately from LVB_UNDERSCORE
        *pAttr |= COMMON_LVB_UNDERSCORE;
        _fChangedMetaAttrs = true;
        break;
    case DispatchTypes::GraphicsOptions::Positive:
        *pAttr &= ~COMMON_LVB_REVERSE_VIDEO;
        _fChangedMetaAttrs = true;
        break;
    case DispatchTypes::GraphicsOptions::NoUnderline:
        *pAttr &= ~COMMON_LVB_UNDERSCORE;
        _fChangedMetaAttrs = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundBlack:
        s_DisableAllColors(pAttr, true); // turn off all color flags first.
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundBlue:
        s_DisableAllColors(pAttr, true); // turn off all color flags first.
        *pAttr |= FOREGROUND_BLUE;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundGreen:
        s_DisableAllColors(pAttr, true); // turn off all color flags first.
        *pAttr |= FOREGROUND_GREEN;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundCyan:
        s_DisableAllColors(pAttr, true); // turn off all color flags first.
        *pAttr |= FOREGROUND_BLUE | FOREGROUND_GREEN;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundRed:
        s_DisableAllColors(pAttr, true); // turn off all color flags first.
        *pAttr |= FOREGROUND_RED;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundMagenta:
        s_DisableAllColors(pAttr, true); // turn off all color flags first.
        *pAttr |= FOREGROUND_BLUE | FOREGROUND_RED;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundYellow:
        s_DisableAllColors(pAttr, true); // turn off all color flags first.
        *pAttr |= FOREGROUND_GREEN | FOREGROUND_RED;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundWhite:
        s_DisableAllColors(pAttr, true); // turn off all color flags first.
        *pAttr |= FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundDefault:
        FAIL_FAST_MSG("GraphicsOptions::ForegroundDefault should be handled by _SetDefaultColorHelper");
        break;
    case DispatchTypes::GraphicsOptions::BackgroundBlack:
        s_DisableAllColors(pAttr, false); // turn off all color flags first.
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundBlue:
        s_DisableAllColors(pAttr, false); // turn off all color flags first.
        *pAttr |= BACKGROUND_BLUE;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundGreen:
        s_DisableAllColors(pAttr, false); // turn off all color flags first.
        *pAttr |= BACKGROUND_GREEN;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundCyan:
        s_DisableAllColors(pAttr, false); // turn off all color flags first.
        *pAttr |= BACKGROUND_BLUE | BACKGROUND_GREEN;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundRed:
        s_DisableAllColors(pAttr, false); // turn off all color flags first.
        *pAttr |= BACKGROUND_RED;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundMagenta:
        s_DisableAllColors(pAttr, false); // turn off all color flags first.
        *pAttr |= BACKGROUND_BLUE | BACKGROUND_RED;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundYellow:
        s_DisableAllColors(pAttr, false); // turn off all color flags first.
        *pAttr |= BACKGROUND_GREEN | BACKGROUND_RED;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundWhite:
        s_DisableAllColors(pAttr, false); // turn off all color flags first.
        *pAttr |= BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundDefault:
        FAIL_FAST_MSG("GraphicsOptions::BackgroundDefault should be handled by _SetDefaultColorHelper");
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundBlack:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundBlack, pAttr);
        *pAttr |= FOREGROUND_INTENSITY;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundBlue:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundBlue, pAttr);
        *pAttr |= FOREGROUND_INTENSITY;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundGreen:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundGreen, pAttr);
        *pAttr |= FOREGROUND_INTENSITY;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundCyan:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundCyan, pAttr);
        *pAttr |= FOREGROUND_INTENSITY;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundRed:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundRed, pAttr);
        *pAttr |= FOREGROUND_INTENSITY;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundMagenta:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundMagenta, pAttr);
        *pAttr |= FOREGROUND_INTENSITY;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundYellow:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundYellow, pAttr);
        *pAttr |= FOREGROUND_INTENSITY;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundWhite:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundWhite, pAttr);
        *pAttr |= FOREGROUND_INTENSITY;
        _fChangedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundBlack:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundBlack, pAttr);
        *pAttr |= BACKGROUND_INTENSITY;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundBlue:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundBlue, pAttr);
        *pAttr |= BACKGROUND_INTENSITY;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundGreen:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundGreen, pAttr);
        *pAttr |= BACKGROUND_INTENSITY;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundCyan:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundCyan, pAttr);
        *pAttr |= BACKGROUND_INTENSITY;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundRed:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundRed, pAttr);
        *pAttr |= BACKGROUND_INTENSITY;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundMagenta:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundMagenta, pAttr);
        *pAttr |= BACKGROUND_INTENSITY;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundYellow:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundYellow, pAttr);
        *pAttr |= BACKGROUND_INTENSITY;
        _fChangedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundWhite:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundWhite, pAttr);
        *pAttr |= BACKGROUND_INTENSITY;
        _fChangedBackground = true;
        break;
    }
}

// Routine Description:
// Returns true if the GraphicsOption represents an extended text attribute.
//   These include things such as Underlined, Italics, Blinking, etc.
// Return Value:
// - true if the opt is the indicator for an extended text attribute, false otherwise.
bool AdaptDispatch::s_IsExtendedTextAttribute(const DispatchTypes::GraphicsOptions opt) noexcept
{
    // TODO:GH#2916 add support for DoublyUnderlined, Faint(RGBColorOrFaint).
    // These two are currently partially implemented as other things:
    // * Faint is approximately the opposite of bold does, though it's much
    //   [more complicated](
    //   https://github.com/microsoft/terminal/issues/2916#issuecomment-535860910)
    //   and less supported/used.
    // * Doubly underlined should exist in a trinary state with Underlined
    return opt == DispatchTypes::GraphicsOptions::Italics ||
           opt == DispatchTypes::GraphicsOptions::NotItalics ||
           opt == DispatchTypes::GraphicsOptions::BlinkOrXterm256Index ||
           opt == DispatchTypes::GraphicsOptions::Steady ||
           opt == DispatchTypes::GraphicsOptions::Invisible ||
           opt == DispatchTypes::GraphicsOptions::Visible ||
           opt == DispatchTypes::GraphicsOptions::CrossedOut ||
           opt == DispatchTypes::GraphicsOptions::NotCrossedOut;
}

// Routine Description:
// Returns true if the GraphicsOption represents an extended color option.
//   These are followed by up to 4 more values which compose the entire option.
// Return Value:
// - true if the opt is the indicator for an extended color sequence, false otherwise.
bool AdaptDispatch::s_IsRgbColorOption(const DispatchTypes::GraphicsOptions opt)
{
    return opt == DispatchTypes::GraphicsOptions::ForegroundExtended ||
           opt == DispatchTypes::GraphicsOptions::BackgroundExtended;
}

// Routine Description:
// Returns true if the GraphicsOption represents an extended color option.
//   These are followed by up to 4 more values which compose the entire option.
// Return Value:
// - true if the opt is the indicator for an extended color sequence, false otherwise.
bool AdaptDispatch::s_IsBoldColorOption(const DispatchTypes::GraphicsOptions opt) noexcept
{
    return opt == DispatchTypes::GraphicsOptions::BoldBright ||
           opt == DispatchTypes::GraphicsOptions::UnBold;
}

// Function Description:
// - checks if this graphics option should set either the console's FG or BG to
//the default attributes.
// Return Value:
// - true if the opt sets either/or attribute to the defaults, false otherwise.
bool AdaptDispatch::s_IsDefaultColorOption(const DispatchTypes::GraphicsOptions opt) noexcept
{
    return opt == DispatchTypes::GraphicsOptions::Off ||
           opt == DispatchTypes::GraphicsOptions::ForegroundDefault ||
           opt == DispatchTypes::GraphicsOptions::BackgroundDefault;
}

// Routine Description:
// - Helper to parse extended graphics options, which start with 38 (FG) or 48 (BG)
//     These options are followed by either a 2 (RGB) or 5 (xterm index)
//      RGB sequences then take 3 MORE params to designate the R, G, B parts of the color
//      Xterm index will use the param that follows to use a color from the preset 256 color xterm color table.
// Arguments:
// - rgOptions - An array of options that will be used to generate the RGB color
// - cOptions - The count of options
// - prgbColor - A pointer to place the generated RGB color into.
// - pfIsForeground - a pointer to place whether or not the parsed color is for the foreground or not.
// - pcOptionsConsumed - a pointer to place the number of options we consumed parsing this option.
// - ColorTable - the windows color table, for xterm indices < 16
// - cColorTable - The number of elements in the windows color table.
// Return Value:
// Returns true if we successfully parsed an extended color option from the options array.
// - This corresponds to the following number of options consumed (pcOptionsConsumed):
//     1 - false, not enough options to parse.
//     2 - false, not enough options to parse.
//     3 - true, parsed an xterm index to a color
//     5 - true, parsed an RGB color.
bool AdaptDispatch::_SetRgbColorsHelper(_In_reads_(cOptions) const DispatchTypes::GraphicsOptions* const rgOptions,
                                        const size_t cOptions,
                                        _Out_ COLORREF* const prgbColor,
                                        _Out_ bool* const pfIsForeground,
                                        _Out_ size_t* const pcOptionsConsumed)
{
    bool fSuccess = false;
    *pcOptionsConsumed = 1;
    if (cOptions >= 2 && s_IsRgbColorOption(rgOptions[0]))
    {
        *pcOptionsConsumed = 2;
        DispatchTypes::GraphicsOptions extendedOpt = rgOptions[0];
        DispatchTypes::GraphicsOptions typeOpt = rgOptions[1];

        if (extendedOpt == DispatchTypes::GraphicsOptions::ForegroundExtended)
        {
            *pfIsForeground = true;
        }
        else if (extendedOpt == DispatchTypes::GraphicsOptions::BackgroundExtended)
        {
            *pfIsForeground = false;
        }

        if (typeOpt == DispatchTypes::GraphicsOptions::RGBColorOrFaint && cOptions >= 5)
        {
            *pcOptionsConsumed = 5;
            // ensure that each value fits in a byte
            unsigned int red = rgOptions[2] > 255 ? 255 : rgOptions[2];
            unsigned int green = rgOptions[3] > 255 ? 255 : rgOptions[3];
            unsigned int blue = rgOptions[4] > 255 ? 255 : rgOptions[4];

            *prgbColor = RGB(red, green, blue);

            fSuccess = !!_conApi->SetConsoleRGBTextAttribute(*prgbColor, *pfIsForeground);
        }
        else if (typeOpt == DispatchTypes::GraphicsOptions::BlinkOrXterm256Index && cOptions >= 3)
        {
            *pcOptionsConsumed = 3;
            if (rgOptions[2] <= 255) // ensure that the provided index is on the table
            {
                unsigned int tableIndex = rgOptions[2];

                fSuccess = !!_conApi->SetConsoleXtermTextAttribute(tableIndex, *pfIsForeground);
            }
        }
    }
    return fSuccess;
}

bool AdaptDispatch::_SetBoldColorHelper(const DispatchTypes::GraphicsOptions option)
{
    const bool bold = (option == DispatchTypes::GraphicsOptions::BoldBright);
    return !!_conApi->PrivateBoldText(bold);
}

bool AdaptDispatch::_SetDefaultColorHelper(const DispatchTypes::GraphicsOptions option)
{
    const bool fg = option == GraphicsOptions::Off || option == GraphicsOptions::ForegroundDefault;
    const bool bg = option == GraphicsOptions::Off || option == GraphicsOptions::BackgroundDefault;

    bool success = _conApi->PrivateSetDefaultAttributes(fg, bg);

    if (success && fg && bg)
    {
        // If we're resetting both the FG & BG, also reset the meta attributes (underline)
        //      as well as the boldness
        success = _conApi->PrivateSetLegacyAttributes(0, false, false, true) &&
                  _conApi->PrivateBoldText(false) &&
                  _conApi->PrivateSetExtendedTextAttributes(ExtendedAttributes::Normal);
    }
    return success;
}

// Method Description:
// - Sets the attributes for extended text attributes. Retrieves the current
//   extended attrs from the console, modifies them according to the new
//   GraphicsOption, and the sets them again.
// - Notably does _not_ handle Bold, Faint, Underline, DoublyUnderlined, or
//   NoUnderline. Those should be handled in TODO:GH#2916.
// Arguments:
// - opt: the graphics option to set
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::_SetExtendedTextAttributeHelper(const DispatchTypes::GraphicsOptions opt)
{
    ExtendedAttributes attrs{ ExtendedAttributes::Normal };

    RETURN_BOOL_IF_FALSE(_conApi->PrivateGetExtendedTextAttributes(&attrs));

    switch (opt)
    {
    case DispatchTypes::GraphicsOptions::Italics:
        WI_SetFlag(attrs, ExtendedAttributes::Italics);
        break;
    case DispatchTypes::GraphicsOptions::NotItalics:
        WI_ClearFlag(attrs, ExtendedAttributes::Italics);
        break;
    case DispatchTypes::GraphicsOptions::BlinkOrXterm256Index:
        WI_SetFlag(attrs, ExtendedAttributes::Blinking);
        break;
    case DispatchTypes::GraphicsOptions::Steady:
        WI_ClearFlag(attrs, ExtendedAttributes::Blinking);
        break;
    case DispatchTypes::GraphicsOptions::Invisible:
        WI_SetFlag(attrs, ExtendedAttributes::Invisible);
        break;
    case DispatchTypes::GraphicsOptions::Visible:
        WI_ClearFlag(attrs, ExtendedAttributes::Invisible);
        break;
    case DispatchTypes::GraphicsOptions::CrossedOut:
        WI_SetFlag(attrs, ExtendedAttributes::CrossedOut);
        break;
    case DispatchTypes::GraphicsOptions::NotCrossedOut:
        WI_ClearFlag(attrs, ExtendedAttributes::CrossedOut);
        break;
        // TODO:GH#2916 add support for the following
        // case DispatchTypes::GraphicsOptions::DoublyUnderlined:
        // case DispatchTypes::GraphicsOptions::RGBColorOrFaint:
        // case DispatchTypes::GraphicsOptions::DoublyUnderlined:
    }

    return _conApi->PrivateSetExtendedTextAttributes(attrs);
}

// Routine Description:
// - SGR - Modifies the graphical rendering options applied to the next
//   characters written into the buffer.
//       - Options include colors, invert, underlines, and other "font style"
//         type options.

// Arguments:
// - rgOptions - An array of options that will be applied from 0 to N, in order,
//   one at a time by setting or removing flags in the font style properties.
// - cOptions - The count of options (a.k.a. the N in the above line of comments)
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetGraphicsRendition(_In_reads_(cOptions) const DispatchTypes::GraphicsOptions* const rgOptions,
                                         const size_t cOptions)
{
    // We use the private function here to get just the default color attributes
    // as a performance optimization. Calling the public
    // GetConsoleScreenBufferInfoEx costs a lot of performance time/power in a
    // tight loop because it has to fill the Largest Window Size by asking the
    // OS and wastes time memcpying colors and other data we do not need to
    // resolve this Set Graphics Rendition request.
    WORD attr;
    bool fSuccess = !!_conApi->PrivateGetConsoleScreenBufferAttributes(&attr);

    if (fSuccess)
    {
        // Run through the graphics options and apply them
        for (size_t i = 0; i < cOptions; i++)
        {
            DispatchTypes::GraphicsOptions opt = rgOptions[i];
            if (s_IsDefaultColorOption(opt))
            {
                fSuccess = _SetDefaultColorHelper(opt);
            }
            else if (s_IsBoldColorOption(opt))
            {
                fSuccess = _SetBoldColorHelper(rgOptions[i]);
            }
            else if (s_IsExtendedTextAttribute(opt))
            {
                fSuccess = _SetExtendedTextAttributeHelper(rgOptions[i]);
            }
            else if (s_IsRgbColorOption(opt))
            {
                COLORREF rgbColor;
                bool fIsForeground = true;

                size_t cOptionsConsumed = 0;

                // _SetRgbColorsHelper will call the appropriate ConApi function
                fSuccess = _SetRgbColorsHelper(&(rgOptions[i]),
                                               cOptions - i,
                                               &rgbColor,
                                               &fIsForeground,
                                               &cOptionsConsumed);

                i += (cOptionsConsumed - 1); // cOptionsConsumed includes the opt we're currently on.
            }
            else
            {
                _SetGraphicsOptionHelper(opt, &attr);
                fSuccess = !!_conApi->PrivateSetLegacyAttributes(attr,
                                                                 _fChangedForeground,
                                                                 _fChangedBackground,
                                                                 _fChangedMetaAttrs);

                // Make sure we un-bold
                if (fSuccess && opt == DispatchTypes::GraphicsOptions::Off)
                {
                    fSuccess = _SetBoldColorHelper(opt);
                }

                _fChangedForeground = false;
                _fChangedBackground = false;
                _fChangedMetaAttrs = false;
            }
        }
    }

    return fSuccess;
}
