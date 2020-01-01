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
// - attr - Font attributes field to adjust
// - isForeground - True if we're modifying the FOREGROUND colors. False if we're doing BACKGROUND.
// Return Value:
// - <none>
void AdaptDispatch::s_DisableAllColors(WORD& attr, const bool isForeground)
{
    if (isForeground)
    {
        WI_ClearAllFlags(attr, FG_ATTRS);
    }
    else
    {
        WI_ClearAllFlags(attr, BG_ATTRS);
    }
}

// Routine Description:
// - Small helper to help mask off the appropriate foreground/background bits in the colors bitfield.
// Arguments:
// - attr - Font attributes field to adjust
// - applyThis - Color values to apply to the low or high word of the font attributes field.
// - isForeground - TRUE = foreground color. FALSE = background color.
//   Specifies which half of the bit field to reset and then apply wApplyThis
//   upon.
// Return Value:
// - <none>
void AdaptDispatch::s_ApplyColors(WORD& attr, const WORD applyThis, const bool isForeground)
{
    // Copy the new attribute to apply
    WORD wNewColors = applyThis;

    // Mask off only the foreground or background
    if (isForeground)
    {
        WI_UpdateFlagsInMask(attr, FG_ATTRS, wNewColors);
    }
    else
    {
        WI_UpdateFlagsInMask(attr, BG_ATTRS, wNewColors);
    }
}

