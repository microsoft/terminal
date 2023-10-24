// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "../inc/RenderSettings.hpp"
#include "../base/renderer.hpp"
#include "../../types/inc/ColorFix.hpp"
#include "../../types/inc/colorTable.hpp"

using namespace Microsoft::Console::Render;
using Microsoft::Console::Utils::InitializeColorTable;

RenderSettings::RenderSettings() noexcept
{
    InitializeColorTable(_colorTable);

    SetColorTableEntry(TextColor::DEFAULT_FOREGROUND, INVALID_COLOR);
    SetColorTableEntry(TextColor::DEFAULT_BACKGROUND, INVALID_COLOR);
    SetColorTableEntry(TextColor::FRAME_FOREGROUND, INVALID_COLOR);
    SetColorTableEntry(TextColor::FRAME_BACKGROUND, INVALID_COLOR);
    SetColorTableEntry(TextColor::CURSOR_COLOR, INVALID_COLOR);

    SetColorAliasIndex(ColorAlias::DefaultForeground, TextColor::DARK_WHITE);
    SetColorAliasIndex(ColorAlias::DefaultBackground, TextColor::DARK_BLACK);
    SetColorAliasIndex(ColorAlias::FrameForeground, TextColor::FRAME_FOREGROUND);
    SetColorAliasIndex(ColorAlias::FrameBackground, TextColor::FRAME_BACKGROUND);
}

// Routine Description:
// - Updates the specified render mode.
// Arguments:
// - mode - The render mode to change.
// - enabled - Set to true to enable the mode, false to disable it.
void RenderSettings::SetRenderMode(const Mode mode, const bool enabled) noexcept
{
    _renderMode.set(mode, enabled);
    // If blinking is disabled, make sure blinking content is not faint.
    if (mode == Mode::BlinkAllowed && !enabled)
    {
        _blinkShouldBeFaint = false;
    }
}

// Routine Description:
// - Retrieves the specified render mode.
// Arguments:
// - mode - The render mode to query.
// Return Value:
// - True if the mode is enabled. False if disabled.
bool RenderSettings::GetRenderMode(const Mode mode) const noexcept
{
    return _renderMode.test(mode);
}

// Routine Description:
// - Returns a reference to the active color table array.
const std::array<COLORREF, TextColor::TABLE_SIZE>& RenderSettings::GetColorTable() const noexcept
{
    return _colorTable;
}

// Routine Description:
// - Resets the first 16 color table entries with default values.
void RenderSettings::ResetColorTable() noexcept
{
    InitializeColorTable({ _colorTable.data(), 16 });
}

// Routine Description:
// - Updates the given index in the color table to a new value.
// Arguments:
// - tableIndex - The index of the color to update.
// - color - The new COLORREF to use as that color table value.
void RenderSettings::SetColorTableEntry(const size_t tableIndex, const COLORREF color)
{
    _colorTable.at(tableIndex) = color;
}

// Routine Description:
// - Retrieves the value in the color table at the specified index.
// Arguments:
// - tableIndex - The index of the color to retrieve.
// Return Value:
// - The COLORREF value for the color at that index in the table.
COLORREF RenderSettings::GetColorTableEntry(const size_t tableIndex) const
{
    return _colorTable.at(tableIndex);
}

// Routine Description:
// - Sets the position in the color table for the given color alias and updates the color.
// Arguments:
// - alias - The color alias to update.
// - tableIndex - The new position of the alias in the color table.
// - color - The new COLORREF to assign to that alias.
void RenderSettings::SetColorAlias(const ColorAlias alias, const size_t tableIndex, const COLORREF color)
{
    SetColorAliasIndex(alias, tableIndex);
    SetColorTableEntry(tableIndex, color);
}

// Routine Description:
// - Retrieves the value in the color table of the given color alias.
// Arguments:
// - alias - The color alias to retrieve.
// Return Value:
// - The COLORREF value of the alias.
COLORREF RenderSettings::GetColorAlias(const ColorAlias alias) const
{
    return GetColorTableEntry(GetColorAliasIndex(alias));
}

// Routine Description:
// - Sets the position in the color table for the given color alias.
// Arguments:
// - alias - The color alias to update.
// - tableIndex - The new position of the alias in the color table.
void RenderSettings::SetColorAliasIndex(const ColorAlias alias, const size_t tableIndex) noexcept
{
    if (tableIndex < TextColor::TABLE_SIZE)
    {
        gsl::at(_colorAliasIndices, static_cast<size_t>(alias)) = tableIndex;
    }
}

