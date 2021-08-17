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
constexpr BYTE BLUE_ATTR      = 0x01;
constexpr BYTE GREEN_ATTR     = 0x02;
constexpr BYTE RED_ATTR       = 0x04;
constexpr BYTE BRIGHT_ATTR    = 0x08;
constexpr BYTE DARK_BLACK     = 0;
constexpr BYTE DARK_RED       = RED_ATTR;
constexpr BYTE DARK_GREEN     = GREEN_ATTR;
constexpr BYTE DARK_YELLOW    = RED_ATTR | GREEN_ATTR;
constexpr BYTE DARK_BLUE      = BLUE_ATTR;
constexpr BYTE DARK_MAGENTA   = RED_ATTR | BLUE_ATTR;
constexpr BYTE DARK_CYAN      = GREEN_ATTR | BLUE_ATTR;
constexpr BYTE DARK_WHITE     = RED_ATTR | GREEN_ATTR | BLUE_ATTR;
constexpr BYTE BRIGHT_BLACK   = BRIGHT_ATTR;
constexpr BYTE BRIGHT_RED     = BRIGHT_ATTR | RED_ATTR;
constexpr BYTE BRIGHT_GREEN   = BRIGHT_ATTR | GREEN_ATTR;
constexpr BYTE BRIGHT_YELLOW  = BRIGHT_ATTR | RED_ATTR | GREEN_ATTR;
constexpr BYTE BRIGHT_BLUE    = BRIGHT_ATTR | BLUE_ATTR;
constexpr BYTE BRIGHT_MAGENTA = BRIGHT_ATTR | RED_ATTR | BLUE_ATTR;
constexpr BYTE BRIGHT_CYAN    = BRIGHT_ATTR | GREEN_ATTR | BLUE_ATTR;
constexpr BYTE BRIGHT_WHITE   = BRIGHT_ATTR | RED_ATTR | GREEN_ATTR | BLUE_ATTR;
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
size_t AdaptDispatch::_SetRgbColorsHelper(const VTParameters options,
                                          TextAttribute& attr,
                                          const bool isForeground) noexcept
{
    size_t optionsConsumed = 1;
    const DispatchTypes::GraphicsOptions typeOpt = options.at(0);
    if (typeOpt == DispatchTypes::GraphicsOptions::RGBColorOrFaint)
    {
        optionsConsumed = 4;
        const size_t red = options.at(1).value_or(0);
        const size_t green = options.at(2).value_or(0);
        const size_t blue = options.at(3).value_or(0);
        // ensure that each value fits in a byte
        if (red <= 255 && green <= 255 && blue <= 255)
        {
            const COLORREF rgbColor = RGB(red, green, blue);
            attr.SetColor(rgbColor, isForeground);
        }
    }
    else if (typeOpt == DispatchTypes::GraphicsOptions::BlinkOrXterm256Index)
    {
        optionsConsumed = 2;
        const size_t tableIndex = options.at(1).value_or(0);
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
bool AdaptDispatch::SetGraphicsRendition(const VTParameters options)
{
    TextAttribute attr;
    bool success = _pConApi->PrivateGetTextAttributes(attr);

    if (success)
    {
        // Run through the graphics options and apply them
        for (size_t i = 0; i < options.size(); i++)
        {
            const GraphicsOptions opt = options.at(i);
            switch (opt)
            {
            case Off:
                attr.SetDefaultForeground();
                attr.SetDefaultBackground();
                attr.SetDefaultMetaAttrs();
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
            case RGBColorOrFaint:
                attr.SetFaint(true);
                break;
            case NotBoldOrFaint:
                attr.SetBold(false);
                attr.SetFaint(false);
                break;
            case Italics:
                attr.SetItalic(true);
                break;
            case NotItalics:
                attr.SetItalic(false);
                break;
            case BlinkOrXterm256Index:
            case RapidBlink: // We just interpret rapid blink as an alias of blink.
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
                attr.SetUnderlined(true);
                break;
            case DoublyUnderlined:
                attr.SetDoublyUnderlined(true);
                break;
            case NoUnderline:
                attr.SetUnderlined(false);
                attr.SetDoublyUnderlined(false);
                break;
            case Overline:
                attr.SetOverlined(true);
                break;
            case NoOverline:
                attr.SetOverlined(false);
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
                i += _SetRgbColorsHelper(options.subspan(i + 1), attr, true);
                break;
            case BackgroundExtended:
                i += _SetRgbColorsHelper(options.subspan(i + 1), attr, false);
                break;
            }
        }
        success = _pConApi->PrivateSetTextAttributes(attr);
    }

    return success;
}

// Method Description:
// - Saves the current text attributes to an internal stack.
// Arguments:
// - options: if not empty, specify which portions of the current text attributes should
//   be saved. Options that are not supported are ignored. If no options are specified,
//   all attributes are stored.
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::PushGraphicsRendition(const VTParameters options)
{
    bool success = true;
    TextAttribute currentAttributes;

    success = _pConApi->PrivateGetTextAttributes(currentAttributes);

    if (success)
    {
        _sgrStack.Push(currentAttributes, options);
    }

    return success;
}

// Method Description:
// - Restores text attributes from the internal stack. If only portions of text attributes
//   were saved, combines those with the current attributes.
// Arguments:
// - <none>
// Return Value:
// - True if handled successfully. False otherwise.
bool AdaptDispatch::PopGraphicsRendition()
{
    bool success = true;
    TextAttribute currentAttributes;

    success = _pConApi->PrivateGetTextAttributes(currentAttributes);

    if (success)
    {
        success = _pConApi->PrivateSetTextAttributes(_sgrStack.Pop(currentAttributes));
    }

    return success;
}
