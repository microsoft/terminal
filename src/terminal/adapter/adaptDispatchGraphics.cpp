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

// clang-format off
const BYTE BLUE_ATTR      = 0x01;
const BYTE GREEN_ATTR     = 0x02;
const BYTE RED_ATTR       = 0x04;
const BYTE BRIGHT_ATTR    = 0x08;
const BYTE DARK_BLACK     = 0;
const BYTE DARK_RED       = RED_ATTR;
const BYTE DARK_GREEN     = GREEN_ATTR;
const BYTE DARK_YELLOW    = RED_ATTR | GREEN_ATTR;
const BYTE DARK_BLUE      = BLUE_ATTR;
const BYTE DARK_MAGENTA   = RED_ATTR | BLUE_ATTR;
const BYTE DARK_CYAN      = GREEN_ATTR | BLUE_ATTR;
const BYTE DARK_WHITE     = RED_ATTR | GREEN_ATTR | BLUE_ATTR;
const BYTE BRIGHT_BLACK   = BRIGHT_ATTR;
const BYTE BRIGHT_RED     = BRIGHT_ATTR | RED_ATTR;
const BYTE BRIGHT_GREEN   = BRIGHT_ATTR | GREEN_ATTR;
const BYTE BRIGHT_YELLOW  = BRIGHT_ATTR | RED_ATTR | GREEN_ATTR;
const BYTE BRIGHT_BLUE    = BRIGHT_ATTR | BLUE_ATTR;
const BYTE BRIGHT_MAGENTA = BRIGHT_ATTR | RED_ATTR | BLUE_ATTR;
const BYTE BRIGHT_CYAN    = BRIGHT_ATTR | GREEN_ATTR | BLUE_ATTR;
const BYTE BRIGHT_WHITE   = BRIGHT_ATTR | RED_ATTR | GREEN_ATTR | BLUE_ATTR;
// clang-format on