// Routine Description:
// - Helper to apply the actual flags to each text attributes field.
// - Placed as a helper so it can be recursive/re-entrant for some of the
//   convenience flag methods that perform similar/multiple operations in one
//   command.
// Arguments:
// - opt - Graphics option sent to us by the parser/requestor.
// - attr - Font attribute field to adjust
// Return Value:
// - <none>
void AdaptDispatch::_SetGraphicsOptionHelper(const DispatchTypes::GraphicsOptions opt, WORD& attr)
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
        WI_SetFlag(attr, COMMON_LVB_REVERSE_VIDEO);
        _changedMetaAttrs = true;
        break;
    case DispatchTypes::GraphicsOptions::Underline:
        // TODO:GH#2915 Treat underline separately from LVB_UNDERSCORE
        WI_SetFlag(attr, COMMON_LVB_UNDERSCORE);
        _changedMetaAttrs = true;
        break;
    case DispatchTypes::GraphicsOptions::Positive:
        WI_ClearFlag(attr, COMMON_LVB_REVERSE_VIDEO);
        _changedMetaAttrs = true;
        break;
    case DispatchTypes::GraphicsOptions::NoUnderline:
        WI_ClearFlag(attr, COMMON_LVB_UNDERSCORE);
        _changedMetaAttrs = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundBlack:
        s_DisableAllColors(attr, true); // turn off all color flags first.
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundBlue:
        s_DisableAllColors(attr, true); // turn off all color flags first.
        WI_SetFlag(attr, FOREGROUND_BLUE);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundGreen:
        s_DisableAllColors(attr, true); // turn off all color flags first.
        WI_SetFlag(attr, FOREGROUND_GREEN);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundCyan:
        s_DisableAllColors(attr, true); // turn off all color flags first.
        WI_SetAllFlags(attr, FOREGROUND_BLUE | FOREGROUND_GREEN);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundRed:
        s_DisableAllColors(attr, true); // turn off all color flags first.
        WI_SetFlag(attr, FOREGROUND_RED);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundMagenta:
        s_DisableAllColors(attr, true); // turn off all color flags first.
        WI_SetAllFlags(attr, FOREGROUND_BLUE | FOREGROUND_RED);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundYellow:
        s_DisableAllColors(attr, true); // turn off all color flags first.
        WI_SetAllFlags(attr, FOREGROUND_GREEN | FOREGROUND_RED);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundWhite:
        s_DisableAllColors(attr, true); // turn off all color flags first.
        WI_SetAllFlags(attr, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::ForegroundDefault:
        FAIL_FAST_MSG("GraphicsOptions::ForegroundDefault should be handled by _SetDefaultColorHelper");
        break;
    case DispatchTypes::GraphicsOptions::BackgroundBlack:
        s_DisableAllColors(attr, false); // turn off all color flags first.
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundBlue:
        s_DisableAllColors(attr, false); // turn off all color flags first.
        WI_SetFlag(attr, BACKGROUND_BLUE);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundGreen:
        s_DisableAllColors(attr, false); // turn off all color flags first.
        WI_SetFlag(attr, BACKGROUND_GREEN);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundCyan:
        s_DisableAllColors(attr, false); // turn off all color flags first.
        WI_SetAllFlags(attr, BACKGROUND_BLUE | BACKGROUND_GREEN);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundRed:
        s_DisableAllColors(attr, false); // turn off all color flags first.
        WI_SetFlag(attr, BACKGROUND_RED);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundMagenta:
        s_DisableAllColors(attr, false); // turn off all color flags first.
        WI_SetAllFlags(attr, BACKGROUND_BLUE | BACKGROUND_RED);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundYellow:
        s_DisableAllColors(attr, false); // turn off all color flags first.
        WI_SetAllFlags(attr, BACKGROUND_GREEN | BACKGROUND_RED);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundWhite:
        s_DisableAllColors(attr, false); // turn off all color flags first.
        WI_SetAllFlags(attr, BACKGROUND_BLUE | BACKGROUND_GREEN | BACKGROUND_RED);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BackgroundDefault:
        FAIL_FAST_MSG("GraphicsOptions::BackgroundDefault should be handled by _SetDefaultColorHelper");
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundBlack:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundBlack, attr);
        WI_SetFlag(attr, FOREGROUND_INTENSITY);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundBlue:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundBlue, attr);
        WI_SetFlag(attr, FOREGROUND_INTENSITY);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundGreen:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundGreen, attr);
        WI_SetFlag(attr, FOREGROUND_INTENSITY);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundCyan:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundCyan, attr);
        WI_SetFlag(attr, FOREGROUND_INTENSITY);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundRed:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundRed, attr);
        WI_SetFlag(attr, FOREGROUND_INTENSITY);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundMagenta:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundMagenta, attr);
        WI_SetFlag(attr, FOREGROUND_INTENSITY);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundYellow:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundYellow, attr);
        WI_SetFlag(attr, FOREGROUND_INTENSITY);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundWhite:
        _SetGraphicsOptionHelper(GraphicsOptions::ForegroundWhite, attr);
        WI_SetFlag(attr, FOREGROUND_INTENSITY);
        _changedForeground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundBlack:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundBlack, attr);
        WI_SetFlag(attr, BACKGROUND_INTENSITY);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundBlue:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundBlue, attr);
        WI_SetFlag(attr, BACKGROUND_INTENSITY);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundGreen:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundGreen, attr);
        WI_SetFlag(attr, BACKGROUND_INTENSITY);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundCyan:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundCyan, attr);
        WI_SetFlag(attr, BACKGROUND_INTENSITY);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundRed:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundRed, attr);
        WI_SetFlag(attr, BACKGROUND_INTENSITY);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundMagenta:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundMagenta, attr);
        WI_SetFlag(attr, BACKGROUND_INTENSITY);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundYellow:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundYellow, attr);
        WI_SetFlag(attr, BACKGROUND_INTENSITY);
        _changedBackground = true;
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundWhite:
        _SetGraphicsOptionHelper(GraphicsOptions::BackgroundWhite, attr);
        WI_SetFlag(attr, BACKGROUND_INTENSITY);
        _changedBackground = true;
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
// - options - An array of options that will be used to generate the RGB color
// - rgbColor - Location to place the generated RGB color into.
// - isForeground - Location to place whether or not the parsed color is for the foreground or not.
// - optionsConsumed - Location to place the number of options we consumed parsing this option.
// Return Value:
// Returns true if we successfully parsed an extended color option from the options array.
// - This corresponds to the following number of options consumed (pcOptionsConsumed):
//     1 - false, not enough options to parse.
//     2 - false, not enough options to parse.
//     3 - true, parsed an xterm index to a color
//     5 - true, parsed an RGB color.
bool AdaptDispatch::_SetRgbColorsHelper(const std::basic_string_view<DispatchTypes::GraphicsOptions> options,
                                        COLORREF& rgbColor,
                                        bool& isForeground,
                                        size_t& optionsConsumed)
{
    bool success = false;
    optionsConsumed = 1;
    if (options.size() >= 2 && s_IsRgbColorOption(options.at(0)))
    {
        optionsConsumed = 2;
        DispatchTypes::GraphicsOptions extendedOpt = options.at(0);
        DispatchTypes::GraphicsOptions typeOpt = options.at(1);

        if (extendedOpt == DispatchTypes::GraphicsOptions::ForegroundExtended)
        {
            isForeground = true;
        }
        else if (extendedOpt == DispatchTypes::GraphicsOptions::BackgroundExtended)
        {
            isForeground = false;
        }

        if (typeOpt == DispatchTypes::GraphicsOptions::RGBColorOrFaint && options.size() >= 5)
        {
            optionsConsumed = 5;
            // ensure that each value fits in a byte
            unsigned int red = std::min(static_cast<unsigned int>(options.at(2)), 255u);
            unsigned int green = std::min(static_cast<unsigned int>(options.at(3)), 255u);
            unsigned int blue = std::min(static_cast<unsigned int>(options.at(4)), 255u);

            rgbColor = RGB(red, green, blue);

            success = _pConApi->SetConsoleRGBTextAttribute(rgbColor, isForeground);
        }
        else if (typeOpt == DispatchTypes::GraphicsOptions::BlinkOrXterm256Index && options.size() >= 3)
        {
            optionsConsumed = 3;
            if (options.at(2) <= 255) // ensure that the provided index is on the table
            {
                unsigned int tableIndex = options.at(2);

                success = _pConApi->SetConsoleXtermTextAttribute(tableIndex, isForeground);
            }
        }
    }
    return success;
}

