// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalDispatch.hpp"
using namespace ::Microsoft::Terminal::Core;
using namespace ::Microsoft::Console::VirtualTerminal;

// clang-format off
const BYTE RED_ATTR     = 0x01;
const BYTE GREEN_ATTR   = 0x02;
const BYTE BLUE_ATTR    = 0x04;
const BYTE BRIGHT_ATTR  = 0x08;
const BYTE DARK_BLACK   = 0;
const BYTE DARK_RED     = RED_ATTR;
const BYTE DARK_GREEN   = GREEN_ATTR;
const BYTE DARK_YELLOW  = RED_ATTR | GREEN_ATTR;
const BYTE DARK_BLUE    = BLUE_ATTR;
const BYTE DARK_MAGENTA = RED_ATTR | BLUE_ATTR;
const BYTE DARK_CYAN    = GREEN_ATTR | BLUE_ATTR;
const BYTE DARK_WHITE   = RED_ATTR | GREEN_ATTR | BLUE_ATTR;
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
// Returns true if the GraphicsOption represents an extended color option.
//   These are followed by up to 4 more values which compose the entire option.
// Return Value:
// - true if the opt is the indicator for an extended color sequence, false otherwise.
static constexpr bool _isRgbColorOption(const DispatchTypes::GraphicsOptions opt) noexcept
{
    return opt == DispatchTypes::GraphicsOptions::ForegroundExtended ||
           opt == DispatchTypes::GraphicsOptions::BackgroundExtended;
}

// Routine Description:
// Returns true if the GraphicsOption represents an extended color option.
//   These are followed by up to 4 more values which compose the entire option.
// Return Value:
// - true if the opt is the indicator for an extended color sequence, false otherwise.
static constexpr bool _isBoldColorOption(const DispatchTypes::GraphicsOptions opt) noexcept
{
    return opt == DispatchTypes::GraphicsOptions::BoldBright ||
           opt == DispatchTypes::GraphicsOptions::UnBold;
}

// Function Description:
// - checks if this graphics option should set either the console's FG or BG to
//the default attributes.
// Return Value:
// - true if the opt sets either/or attribute to the defaults, false otherwise.
static constexpr bool _isDefaultColorOption(const DispatchTypes::GraphicsOptions opt) noexcept
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
// - optionsConsumed - Returns the number of options we consumed parsing this option.
// Return Value:
// Returns true if we successfully parsed an extended color option from the options array.
// - This corresponds to the following number of options consumed (pcOptionsConsumed):
//     1 - false, not enough options to parse.
//     2 - false, not enough options to parse.
//     3 - true, parsed an xterm index to a color
//     5 - true, parsed an RGB color.
bool TerminalDispatch::_SetRgbColorsHelper(const std::basic_string_view<DispatchTypes::GraphicsOptions> options,
                                           size_t& optionsConsumed) noexcept
{
    COLORREF color = 0;
    bool isForeground = false;

    bool success = false;
    optionsConsumed = 1;
    if (options.size() >= 2 && _isRgbColorOption(options.front()))
    {
        optionsConsumed = 2;
        const auto extendedOpt = til::at(options, 0);
        const auto typeOpt = til::at(options, 1);

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
            const auto limit = static_cast<DispatchTypes::GraphicsOptions>(255);
            const auto red = std::min(options.at(2), limit);
            const auto green = std::min(options.at(3), limit);
            const auto blue = std::min(options.at(4), limit);

            color = RGB(red, green, blue);

            success = _terminalApi.SetTextRgbColor(color, isForeground);
        }
        else if (typeOpt == DispatchTypes::GraphicsOptions::BlinkOrXterm256Index && options.size() >= 3)
        {
            optionsConsumed = 3;
            if (options.at(2) <= 255) // ensure that the provided index is on the table
            {
                const auto tableIndex = til::at(options, 2);
                success = isForeground ?
                              _terminalApi.SetTextForegroundIndex256(gsl::narrow_cast<BYTE>(tableIndex)) :
                              _terminalApi.SetTextBackgroundIndex256(gsl::narrow_cast<BYTE>(tableIndex));
            }
        }
    }
    return success;
}

bool TerminalDispatch::_SetBoldColorHelper(const DispatchTypes::GraphicsOptions option) noexcept
{
    const bool bold = (option == DispatchTypes::GraphicsOptions::BoldBright);
    return _terminalApi.BoldText(bold);
}

bool TerminalDispatch::_SetDefaultColorHelper(const DispatchTypes::GraphicsOptions option) noexcept
{
    const bool fg = option == DispatchTypes::GraphicsOptions::Off || option == DispatchTypes::GraphicsOptions::ForegroundDefault;
    const bool bg = option == DispatchTypes::GraphicsOptions::Off || option == DispatchTypes::GraphicsOptions::BackgroundDefault;
    bool success = _terminalApi.SetTextToDefaults(fg, bg);

    if (success && fg && bg)
    {
        // If we're resetting both the FG & BG, also reset the meta attributes (underline)
        //      as well as the boldness
        success = _terminalApi.UnderlineText(false) &&
                  _terminalApi.ReverseText(false) &&
                  _terminalApi.BoldText(false);
    }
    return success;
}