// Routine Description:
// - Helper to parse extended graphics options, which start with 38 (FG) or 48 (BG)
//     These options are followed by either a 2 (RGB) or 5 (xterm index)
//      RGB sequences then take 3 MORE params to designate the R, G, B parts of the color
//      Xterm index will use the param that follows to use a color from the preset 256 color xterm color table.
// Arguments:
// - options - An array of options that will be used to generate the RGB color
// - attr - The attribute that will be updated with the parsed color.
// - isForeground - Whether or not the parsed color is for the foreground.
// Return Value:
// - The number of options consumed, not including the initial 38/48.
size_t AdaptDispatch::_SetRgbColorsHelper(const std::basic_string_view<DispatchTypes::GraphicsOptions> options,
                                          TextAttribute& attr,
                                          const bool isForeground) noexcept
{
    size_t optionsConsumed = 0;
    if (options.size() >= 1)
    {
        optionsConsumed = 1;
        const auto typeOpt = til::at(options, 0);
        if (typeOpt == DispatchTypes::GraphicsOptions::RGBColorOrFaint && options.size() >= 4)
        {
            optionsConsumed = 4;
            const size_t red = til::at(options, 1);
            const size_t green = til::at(options, 2);
            const size_t blue = til::at(options, 3);
            // ensure that each value fits in a byte
            if (red <= 255 && green <= 255 && blue <= 255)
            {
                const COLORREF rgbColor = RGB(red, green, blue);
                attr.SetColor(rgbColor, isForeground);
            }
        }
        else if (typeOpt == DispatchTypes::GraphicsOptions::BlinkOrXterm256Index && options.size() >= 2)
        {
            optionsConsumed = 2;
            const size_t tableIndex = til::at(options, 1);
            if (tableIndex <= 255)
            {
                const auto adjustedIndex = gsl::narrow_cast<BYTE>(::Xterm256ToWindowsIndex(tableIndex));
                if (isForeground)
                {
                    attr.SetIndexedForeground256(adjustedIndex);
                }
                else
                {
                    attr.SetIndexedBackground256(adjustedIndex);
                }
            }
        }
    }
    return optionsConsumed;
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
    TextAttribute attr;
    bool success = _pConApi->PrivateGetTextAttributes(attr);

    if (success)
    {
        // Run through the graphics options and apply them
        for (size_t i = 0; i < options.size(); i++)
        {
            const auto opt = til::at(options, i);
            switch (opt)
            {
            case Off:
                attr.SetDefaultForeground();
                attr.SetDefaultBackground();
                attr.SetStandardErase();
                break;
            case ForegroundDefault:
                attr.SetDefaultForeground();
                break;
            case BackgroundDefault:
                attr.SetDefaultBackground();
                break;
            case BoldBright:
                attr.SetBold(true);
                break;
            case UnBold:
                attr.SetBold(false);
                break;
            case Italics:
                attr.SetItalics(true);
                break;
            case NotItalics:
                attr.SetItalics(false);
                break;
            case BlinkOrXterm256Index:
                attr.SetBlinking(true);
                break;
            case Steady:
                attr.SetBlinking(false);
                break;
            case Invisible:
                attr.SetInvisible(true);
                break;
            case Visible:
                attr.SetInvisible(false);
                break;
            case CrossedOut:
                attr.SetCrossedOut(true);
                break;
            case NotCrossedOut:
                attr.SetCrossedOut(false);
                break;
            case Negative:
                attr.SetReverseVideo(true);
                break;
            case Positive:
                attr.SetReverseVideo(false);
                break;
            case Underline:
                attr.SetUnderline(true);
                break;
            case NoUnderline:
                attr.SetUnderline(false);
                break;
            case ForegroundBlack:
                attr.SetIndexedForeground(DARK_BLACK);
                break;
            case ForegroundBlue:
                attr.SetIndexedForeground(DARK_BLUE);
                break;
            case ForegroundGreen:
                attr.SetIndexedForeground(DARK_GREEN);
                break;
            case ForegroundCyan:
                attr.SetIndexedForeground(DARK_CYAN);
                break;
            case ForegroundRed:
                attr.SetIndexedForeground(DARK_RED);
                break;
            case ForegroundMagenta:
                attr.SetIndexedForeground(DARK_MAGENTA);
                break;
            case ForegroundYellow:
                attr.SetIndexedForeground(DARK_YELLOW);
                break;
            case ForegroundWhite:
                attr.SetIndexedForeground(DARK_WHITE);
                break;
            case BackgroundBlack:
                attr.SetIndexedBackground(DARK_BLACK);
                break;
            case BackgroundBlue:
                attr.SetIndexedBackground(DARK_BLUE);
                break;
            case BackgroundGreen:
                attr.SetIndexedBackground(DARK_GREEN);
                break;
            case BackgroundCyan:
                attr.SetIndexedBackground(DARK_CYAN);
                break;
            case BackgroundRed:
                attr.SetIndexedBackground(DARK_RED);
                break;
            case BackgroundMagenta:
                attr.SetIndexedBackground(DARK_MAGENTA);
                break;
            case BackgroundYellow:
                attr.SetIndexedBackground(DARK_YELLOW);
                break;
            case BackgroundWhite:
                attr.SetIndexedBackground(DARK_WHITE);
                break;
            case BrightForegroundBlack:
                attr.SetIndexedForeground(BRIGHT_BLACK);
                break;
            case BrightForegroundBlue:
                attr.SetIndexedForeground(BRIGHT_BLUE);
                break;
            case BrightForegroundGreen:
                attr.SetIndexedForeground(BRIGHT_GREEN);
                break;
            case BrightForegroundCyan:
                attr.SetIndexedForeground(BRIGHT_CYAN);
                break;
            case BrightForegroundRed:
                attr.SetIndexedForeground(BRIGHT_RED);
                break;
            case BrightForegroundMagenta:
                attr.SetIndexedForeground(BRIGHT_MAGENTA);
                break;
            case BrightForegroundYellow:
                attr.SetIndexedForeground(BRIGHT_YELLOW);
                break;
            case BrightForegroundWhite:
                attr.SetIndexedForeground(BRIGHT_WHITE);
                break;
            case BrightBackgroundBlack:
                attr.SetIndexedBackground(BRIGHT_BLACK);
                break;
            case BrightBackgroundBlue:
                attr.SetIndexedBackground(BRIGHT_BLUE);
                break;
            case BrightBackgroundGreen:
                attr.SetIndexedBackground(BRIGHT_GREEN);
                break;
            case BrightBackgroundCyan:
                attr.SetIndexedBackground(BRIGHT_CYAN);
                break;
            case BrightBackgroundRed:
                attr.SetIndexedBackground(BRIGHT_RED);
                break;
            case BrightBackgroundMagenta:
                attr.SetIndexedBackground(BRIGHT_MAGENTA);
                break;
            case BrightBackgroundYellow:
                attr.SetIndexedBackground(BRIGHT_YELLOW);
                break;
            case BrightBackgroundWhite:
                attr.SetIndexedBackground(BRIGHT_WHITE);
                break;
            case ForegroundExtended:
                i += _SetRgbColorsHelper(options.substr(i + 1), attr, true);
                break;
            case BackgroundExtended:
                i += _SetRgbColorsHelper(options.substr(i + 1), attr, false);
                break;
            }
        }
        success = _pConApi->PrivateSetTextAttributes(attr);
    }

    return success;
}