// Routine Description:
// - Retrieves the position in the color table of the given color alias.
// Arguments:
// - alias - The color alias to retrieve.
// Return Value:
// - The position in the color table where the color is stored.
size_t RenderSettings::GetColorAliasIndex(const ColorAlias alias) const noexcept
{
    return gsl::at(_colorAliasIndices, static_cast<size_t>(alias));
}

// Routine Description:
// - Calculates the RGB colors of a given text attribute, using the current
//   color table configuration and active render settings.
// Arguments:
// - attr - The TextAttribute to retrieve the colors for.
// Return Value:
// - The color values of the attribute's foreground and background.
std::pair<COLORREF, COLORREF> RenderSettings::GetAttributeColors(const TextAttribute& attr) const noexcept
{
    _blinkIsInUse = _blinkIsInUse || attr.IsBlinking();

    const auto fgTextColor = attr.GetForeground();
    const auto bgTextColor = attr.GetBackground();

    const auto defaultFgIndex = GetColorAliasIndex(ColorAlias::DefaultForeground);
    const auto defaultBgIndex = GetColorAliasIndex(ColorAlias::DefaultBackground);

    const auto brightenFg = attr.IsIntense() && GetRenderMode(Mode::IntenseIsBright);
    const auto dimFg = attr.IsFaint() || (_blinkShouldBeFaint && attr.IsBlinking());
    const auto swapFgAndBg = attr.IsReverseVideo() ^ GetRenderMode(Mode::ScreenReversed);

    auto fg = fgTextColor.GetColor(_colorTable, defaultFgIndex, brightenFg);
    auto bg = bgTextColor.GetColor(_colorTable, defaultBgIndex);

    if (dimFg)
    {
        fg = (fg >> 1) & 0x7F7F7F; // Divide foreground color components by two.
    }
    if (swapFgAndBg)
    {
        std::swap(fg, bg);
    }
    if (attr.IsInvisible())
    {
        fg = bg;
    }

    // We intentionally aren't _only_ checking for attr.IsInvisible here, because we also want to
    // catch the cases where the fg was intentionally set to be the same as the bg. In either case,
    // don't adjust the foreground.
    if constexpr (Feature_AdjustIndistinguishableText::IsEnabled())
    {
        if (
            _renderMode.any(Mode::IndexedDistinguishableColors, Mode::AlwaysDistinguishableColors) &&
            fg != bg &&
            (_renderMode.test(Mode::AlwaysDistinguishableColors) || (fgTextColor.IsDefaultOrLegacy() && bgTextColor.IsDefaultOrLegacy())))
        {
            fg = ColorFix::GetPerceivableColor(fg, bg, 0.5f * 0.5f);
        }
    }

    return { fg, bg };
}

// Routine Description:
// - Calculates the RGBA colors of a given text attribute, using the current
//   color table configuration and active render settings. This differs from
//   GetAttributeColors in that it also sets the alpha color components.
// Arguments:
// - attr - The TextAttribute to retrieve the colors for.
// Return Value:
// - The color values of the attribute's foreground and background.
std::pair<COLORREF, COLORREF> RenderSettings::GetAttributeColorsWithAlpha(const TextAttribute& attr) const noexcept
{
    auto [fg, bg] = GetAttributeColors(attr);

    fg |= 0xff000000;
    // We only care about alpha for the default BG (which enables acrylic)
    // If the bg isn't the default bg color, or reverse video is enabled, make it fully opaque.
    if (!attr.BackgroundIsDefault() || (attr.IsReverseVideo() ^ GetRenderMode(Mode::ScreenReversed)) || attr.IsInvisible())
    {
        bg |= 0xff000000;
    }

    return { fg, bg };
}

// Routine Description:
// - Increments the position in the blink cycle, toggling the blink rendition
//   state on every second call, potentially triggering a redraw of the given
//   renderer if there are blinking cells currently in view.
// Arguments:
// - renderer: the renderer that will be redrawn.
void RenderSettings::ToggleBlinkRendition(Renderer& renderer) noexcept
try
{
    if (GetRenderMode(Mode::BlinkAllowed))
    {
        // This method is called with the frequency of the cursor blink rate,
        // but we only want our cells to blink at half that frequency. We thus
        // have a blink cycle that loops through four phases...
        _blinkCycle = (_blinkCycle + 1) % 4;
        // ... and two of those four render the blink attributes as faint.
        _blinkShouldBeFaint = _blinkCycle >= 2;
        // Every two cycles (when the state changes), we need to trigger a
        // redraw, but only if there are actually blink attributes in use.
        if (_blinkIsInUse && _blinkCycle % 2 == 0)
        {
            // We reset the _blinkIsInUse flag before redrawing, so we can
            // get a fresh assessment of the current blink attribute usage.
            _blinkIsInUse = false;
            renderer.TriggerRedrawAll();
        }
    }
}
CATCH_LOG()