// Routine Description:
// - Helper to apply the actual flags to each text attributes field.
// - Placed as a helper so it can be recursive/re-entrant for some of the convenience flag methods that perform similar/multiple operations in one command.
// Arguments:
// - opt - Graphics option sent to us by the parser/requestor.
// - pAttr - Pointer to the font attribute field to adjust
// Return Value:
// - <none>
void TerminalDispatch::_SetGraphicsOptionHelper(const DispatchTypes::GraphicsOptions opt) noexcept
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
        _terminalApi.ReverseText(true);
        break;
    case DispatchTypes::GraphicsOptions::Underline:
        _terminalApi.UnderlineText(true);
        break;
    case DispatchTypes::GraphicsOptions::Positive:
        _terminalApi.ReverseText(false);
        break;
    case DispatchTypes::GraphicsOptions::NoUnderline:
        _terminalApi.UnderlineText(false);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundBlack:
        _terminalApi.SetTextForegroundIndex(DARK_BLACK);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundBlue:
        _terminalApi.SetTextForegroundIndex(DARK_BLUE);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundGreen:
        _terminalApi.SetTextForegroundIndex(DARK_GREEN);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundCyan:
        _terminalApi.SetTextForegroundIndex(DARK_CYAN);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundRed:
        _terminalApi.SetTextForegroundIndex(DARK_RED);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundMagenta:
        _terminalApi.SetTextForegroundIndex(DARK_MAGENTA);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundYellow:
        _terminalApi.SetTextForegroundIndex(DARK_YELLOW);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundWhite:
        _terminalApi.SetTextForegroundIndex(DARK_WHITE);
        break;
    case DispatchTypes::GraphicsOptions::ForegroundDefault:
        FAIL_FAST_MSG("GraphicsOptions::ForegroundDefault should be handled by _SetDefaultColorHelper");
        break;
    case DispatchTypes::GraphicsOptions::BackgroundBlack:
        _terminalApi.SetTextBackgroundIndex(DARK_BLACK);
        break;
    case DispatchTypes::GraphicsOptions::BackgroundBlue:
        _terminalApi.SetTextBackgroundIndex(DARK_BLUE);
        break;
    case DispatchTypes::GraphicsOptions::BackgroundGreen:
        _terminalApi.SetTextBackgroundIndex(DARK_GREEN);
        break;
    case DispatchTypes::GraphicsOptions::BackgroundCyan:
        _terminalApi.SetTextBackgroundIndex(DARK_CYAN);
        break;
    case DispatchTypes::GraphicsOptions::BackgroundRed:
        _terminalApi.SetTextBackgroundIndex(DARK_RED);
        break;
    case DispatchTypes::GraphicsOptions::BackgroundMagenta:
        _terminalApi.SetTextBackgroundIndex(DARK_MAGENTA);
        break;
    case DispatchTypes::GraphicsOptions::BackgroundYellow:
        _terminalApi.SetTextBackgroundIndex(DARK_YELLOW);
        break;
    case DispatchTypes::GraphicsOptions::BackgroundWhite:
        _terminalApi.SetTextBackgroundIndex(DARK_WHITE);
        break;
    case DispatchTypes::GraphicsOptions::BackgroundDefault:
        FAIL_FAST_MSG("GraphicsOptions::BackgroundDefault should be handled by _SetDefaultColorHelper");
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundBlack:
        _terminalApi.SetTextForegroundIndex(BRIGHT_BLACK);
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundBlue:
        _terminalApi.SetTextForegroundIndex(BRIGHT_BLUE);
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundGreen:
        _terminalApi.SetTextForegroundIndex(BRIGHT_GREEN);
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundCyan:
        _terminalApi.SetTextForegroundIndex(BRIGHT_CYAN);
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundRed:
        _terminalApi.SetTextForegroundIndex(BRIGHT_RED);
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundMagenta:
        _terminalApi.SetTextForegroundIndex(BRIGHT_MAGENTA);
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundYellow:
        _terminalApi.SetTextForegroundIndex(BRIGHT_YELLOW);
        break;
    case DispatchTypes::GraphicsOptions::BrightForegroundWhite:
        _terminalApi.SetTextForegroundIndex(BRIGHT_WHITE);
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundBlack:
        _terminalApi.SetTextBackgroundIndex(BRIGHT_BLACK);
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundBlue:
        _terminalApi.SetTextBackgroundIndex(BRIGHT_BLUE);
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundGreen:
        _terminalApi.SetTextBackgroundIndex(BRIGHT_GREEN);
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundCyan:
        _terminalApi.SetTextBackgroundIndex(BRIGHT_CYAN);
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundRed:
        _terminalApi.SetTextBackgroundIndex(BRIGHT_RED);
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundMagenta:
        _terminalApi.SetTextBackgroundIndex(BRIGHT_MAGENTA);
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundYellow:
        _terminalApi.SetTextBackgroundIndex(BRIGHT_YELLOW);
        break;
    case DispatchTypes::GraphicsOptions::BrightBackgroundWhite:
        _terminalApi.SetTextBackgroundIndex(BRIGHT_WHITE);
        break;
    }
}

bool TerminalDispatch::SetGraphicsRendition(const std::basic_string_view<DispatchTypes::GraphicsOptions> options) noexcept
{
    bool success = false;
    // Run through the graphics options and apply them
    for (size_t i = 0; i < options.size(); i++)
    {
        const auto opt = options.at(i);
        if (_isDefaultColorOption(opt))
        {
            success = _SetDefaultColorHelper(opt);
        }
        else if (_isBoldColorOption(opt))
        {
            success = _SetBoldColorHelper(opt);
        }
        else if (_isRgbColorOption(opt))
        {
            size_t optionsConsumed = 0;

            // _SetRgbColorsHelper will call the appropriate ConApi function
            success = _SetRgbColorsHelper(options.substr(i), optionsConsumed);

            i += (optionsConsumed - 1); // optionsConsumed includes the opt we're currently on.
        }
        else
        {
            _SetGraphicsOptionHelper(opt);

            // Make sure we un-bold
            if (success && opt == DispatchTypes::GraphicsOptions::Off)
            {
                success = _SetBoldColorHelper(opt);
            }
        }
    }
    return success;
}
