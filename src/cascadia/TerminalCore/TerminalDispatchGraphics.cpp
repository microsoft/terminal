// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalDispatch.hpp"

using namespace Microsoft::Console::VirtualTerminal;
using namespace Microsoft::Console::VirtualTerminal::DispatchTypes;

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
size_t TerminalDispatch::_SetRgbColorsHelper(const VTParameters options,
                                             TextAttribute& attr,
                                             const bool isForeground)
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
            const auto rgbColor = RGB(red, green, blue);
            attr.SetColor(rgbColor, isForeground);
        }
    }
    else if (typeOpt == DispatchTypes::GraphicsOptions::BlinkOrXterm256Index)
    {
        optionsConsumed = 2;
        const size_t tableIndex = options.at(1).value_or(0);
        if (tableIndex <= 255)
        {
            const auto adjustedIndex = gsl::narrow_cast<BYTE>(tableIndex);
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
bool TerminalDispatch::SetGraphicsRendition(const VTParameters options)
{
    auto attr = _terminalApi.GetTextAttributes();

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
        case Intense:
            attr.SetIntense(true);
            break;
        case RGBColorOrFaint:
            attr.SetFaint(true);
            break;
        case NotIntenseOrFaint:
            attr.SetIntense(false);
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
            attr.SetIndexedForeground(TextColor::DARK_BLACK);
            break;
        case ForegroundBlue:
            attr.SetIndexedForeground(TextColor::DARK_BLUE);
            break;
        case ForegroundGreen:
            attr.SetIndexedForeground(TextColor::DARK_GREEN);
            break;
        case ForegroundCyan:
            attr.SetIndexedForeground(TextColor::DARK_CYAN);
            break;
        case ForegroundRed:
            attr.SetIndexedForeground(TextColor::DARK_RED);
            break;
        case ForegroundMagenta:
            attr.SetIndexedForeground(TextColor::DARK_MAGENTA);
            break;
        case ForegroundYellow:
            attr.SetIndexedForeground(TextColor::DARK_YELLOW);
            break;
        case ForegroundWhite:
            attr.SetIndexedForeground(TextColor::DARK_WHITE);
            break;
        case BackgroundBlack:
            attr.SetIndexedBackground(TextColor::DARK_BLACK);
            break;
        case BackgroundBlue:
            attr.SetIndexedBackground(TextColor::DARK_BLUE);
            break;
        case BackgroundGreen:
            attr.SetIndexedBackground(TextColor::DARK_GREEN);
            break;
        case BackgroundCyan:
            attr.SetIndexedBackground(TextColor::DARK_CYAN);
            break;
        case BackgroundRed:
            attr.SetIndexedBackground(TextColor::DARK_RED);
            break;
        case BackgroundMagenta:
            attr.SetIndexedBackground(TextColor::DARK_MAGENTA);
            break;
        case BackgroundYellow:
            attr.SetIndexedBackground(TextColor::DARK_YELLOW);
            break;
        case BackgroundWhite:
            attr.SetIndexedBackground(TextColor::DARK_WHITE);
            break;
        case BrightForegroundBlack:
            attr.SetIndexedForeground(TextColor::BRIGHT_BLACK);
            break;
        case BrightForegroundBlue:
            attr.SetIndexedForeground(TextColor::BRIGHT_BLUE);
            break;
        case BrightForegroundGreen:
            attr.SetIndexedForeground(TextColor::BRIGHT_GREEN);
            break;
        case BrightForegroundCyan:
            attr.SetIndexedForeground(TextColor::BRIGHT_CYAN);
            break;
        case BrightForegroundRed:
            attr.SetIndexedForeground(TextColor::BRIGHT_RED);
            break;
        case BrightForegroundMagenta:
            attr.SetIndexedForeground(TextColor::BRIGHT_MAGENTA);
            break;
        case BrightForegroundYellow:
            attr.SetIndexedForeground(TextColor::BRIGHT_YELLOW);
            break;
        case BrightForegroundWhite:
            attr.SetIndexedForeground(TextColor::BRIGHT_WHITE);
            break;
        case BrightBackgroundBlack:
            attr.SetIndexedBackground(TextColor::BRIGHT_BLACK);
            break;
        case BrightBackgroundBlue:
            attr.SetIndexedBackground(TextColor::BRIGHT_BLUE);
            break;
        case BrightBackgroundGreen:
            attr.SetIndexedBackground(TextColor::BRIGHT_GREEN);
            break;
        case BrightBackgroundCyan:
            attr.SetIndexedBackground(TextColor::BRIGHT_CYAN);
            break;
        case BrightBackgroundRed:
            attr.SetIndexedBackground(TextColor::BRIGHT_RED);
            break;
        case BrightBackgroundMagenta:
            attr.SetIndexedBackground(TextColor::BRIGHT_MAGENTA);
            break;
        case BrightBackgroundYellow:
            attr.SetIndexedBackground(TextColor::BRIGHT_YELLOW);
            break;
        case BrightBackgroundWhite:
            attr.SetIndexedBackground(TextColor::BRIGHT_WHITE);
            break;
        case ForegroundExtended:
            i += _SetRgbColorsHelper(options.subspan(i + 1), attr, true);
            break;
        case BackgroundExtended:
            i += _SetRgbColorsHelper(options.subspan(i + 1), attr, false);
            break;
        }
    }

    _terminalApi.SetTextAttributes(attr);
    return true;
}

bool TerminalDispatch::PushGraphicsRendition(const VTParameters options)
{
    _terminalApi.PushGraphicsRendition(options);
    return true;
}

bool TerminalDispatch::PopGraphicsRendition()
{
    _terminalApi.PopGraphicsRendition();
    return true;
}
