// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "adaptDispatch.hpp"
#include "../../types/inc/utils.hpp"

#define ENABLE_INTSAFE_SIGNED_FUNCTIONS
#include <intsafe.h>

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
        // We only apply the color if the R, G, B values fit within a byte.
        // This is to match XTerm's and VTE's behavior.
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

        // We only apply the color if the index value fit within a byte.
        // This is to match XTerm's and VTE's behavior.
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
// - Helper to parse extended graphics options, which start with 38 (FG) or 48 (BG)
//   - These options are followed by either a 2 (RGB) or 5 (xterm index):
//     - RGB sequences then take 4 MORE options to designate the ColorSpaceID, R, G, B parts
//       of the color.
//     - Xterm index will use the option that follows to use a color from the
//       preset 256 color xterm color table.
// Arguments:
// - colorItem - One of FG(38) and BG(48), indicating which color we're setting.
// - options - An array of options that will be used to generate the RGB color
// - attr - The attribute that will be updated with the parsed color.
// Return Value:
// - <none>
void AdaptDispatch::_SetRgbColorsHelperFromSubParams(const VTParameter colorItem,
                                                     const VTSubParameters options,
                                                     TextAttribute& attr) noexcept
{
    // This should be called for applying FG and BG colors only.
    assert(colorItem == GraphicsOptions::ForegroundExtended ||
           colorItem == GraphicsOptions::BackgroundExtended);

    const bool isForeground = (colorItem == GraphicsOptions::ForegroundExtended);
    const DispatchTypes::GraphicsOptions typeOpt = options.at(0);

    if (typeOpt == DispatchTypes::GraphicsOptions::RGBColorOrFaint)
    {
        // sub params are in the order:
        // :2:<color-space-id>:<r>:<g>:<b>

        // We treat a color as invalid, if it has a color space ID, as some
        // applications that support non-standard ODA color sequence may send
        // the red value in its place.
        const bool hasColorSpaceId = options.at(1).has_value();

        // Skip color-space-id.
        const size_t red = options.at(2).value_or(0);
        const size_t green = options.at(3).value_or(0);
        const size_t blue = options.at(4).value_or(0);

        // We only apply the color if the R, G, B values fit within a byte.
        // This is to match XTerm's and VTE's behavior.
        if (!hasColorSpaceId && red <= 255 && green <= 255 && blue <= 255)
        {
            const auto rgbColor = RGB(red, green, blue);
            attr.SetColor(rgbColor, isForeground);
        }
    }
    else if (typeOpt == DispatchTypes::GraphicsOptions::BlinkOrXterm256Index)
    {
        // sub params are in the order:
        // :5:<n>
        // where 'n' is the index into the xterm color table.
        const size_t tableIndex = options.at(1).value_or(0);

        // We only apply the color if the index value fit within a byte.
        // This is to match XTerm's and VTE's behavior.
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
}

// Routine Description:
// - Helper to apply a single graphic rendition option to an attribute.
// - Calls appropriate helper to apply the option with sub parameters when necessary.
// Arguments:
// - options - An array of options.
// - optionIndex - The start index of the option that will be applied.
// - attr - The attribute that will be updated with the applied option.
// Return Value:
// - The number of entries in the array that were consumed.
size_t AdaptDispatch::_ApplyGraphicsOption(const VTParameters options,
                                           const size_t optionIndex,
                                           TextAttribute& attr) noexcept
{
    const GraphicsOptions opt = options.at(optionIndex);

    if (options.hasSubParamsFor(optionIndex))
    {
        const auto subParams = options.subParamsFor(optionIndex);
        _ApplyGraphicsOptionWithSubParams(opt, subParams, attr);
        return 1;
    }

    switch (opt)
    {
    case Off:
        attr.SetDefaultForeground();
        attr.SetDefaultBackground();
        attr.SetDefaultRenditionAttributes();
        return 1;
    case ForegroundDefault:
        attr.SetDefaultForeground();
        return 1;
    case BackgroundDefault:
        attr.SetDefaultBackground();
        return 1;
    case Intense:
        attr.SetIntense(true);
        return 1;
    case RGBColorOrFaint:
        attr.SetFaint(true);
        return 1;
    case NotIntenseOrFaint:
        attr.SetIntense(false);
        attr.SetFaint(false);
        return 1;
    case Italics:
        attr.SetItalic(true);
        return 1;
    case NotItalics:
        attr.SetItalic(false);
        return 1;
    case BlinkOrXterm256Index:
    case RapidBlink: // We just interpret rapid blink as an alias of blink.
        attr.SetBlinking(true);
        return 1;
    case Steady:
        attr.SetBlinking(false);
        return 1;
    case Invisible:
        attr.SetInvisible(true);
        return 1;
    case Visible:
        attr.SetInvisible(false);
        return 1;
    case CrossedOut:
        attr.SetCrossedOut(true);
        return 1;
    case NotCrossedOut:
        attr.SetCrossedOut(false);
        return 1;
    case Negative:
        attr.SetReverseVideo(true);
        return 1;
    case Positive:
        attr.SetReverseVideo(false);
        return 1;
    case Underline:
        attr.SetUnderlined(true);
        return 1;
    case DoublyUnderlined:
        attr.SetDoublyUnderlined(true);
        return 1;
    case NoUnderline:
        attr.SetUnderlined(false);
        attr.SetDoublyUnderlined(false);
        return 1;
    case Overline:
        attr.SetOverlined(true);
        return 1;
    case NoOverline:
        attr.SetOverlined(false);
        return 1;
    case ForegroundBlack:
        attr.SetIndexedForeground(TextColor::DARK_BLACK);
        return 1;
    case ForegroundBlue:
        attr.SetIndexedForeground(TextColor::DARK_BLUE);
        return 1;
    case ForegroundGreen:
        attr.SetIndexedForeground(TextColor::DARK_GREEN);
        return 1;
    case ForegroundCyan:
        attr.SetIndexedForeground(TextColor::DARK_CYAN);
        return 1;
    case ForegroundRed:
        attr.SetIndexedForeground(TextColor::DARK_RED);
        return 1;
    case ForegroundMagenta:
        attr.SetIndexedForeground(TextColor::DARK_MAGENTA);
        return 1;
    case ForegroundYellow:
        attr.SetIndexedForeground(TextColor::DARK_YELLOW);
        return 1;
    case ForegroundWhite:
        attr.SetIndexedForeground(TextColor::DARK_WHITE);
        return 1;
    case BackgroundBlack:
        attr.SetIndexedBackground(TextColor::DARK_BLACK);
        return 1;
    case BackgroundBlue:
        attr.SetIndexedBackground(TextColor::DARK_BLUE);
        return 1;
    case BackgroundGreen:
        attr.SetIndexedBackground(TextColor::DARK_GREEN);
        return 1;
    case BackgroundCyan:
        attr.SetIndexedBackground(TextColor::DARK_CYAN);
        return 1;
    case BackgroundRed:
        attr.SetIndexedBackground(TextColor::DARK_RED);
        return 1;
    case BackgroundMagenta:
        attr.SetIndexedBackground(TextColor::DARK_MAGENTA);
        return 1;
    case BackgroundYellow:
        attr.SetIndexedBackground(TextColor::DARK_YELLOW);
        return 1;
    case BackgroundWhite:
        attr.SetIndexedBackground(TextColor::DARK_WHITE);
        return 1;
    case BrightForegroundBlack:
        attr.SetIndexedForeground(TextColor::BRIGHT_BLACK);
        return 1;
    case BrightForegroundBlue:
        attr.SetIndexedForeground(TextColor::BRIGHT_BLUE);
        return 1;
    case BrightForegroundGreen:
        attr.SetIndexedForeground(TextColor::BRIGHT_GREEN);
        return 1;
    case BrightForegroundCyan:
        attr.SetIndexedForeground(TextColor::BRIGHT_CYAN);
        return 1;
    case BrightForegroundRed:
        attr.SetIndexedForeground(TextColor::BRIGHT_RED);
        return 1;
    case BrightForegroundMagenta:
        attr.SetIndexedForeground(TextColor::BRIGHT_MAGENTA);
        return 1;
    case BrightForegroundYellow:
        attr.SetIndexedForeground(TextColor::BRIGHT_YELLOW);
        return 1;
    case BrightForegroundWhite:
        attr.SetIndexedForeground(TextColor::BRIGHT_WHITE);
        return 1;
    case BrightBackgroundBlack:
        attr.SetIndexedBackground(TextColor::BRIGHT_BLACK);
        return 1;
    case BrightBackgroundBlue:
        attr.SetIndexedBackground(TextColor::BRIGHT_BLUE);
        return 1;
    case BrightBackgroundGreen:
        attr.SetIndexedBackground(TextColor::BRIGHT_GREEN);
        return 1;
    case BrightBackgroundCyan:
        attr.SetIndexedBackground(TextColor::BRIGHT_CYAN);
        return 1;
    case BrightBackgroundRed:
        attr.SetIndexedBackground(TextColor::BRIGHT_RED);
        return 1;
    case BrightBackgroundMagenta:
        attr.SetIndexedBackground(TextColor::BRIGHT_MAGENTA);
        return 1;
    case BrightBackgroundYellow:
        attr.SetIndexedBackground(TextColor::BRIGHT_YELLOW);
        return 1;
    case BrightBackgroundWhite:
        attr.SetIndexedBackground(TextColor::BRIGHT_WHITE);
        return 1;
    case ForegroundExtended:
        return 1 + _SetRgbColorsHelper(options.subspan(optionIndex + 1), attr, true);
    case BackgroundExtended:
        return 1 + _SetRgbColorsHelper(options.subspan(optionIndex + 1), attr, false);
    default:
        return 1;
    }
}

// Routine Description:
// - Helper to apply a single graphic rendition option with sub parameters to an attribute.
// Arguments:
// - option - An option to apply.
// - subParams - Sub parameters associated with the option.
// - attr - The attribute that will be updated with the applied option.
// Return Value:
// - <None>
void AdaptDispatch::_ApplyGraphicsOptionWithSubParams(const VTParameter option,
                                                      const VTSubParameters subParams,
                                                      TextAttribute& attr) noexcept
{
    // here, we apply our "best effort" rule, while handling sub params if we don't
    // recognise the parameter substring (parameter and it's sub parameters) then
    // we should just skip over them.
    switch (option)
    {
    case ForegroundExtended:
    case BackgroundExtended:
        _SetRgbColorsHelperFromSubParams(option, subParams, attr);
        break;
    default:
        /* do nothing */
        break;
    }
}

// Routine Description:
// - Helper to apply a number of graphic rendition options to an attribute.
// Arguments:
// - options - An array of options that will be applied in sequence.
// - attr - The attribute that will be updated with the applied options.
// Return Value:
// - <none>
void AdaptDispatch::_ApplyGraphicsOptions(const VTParameters options,
                                          TextAttribute& attr) noexcept
{
    for (size_t i = 0; i < options.size();)
    {
        i += _ApplyGraphicsOption(options, i, attr);
    }
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
// - True.
bool AdaptDispatch::SetGraphicsRendition(const VTParameters options)
{
    auto attr = _api.GetTextBuffer().GetCurrentAttributes();
    _ApplyGraphicsOptions(options, attr);
    _api.SetTextAttributes(attr);
    return true;
}

// Routine Description:
// - DECSCA - Modifies the character protection attribute. This operation was
//   originally intended to support a range of logical character attributes,
//   but the protected attribute was the only one ever implemented.
// Arguments:
// - options - An array of options that will be applied in order.
// Return Value:
// - True.
bool AdaptDispatch::SetCharacterProtectionAttribute(const VTParameters options)
{
    auto& textBuffer = _api.GetTextBuffer();
    auto attr = textBuffer.GetCurrentAttributes();
    for (size_t i = 0; i < options.size(); i++)
    {
        const LogicalAttributeOptions opt = options.at(i);
        switch (opt)
        {
        case Default:
            attr.SetProtected(false);
            break;
        case Protected:
            attr.SetProtected(true);
            break;
        case Unprotected:
            attr.SetProtected(false);
            break;
        }
    }
    textBuffer.SetCurrentAttributes(attr);
    return true;
}

// Method Description:
// - Saves the current text attributes to an internal stack.
// Arguments:
// - options: if not empty, specify which portions of the current text attributes should
//   be saved. Options that are not supported are ignored. If no options are specified,
//   all attributes are stored.
// Return Value:
// - True.
bool AdaptDispatch::PushGraphicsRendition(const VTParameters options)
{
    const auto currentAttributes = _api.GetTextBuffer().GetCurrentAttributes();
    _sgrStack.Push(currentAttributes, options);
    return true;
}

// Method Description:
// - Restores text attributes from the internal stack. If only portions of text attributes
//   were saved, combines those with the current attributes.
// Arguments:
// - <none>
// Return Value:
// - True.
bool AdaptDispatch::PopGraphicsRendition()
{
    const auto currentAttributes = _api.GetTextBuffer().GetCurrentAttributes();
    _api.SetTextAttributes(_sgrStack.Pop(currentAttributes));
    return true;
}