bool AdaptDispatch::_SetBoldColorHelper(const DispatchTypes::GraphicsOptions option)
{
    const bool bold = (option == DispatchTypes::GraphicsOptions::BoldBright);
    return _pConApi->PrivateBoldText(bold);
}

bool AdaptDispatch::_SetDefaultColorHelper(const DispatchTypes::GraphicsOptions option)
{
    const bool fg = option == GraphicsOptions::Off || option == GraphicsOptions::ForegroundDefault;
    const bool bg = option == GraphicsOptions::Off || option == GraphicsOptions::BackgroundDefault;

    bool success = _pConApi->PrivateSetDefaultAttributes(fg, bg);

    if (success && fg && bg)
    {
        // If we're resetting both the FG & BG, also reset the meta attributes (underline)
        //      as well as the boldness
        success = _pConApi->PrivateSetLegacyAttributes(0, false, false, true) &&
                  _pConApi->PrivateBoldText(false) &&
                  _pConApi->PrivateSetExtendedTextAttributes(ExtendedAttributes::Normal);
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

    RETURN_BOOL_IF_FALSE(_pConApi->PrivateGetExtendedTextAttributes(attrs));

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

    return _pConApi->PrivateSetExtendedTextAttributes(attrs);
}

// Routine Description:
// - SGR - Modifies the graphical rendering options applied to the next
//   characters written into the buffer.
//       - Options include colors, invert, underlines, and other "font style"
//         type options.

// Arguments:
// - options - An array of options that will be applied from 0 to N, in order,
//   one at a time by setting or removing flags in the font style properties.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::SetGraphicsRendition(const std::basic_string_view<DispatchTypes::GraphicsOptions> options)
{
    // We use the private function here to get just the default color attributes
    // as a performance optimization. Calling the public
    // GetConsoleScreenBufferInfoEx costs a lot of performance time/power in a
    // tight loop because it has to fill the Largest Window Size by asking the
    // OS and wastes time memcpying colors and other data we do not need to
    // resolve this Set Graphics Rendition request.
    WORD attr;
    bool success = _pConApi->PrivateGetConsoleScreenBufferAttributes(attr);

    if (success)
    {
        // Run through the graphics options and apply them
        for (size_t i = 0; i < options.size(); i++)
        {
            DispatchTypes::GraphicsOptions opt = options.at(i);
            if (s_IsDefaultColorOption(opt))
            {
                success = _SetDefaultColorHelper(opt);
            }
            else if (s_IsBoldColorOption(opt))
            {
                success = _SetBoldColorHelper(opt);
            }
            else if (s_IsExtendedTextAttribute(opt))
            {
                success = _SetExtendedTextAttributeHelper(opt);
            }
            else if (s_IsRgbColorOption(opt))
            {
                COLORREF rgbColor;
                bool isForeground = true;

                size_t optionsConsumed = 0;

                // _SetRgbColorsHelper will call the appropriate ConApi function
                success = _SetRgbColorsHelper(options.substr(i),
                                              rgbColor,
                                              isForeground,
                                              optionsConsumed);

                i += (optionsConsumed - 1); // cOptionsConsumed includes the opt we're currently on.
            }
            else
            {
                _SetGraphicsOptionHelper(opt, attr);
                success = _pConApi->PrivateSetLegacyAttributes(attr,
                                                               _changedForeground,
                                                               _changedBackground,
                                                               _changedMetaAttrs);

                // Make sure we un-bold
                if (success && opt == DispatchTypes::GraphicsOptions::Off)
                {
                    success = _SetBoldColorHelper(opt);
                }

                _changedForeground = false;
                _changedBackground = false;
                _changedMetaAttrs = false;
            }
        }
    }

    return success;
}
