// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "SixelParser.hpp"
#include "adaptDispatch.hpp"
#include "../buffer/out/ImageSlice.hpp"
#include "../parser/ascii.hpp"
#include "../renderer/base/renderer.hpp"
#include "../types/inc/colorTable.hpp"
#include "../types/inc/utils.hpp"

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Utils;
using namespace Microsoft::Console::VirtualTerminal;
using namespace std::chrono;
using namespace std::chrono_literals;

til::size SixelParser::CellSizeForLevel(const VTInt conformanceLevel) noexcept
{
    switch (conformanceLevel)
    {
    case 1: // Compatible with the VT125
        return { 9, 20 };
    default: // Compatible with the VT240 and VT340
        return { 10, 20 };
    }
}

size_t SixelParser::MaxColorsForLevel(const VTInt conformanceLevel) noexcept
{
    switch (conformanceLevel)
    {
    case 1:
    case 2: // Compatible with the 4-color VT125 and VT240
        return 4;
    case 3: // Compatible with the 16-color VT340
        return 16;
    default: // Modern sixel apps often require 256 colors.
        return MAX_COLORS;
    }
}

SixelParser::SixelParser(AdaptDispatch& dispatcher, const StateMachine& stateMachine, const VTInt conformanceLevel) noexcept :
    _dispatcher{ dispatcher },
    _stateMachine{ stateMachine },
    _conformanceLevel{ conformanceLevel },
    _cellSize{ CellSizeForLevel(conformanceLevel) },
    _maxColors{ MaxColorsForLevel(conformanceLevel) }
{
    // We initialize the first 16 color entries with the VT340 palette, which is
    // also compatible with the 4-color VT125 and VT240. The remaining entries
    // are initialized with the XTerm extended colors.
    Microsoft::Console::Utils::InitializeVT340ColorTable(_colorTable);
    Microsoft::Console::Utils::InitializeExtendedColorTable(_colorTable);
}

void SixelParser::SoftReset()
{
    // The VT240 is the only terminal known to reset colors with DECSTR.
    // We only reset the first 16, since it only needs 4 of them anyway.
    if (_conformanceLevel == 2)
    {
        Microsoft::Console::Utils::InitializeVT340ColorTable(_colorTable);
        _updateTextColors();
    }
}

void SixelParser::SetDisplayMode(const bool enabled) noexcept
{
    // The display mode determines whether images are clamped at the bottom of
    // the screen (the set state), or scroll when they reach the bottom of the
    // margin area (the reset state). Clamping was the only mode of operation
    // supported prior to the VT340, so we don't allow the mode to be reset on
    // levels 1 and 2.
    if (_conformanceLevel >= 3)
    {
        _displayMode = enabled;
    }
}

std::function<bool(wchar_t)> SixelParser::DefineImage(const VTInt macroParameter, const DispatchTypes::SixelBackground backgroundSelect, const VTParameter backgroundColor)
{
    if (_initTextBufferBoundaries())
    {
        _initRasterAttributes(macroParameter, backgroundSelect);
        _initColorMap(backgroundColor);
        _initImageBuffer();
        _state = States::Normal;
        _parameters.clear();
        return [&](const auto ch) {
            _parseCommandChar(ch);
            return true;
        };
    }
    else
    {
        return nullptr;
    }
}

void SixelParser::_parseCommandChar(const wchar_t ch)
{
    // Characters in the range `?` to `~` encode a sixel value, which is a group
    // of six vertical pixels. After subtracting `?` from the character, you've
    // got a six bit binary value which represents the six pixels.
    if (ch >= '?' && ch <= '~') [[likely]]
    {
        // When preceded by a repeat command, the repeat parameter value denotes
        // the number of times that the following sixel should be repeated.
        const auto repeatCount = _applyPendingCommand();
        _writeToImageBuffer(ch - L'?', repeatCount);
    }
    // Characters `0` to `9` and `;` are used to represent parameter values for
    // commands that require them.
    else if ((ch >= '0' && ch <= '9') || ch == ';')
    {
        _parseParameterChar(ch);
    }
    // The remaining characters represent commands, some of which will execute
    // immediately, but some requiring additional parameter values. In the
    // latter case, the command will only be applied once the next command
    // character is received.
    else
    {
        switch (ch)
        {
        case '#': // DECGCI - Color Introducer
            _applyPendingCommand();
            _state = States::Color;
            _parameters.clear();
            break;
        case '!': // DECGRI - Repeat Introducer
            _applyPendingCommand();
            _state = States::Repeat;
            _parameters.clear();
            break;
        case '$': // DECGCR - Graphics Carriage Return
            _applyPendingCommand();
            _executeCarriageReturn();
            break;
        case '-': // DECGNL - Graphics Next Line
            _applyPendingCommand();
            _executeNextLine();
            break;
        case '+': // Undocumented home command (VT240 only)
            if (_conformanceLevel == 2)
            {
                _applyPendingCommand();
                _executeMoveToHome();
            }
            break;
        case '"': // DECGRA - Set Raster Attributes
            if (_conformanceLevel >= 3)
            {
                _applyPendingCommand();
                _state = States::Attributes;
                _parameters.clear();
            }
            break;
        case AsciiChars::ESC: // End of image sequence
            // At this point we only care about pending color changes. Raster
            // attributes have no effect at the end of a sequence, and a repeat
            // command is only applicable when followed by a sixel value.
            if (_state == States::Color)
            {
                _applyPendingCommand();
            }
            _fillImageBackground();
            _executeCarriageReturn();
            _maybeFlushImageBuffer(true);
            break;
        default:
            break;
        }
    }
}

void SixelParser::_parseParameterChar(const wchar_t ch)
{
    // The most any command requires is 5 parameters (for the color command),
    // so anything after that can be ignored.
    if (_parameters.size() <= 5)
    {
        if (_parameters.empty())
        {
            _parameters.push_back({});
        }

        if (ch == ';')
        {
            _parameters.push_back({});
        }
        else
        {
            const VTInt digit = ch - L'0';
            auto currentValue = _parameters.back().value_or(0);
            currentValue = currentValue * 10 + digit;
            _parameters.back() = std::min(currentValue, MAX_PARAMETER_VALUE);
        }
    }
}

int SixelParser::_applyPendingCommand()
{
    if (_state != States::Normal) [[unlikely]]
    {
        const auto previousState = _state;
        _state = States::Normal;
        switch (previousState)
        {
        case States::Color:
            _defineColor({ _parameters.data(), _parameters.size() });
            return 1;
        case States::Repeat:
            return VTParameters{ _parameters.data(), _parameters.size() }.at(0);
        case States::Attributes:
            _updateRasterAttributes({ _parameters.data(), _parameters.size() });
            return 1;
        }
    }
    return 1;
}

void SixelParser::_executeCarriageReturn() noexcept
{
    _imageWidth = std::max(_imageWidth, _imageCursor.x);
    _imageCursor.x = 0;
}

void SixelParser::_executeNextLine()
{
    _executeCarriageReturn();
    _imageLineCount++;
    _maybeFlushImageBuffer();
    _imageCursor.y += _sixelHeight;
    _availablePixelHeight -= _sixelHeight;
    _resizeImageBuffer(_sixelHeight);
}

void SixelParser::_executeMoveToHome()
{
    _executeCarriageReturn();
    _maybeFlushImageBuffer();
    _imageCursor.y = 0;
    _availablePixelHeight = _textMargins.height() * _cellSize.height;
}

bool SixelParser::_initTextBufferBoundaries()
{
    const auto page = _dispatcher._pages.ActivePage();
    auto validOrigin = true;
    if (_displayMode)
    {
        // When display mode is set, we can write to the full extent of the page
        // and the starting cursor position is the top left of the page.
        _textMargins = { 0, page.Top(), page.Width(), page.Bottom() };
        _textCursor = _textMargins.origin();
        _availablePixelWidth = page.Width() * _cellSize.width;
        _availablePixelHeight = page.Height() * _cellSize.height;
    }
    else
    {
        // When display mode is reset, we're constrained by the text margins,
        // and the starting position is the current cursor position. This must
        // be inside the horizontal margins and above the bottom margin, else
        // nothing will be rendered.
        const auto [topMargin, bottomMargin] = _dispatcher._GetVerticalMargins(page, true);
        const auto [leftMargin, rightMargin] = _dispatcher._GetHorizontalMargins(page.Width());
        _textMargins = til::rect{ leftMargin, topMargin, rightMargin + 1, bottomMargin + 1 };
        _textCursor = page.Cursor().GetPosition();
        _availablePixelWidth = (_textMargins.right - _textCursor.x) * _cellSize.width;
        _availablePixelHeight = (_textMargins.bottom - _textCursor.y) * _cellSize.height;
        validOrigin = _textCursor.x >= leftMargin && _textCursor.x <= rightMargin && _textCursor.y <= bottomMargin;
    }
    _pendingTextScrollCount = 0;

    // The pixel aspect ratio can't be so large that it would prevent a sixel
    // row from fitting within the margin height, so we need to have a limit.
    _maxPixelAspectRatio = _textMargins.height() * _cellSize.height / 6;

    // If the cursor is visible, we need to hide it while the sixel data is
    // being processed. It will be made visible again when we're done.
    _textCursorWasVisible = page.Cursor().IsVisible();
    if (_textCursorWasVisible && validOrigin)
    {
        page.Cursor().SetIsVisible(false);
    }
    return validOrigin;
}

void SixelParser::_initRasterAttributes(const VTInt macroParameter, const DispatchTypes::SixelBackground backgroundSelect) noexcept
{
    if (_conformanceLevel < 3)
    {
        // Prior to the VT340, the pixel aspect ratio was fixed at 2:1.
        _pixelAspectRatio = 2;
    }
    else
    {
        // The macro parameter was originally used on printers to define the
        // pixel aspect ratio and the grid size (the distance between pixels).
        // On graphic terminals, though, it's only used for the aspect ratio,
        // and then only a limited set of ratios are supported.
        switch (macroParameter)
        {
        case 0:
        case 1:
        case 5:
        case 6:
            _pixelAspectRatio = 2;
            break;
        case 2:
            _pixelAspectRatio = 5;
            break;
        case 3:
        case 4:
            _pixelAspectRatio = 3;
            break;
        case 7:
        case 8:
        case 9:
        default:
            // While the default aspect ratio is defined as 2:1, macro parameter
            // values outside the defined range of 0 to 9 should map to 1:1.
            _pixelAspectRatio = 1;
            break;
        }
    }

    // The height of a sixel row is 6 virtual pixels, but if the aspect ratio is
    // greater than one, the height in device pixels is a multiple of that.
    _sixelHeight = 6 * _pixelAspectRatio;
    _segmentHeight = _sixelHeight;

    // On the VT125, the background was always drawn, but for other terminals it
    // depends on the value of the background select parameter.
    const auto transparent = (backgroundSelect == DispatchTypes::SixelBackground::Transparent);
    _backgroundFillRequired = (_conformanceLevel == 1 || !transparent);

    // By default, the filled area will cover the maximum extent allowed.
    _backgroundSize = { til::CoordTypeMax, til::CoordTypeMax };
}

void SixelParser::_updateRasterAttributes(const VTParameters& rasterAttributes)
{
    // The documentation says default values should be interpreted as 1, but
    // the original VT340 hardware interprets omitted parameters as 0, and if
    // the x aspect is 0 (implying division by zero), the update is ignored.
    const auto yAspect = rasterAttributes.at(0).value_or(0);
    const auto xAspect = rasterAttributes.at(1).value_or(0);
    if (xAspect > 0)
    {
        // The documentation suggests the aspect ratio is rounded to the nearest
        // integer, but on the original VT340 hardware it was rounded up.
        _pixelAspectRatio = std::clamp(static_cast<int>(std::ceil(yAspect * 1.0 / xAspect)), 1, _maxPixelAspectRatio);
        _sixelHeight = 6 * _pixelAspectRatio;
        // When the sixel height is changed multiple times in a row, the segment
        // height has to track the maximum of all the sixel heights used.
        _segmentHeight = std::max(_segmentHeight, _sixelHeight);
        _resizeImageBuffer(_sixelHeight);
    }

    // Although it's not clear from the documentation, we know from testing on
    // a VT340 that the background dimensions are measured in device pixels, so
    // the given height does not need to be scaled by the pixel aspect ratio.
    const auto width = rasterAttributes.at(2).value_or(0);
    const auto height = rasterAttributes.at(3).value_or(0);

    // If these values are omitted or 0, they default to what they were before,
    // which typically would mean filling the whole screen, but could also fall
    // back to the dimensions from an earlier raster attributes command.
    _backgroundSize.width = width > 0 ? width : _backgroundSize.width;
    _backgroundSize.height = height > 0 ? height : _backgroundSize.height;
}

void SixelParser::_scrollTextBuffer(Page& page, const int scrollAmount)
{
    // We scroll the text buffer by moving the cursor to the bottom of the
    // margin area and executing an appropriate number of line feeds.
    if (_textCursor.y != _textMargins.bottom - 1)
    {
        _textCursor = { _textCursor.x, _textMargins.bottom - 1 };
        page.Cursor().SetPosition(_textCursor);
    }
    auto panAmount = 0;
    for (auto i = 0; i < scrollAmount; i++)
    {
        if (_dispatcher._DoLineFeed(page, false, false))
        {
            page.MoveViewportDown();
            panAmount++;
        }
    }

    // If the line feeds panned the viewport down, we need to adjust our margins
    // and text cursor coordinates to align with that movement.
    _textCursor.y += panAmount;
    _textMargins += til::point{ 0, panAmount };

    // And if it wasn't all panning, we need to move the image origin up to
    // match the number of rows that were actually scrolled.
    if (scrollAmount > panAmount)
    {
        auto expectedMovement = scrollAmount - panAmount;
        // If constrained by margins, we can only move as far as the top margin.
        if (_textMargins.top > page.Top() || _textMargins.left > 0 || _textMargins.right < page.Width() - 1)
        {
            const auto availableSpace = std::max(_imageOriginCell.y - _textMargins.top, 0);
            if (expectedMovement > availableSpace)
            {
                // Anything more than that will need to be erased from the
                // image. And if the origin was already above the top margin,
                // this erased segment will be partway through the image.
                const auto eraseRowCount = expectedMovement - availableSpace;
                const auto eraseOffset = std::max(_textMargins.top - _imageOriginCell.y, 0);
                _eraseImageBufferRows(eraseRowCount, eraseOffset);
                // But if there was any available space, we still then need to
                // move the origin up as far as it can go.
                expectedMovement = availableSpace;
            }
        }
        _imageOriginCell.y -= expectedMovement;
    }
}

void SixelParser::_updateTextCursor(Cursor& cursor) noexcept
{
    // Unless the sixel display mode is set, we need to update the text cursor
    // position to align with the final image cursor position. This should be
    // the cell which is intersected by the top of the final sixel row.
    if (!_displayMode)
    {
        const auto finalRow = _imageOriginCell.y + _imageCursor.y / _cellSize.height;
        if (finalRow != _textCursor.y)
        {
            cursor.SetPosition({ _textCursor.x, finalRow });
        }
    }
    // And if the cursor was visible when we started, we need to restore it.
    if (_textCursorWasVisible)
    {
        cursor.SetIsVisible(true);
    }
}

void SixelParser::_initColorMap(const VTParameter backgroundColor)
{
    _colorsUsed = 0;
    _colorsAvailable = _maxColors;
    _colorTableChanged = false;

    // The color numbers in a sixel image don't necessarily map directly to
    // entries in the color table. That mapping is determined by the order in
    // which the colors are defined. If they aren't defined, though, the default
    // mapping is just the color number modulo the color table size.
    for (size_t colorNumber = 0; colorNumber < _colorMap.size(); colorNumber++)
    {
        _colorMap.at(colorNumber) = gsl::narrow_cast<IndexType>(colorNumber % _maxColors);
    }

    // The _colorMapUsed field keeps track of the color numbers that have been
    // explicitly mapped to a color table entry, since that locks in the mapping
    // for the duration of the image. Additional definitions for that color
    // number will update the existing mapped table entry - they won't generate
    // new mappings for the number.
    std::fill(_colorMapUsed.begin(), _colorMapUsed.end(), false);

    // The VT240 has an extra feature, whereby the P3 parameter defines the
    // color number to be used for the background (i.e. it's preassigned to
    // table entry 0). If you specify a value larger than the maximum color
    // table index, the number of available colors is reduced by 1, which
    // effectively protects the background color from modification.
    if (_conformanceLevel == 2 && backgroundColor.has_value()) [[unlikely]]
    {
        const size_t colorNumber = backgroundColor.value();
        if (colorNumber < _maxColors)
        {
            til::at(_colorMap, colorNumber) = 0;
            til::at(_colorMapUsed, colorNumber) = true;
        }
        else
        {
            _colorsAvailable = _maxColors - 1;
        }
    }

    // On the original hardware terminals, the default color index would have
    // been the last entry in the color table. But on modern terminals, it is
    // typically capped at 15 for compatibility with the 16-color VT340. This
    // is the color used if no color commands are received.
    const auto defaultColorIndex = std::min<size_t>(_maxColors - 1, 15);
    _foregroundPixel = { .colorIndex = gsl::narrow_cast<IndexType>(defaultColorIndex) };
}

void SixelParser::_defineColor(const VTParameters& colorParameters)
{
    // The first parameter selects the color number to use. If it's greater than
    // the color map size, we just mod the value into range.
    const auto colorNumber = colorParameters.at(0).value_or(0) % _colorMap.size();

    // If there are additional parameters, then this command will also redefine
    // the color palette associated with the selected color number. This is not
    // supported on the VT125 though.
    if (colorParameters.size() > 1 && _conformanceLevel > 1) [[unlikely]]
    {
        const auto model = DispatchTypes::ColorModel{ colorParameters.at(1) };
        const auto x = colorParameters.at(2).value_or(0);
        const auto y = colorParameters.at(3).value_or(0);
        const auto z = colorParameters.at(4).value_or(0);
        switch (model)
        {
        case DispatchTypes::ColorModel::HLS:
            _defineColor(colorNumber, Utils::ColorFromHLS(x, y, z));
            break;
        case DispatchTypes::ColorModel::RGB:
            _defineColor(colorNumber, Utils::ColorFromRGB100(x, y, z));
            break;
        }
    }

    // The actual color table index we use is derived from the color number via
    // the color map. This is initially defined in _initColorMap above, but may
    // be altered when colors are set in the _defineColor method below.
    const auto colorIndex = _colorMap.at(colorNumber);
    _foregroundPixel = { .colorIndex = colorIndex };
}

void SixelParser::_defineColor(const size_t colorNumber, const COLORREF color)
{
    if (til::at(_colorMapUsed, colorNumber))
    {
        // If the color is already assigned, we update the mapped table entry.
        const auto tableIndex = til::at(_colorMap, colorNumber);
        til::at(_colorTable, tableIndex) = color;
        _colorTableChanged = true;
        // If some image content has already been defined at this point, and
        // we're processing the last character in the packet, this is likely an
        // attempt to animate the palette, so we should flush the image.
        if (_imageWidth > 0 && _stateMachine.IsProcessingLastCharacter())
        {
            _maybeFlushImageBuffer();
        }
    }
    else
    {
        // Otherwise assign it to the next available color table entry.
        if (_colorsUsed < _colorsAvailable)
        {
            // Since table entry 0 is the background color, which you typically
            // want to leave unchanged, the original hardware terminals would
            // skip that and start with table entry 1, and only wrap back to 0
            // when all others had been used.
            const auto tableIndex = ++_colorsUsed % _maxColors;
            til::at(_colorMap, colorNumber) = gsl::narrow_cast<IndexType>(tableIndex);
            til::at(_colorTable, tableIndex) = color;
            _colorTableChanged = true;
        }
        else if (_conformanceLevel == 2)
        {
            // If we've used up all the available color table entries, we have
            // to assign this color number to one of the previously used ones.
            // The VT240 uses the closest match from the existing color entries,
            // but the VT340 just uses the default mapping assigned at the start
            // (i.e. the color number modulo the color table size).
            size_t tableIndex = 0;
            int bestDiff = std::numeric_limits<int>::max();
            for (size_t i = 0; i < _maxColors; i++)
            {
                const auto existingColor = til::at(_colorTable, i);
                const auto diff = [](const auto c1, const auto c2) noexcept {
                    return static_cast<int>(c1) - static_cast<int>(c2);
                };
                const auto redDiff = diff(GetRValue(existingColor), GetRValue(color));
                const auto greenDiff = diff(GetGValue(existingColor), GetGValue(color));
                const auto blueDiff = diff(GetBValue(existingColor), GetBValue(color));
                const auto totalDiff = redDiff * redDiff + greenDiff * greenDiff + blueDiff * blueDiff;
                if (totalDiff <= bestDiff)
                {
                    bestDiff = totalDiff;
                    tableIndex = i;
                }
            }
            til::at(_colorMap, colorNumber) = gsl::narrow_cast<IndexType>(tableIndex);
        }
        til::at(_colorMapUsed, colorNumber) = true;
    }
}

COLORREF SixelParser::_colorFromIndex(const IndexType tableIndex) const noexcept
{
    return til::at(_colorTable, tableIndex);
}

constexpr RGBQUAD SixelParser::_makeRGBQUAD(const COLORREF color) noexcept
{
    return RGBQUAD{
        .rgbBlue = GetBValue(color),
        .rgbGreen = GetGValue(color),
        .rgbRed = GetRValue(color),
        .rgbReserved = 255
    };
}

void SixelParser::_updateTextColors()
{
    // On the original hardware terminals, text and images shared the same
    // color table, so palette changes made in an image would be reflected in
    // the text output as well.
    if (_conformanceLevel <= 3 && _maxColors > 2 && _colorTableChanged) [[unlikely]]
    {
        for (IndexType tableIndex = 0; tableIndex < _maxColors; tableIndex++)
        {
            _dispatcher.SetColorTableEntry(tableIndex, _colorFromIndex(tableIndex));
        }
        _colorTableChanged = false;
    }
}

void SixelParser::_initImageBuffer()
{
    _imageBuffer.clear();
    _imageOriginCell = _textCursor;
    _imageCursor = {};
    _imageWidth = 0;
    _imageMaxWidth = _availablePixelWidth;
    _imageLineCount = 0;
    _resizeImageBuffer(_sixelHeight);

    _lastFlushLine = 0;
    _lastFlushTime = steady_clock::now();

    // Prior to the VT340, the background was filled as soon as the sixel
    // definition was started, because the initial raster attributes could
    // not be altered.
    if (_conformanceLevel < 3)
    {
        _fillImageBackground();
    }
}

void SixelParser::_resizeImageBuffer(const til::CoordType requiredHeight)
{
    const auto requiredSize = (_imageCursor.y + requiredHeight) * _imageMaxWidth;
    if (static_cast<size_t>(requiredSize) > _imageBuffer.size())
    {
        static constexpr auto transparentPixel = IndexedPixel{ .transparent = true };
        _imageBuffer.resize(requiredSize, transparentPixel);
    }
}

void SixelParser::_fillImageBackground()
{
    if (_backgroundFillRequired) [[unlikely]]
    {
        _backgroundFillRequired = false;

        const auto backgroundHeight = std::min(_backgroundSize.height, _availablePixelHeight);
        const auto backgroundWidth = std::min(_backgroundSize.width, _availablePixelWidth);
        _resizeImageBuffer(backgroundHeight);

        // When a background fill is requested, we prefill the buffer with the 0
        // color index, up to the boundaries set by the raster attributes (or if
        // none were given, up to the page boundaries). The actual image output
        // isn't limited by the background dimensions though.
        static constexpr auto backgroundPixel = IndexedPixel{};
        const auto backgroundOffset = _imageCursor.y * _imageMaxWidth;
        auto dst = std::next(_imageBuffer.begin(), backgroundOffset);
        for (auto i = 0; i < backgroundHeight; i++)
        {
            std::fill_n(dst, backgroundWidth, backgroundPixel);
            std::advance(dst, _imageMaxWidth);
        }

        _imageWidth = std::max(_imageWidth, backgroundWidth);
    }
}

void SixelParser::_writeToImageBuffer(int sixelValue, int repeatCount)
{
    // On terminals that support the raster attributes command (which sets the
    // background size), the background is only drawn when the first sixel value
    // is received. So if we haven't filled it yet, we need to do so now.
    _fillImageBackground();

    // Then we need to render the 6 vertical pixels that are represented by the
    // bits in the sixel value. Although note that each of these sixel pixels
    // may cover more than one device pixel, depending on the aspect ratio.
    const auto targetOffset = _imageCursor.y * _imageMaxWidth + _imageCursor.x;
    auto imageBufferPtr = std::next(_imageBuffer.data(), targetOffset);
    repeatCount = std::min(repeatCount, _imageMaxWidth - _imageCursor.x);
    for (auto i = 0; i < 6; i++)
    {
        if (sixelValue & 1)
        {
            auto repeatAspectRatio = _pixelAspectRatio;
            do
            {
                std::fill_n(imageBufferPtr, repeatCount, _foregroundPixel);
                std::advance(imageBufferPtr, _imageMaxWidth);
            } while (--repeatAspectRatio > 0);
        }
        else
        {
            std::advance(imageBufferPtr, _imageMaxWidth * _pixelAspectRatio);
        }
        sixelValue >>= 1;
    }
    _imageCursor.x += repeatCount;
}

void SixelParser::_eraseImageBufferRows(const int rowCount, const til::CoordType rowOffset) noexcept
{
    const auto pixelCount = rowCount * _cellSize.height;
    const auto bufferOffset = rowOffset * _cellSize.height * _imageMaxWidth;
    const auto bufferOffsetEnd = bufferOffset + pixelCount * _imageMaxWidth;
    if (static_cast<size_t>(bufferOffsetEnd) >= _imageBuffer.size()) [[unlikely]]
    {
        _imageBuffer.clear();
        _imageCursor.y = 0;
    }
    else
    {
        _imageBuffer.erase(_imageBuffer.begin() + bufferOffset, _imageBuffer.begin() + bufferOffsetEnd);
        _imageCursor.y -= pixelCount;
    }
}

void SixelParser::_maybeFlushImageBuffer(const bool endOfSequence)
{
    // Regardless of whether we flush the image or not, we always calculate how
    // much we need to scroll in advance. This algorithm is a bit odd. If there
    // isn't enough space for the current segment, it'll scroll until it can fit
    // the segment with a pixel to spare. So in the case that it's an exact fit,
    // it's expected that we'd scroll an additional line. Although this is not
    // common, since it only occurs for pixel aspect ratios of 4:1 or more. Also
    // note that we never scroll more than the margin height, since that would
    // result in the top of the segment being pushed offscreen.
    if (_segmentHeight > _availablePixelHeight && !_displayMode) [[unlikely]]
    {
        const auto marginPixelHeight = _textMargins.height() * _cellSize.height;
        while (_availablePixelHeight < marginPixelHeight && _segmentHeight >= _availablePixelHeight)
        {
            _pendingTextScrollCount += 1;
            _availablePixelHeight += _cellSize.height;
        }
    }

    // Once we've calculated how much scrolling was necessary for the existing
    // segment height, we don't need to track that any longer. The next segment
    // will start with the active sixel height.
    _segmentHeight = _sixelHeight;

    // This method is called after every newline (DECGNL), but we don't want to
    // render partial output for high speed image sequences like video, so we
    // only flush if it has been more than 500ms since the last flush, or it
    // appears that the output is intentionally streamed. If the current buffer
    // has ended with a newline, and we've received no more than one line since
    // the last flush, that suggest it's an intentional break in the stream.
    const auto currentTime = steady_clock::now();
    const auto timeSinceLastFlush = duration_cast<milliseconds>(currentTime - _lastFlushTime);
    const auto linesSinceLastFlush = _imageLineCount - _lastFlushLine;
    if (endOfSequence || timeSinceLastFlush > 500ms || (linesSinceLastFlush <= 1 && _stateMachine.IsProcessingLastCharacter()))
    {
        _lastFlushTime = currentTime;
        _lastFlushLine = _imageLineCount;

        // Before we output anything, we need to scroll the text buffer to make
        // space for the image, using the precalculated scroll count from above.
        auto page = _dispatcher._pages.ActivePage();
        if (_pendingTextScrollCount > 0) [[unlikely]]
        {
            _scrollTextBuffer(page, _pendingTextScrollCount);
            _pendingTextScrollCount = 0;
        }

        // If there's no image width, there's nothing to render at this point,
        // so the only visible change will be the scrolling.
        if (_imageWidth > 0)
        {
            const auto columnBegin = _imageOriginCell.x;
            const auto columnEnd = _imageOriginCell.x + (_imageWidth + _cellSize.width - 1) / _cellSize.width;
            auto rowOffset = _imageOriginCell.y;
            auto srcIterator = _imageBuffer.begin();
            while (srcIterator < _imageBuffer.end() && rowOffset < page.Bottom())
            {
                if (rowOffset >= 0)
                {
                    auto& dstRow = page.Buffer().GetMutableRowByOffset(rowOffset);
                    auto dstSlice = dstRow.GetMutableImageSlice();
                    if (!dstSlice)
                    {
                        dstSlice = dstRow.SetImageSlice(std::make_unique<ImageSlice>(_cellSize));
                        __assume(dstSlice != nullptr);
                    }
                    auto dstIterator = dstSlice->MutablePixels(columnBegin, columnEnd);
                    for (auto pixelRow = 0; pixelRow < _cellSize.height; pixelRow++)
                    {
                        for (auto pixelColumn = 0; pixelColumn < _imageWidth; pixelColumn++)
                        {
                            const auto srcPixel = til::at(srcIterator, pixelColumn);
                            if (!srcPixel.transparent)
                            {
                                const auto srcColor = _colorFromIndex(srcPixel.colorIndex);
                                til::at(dstIterator, pixelColumn) = _makeRGBQUAD(srcColor);
                            }
                        }
                        std::advance(srcIterator, _imageMaxWidth);
                        if (srcIterator >= _imageBuffer.end())
                        {
                            break;
                        }
                        std::advance(dstIterator, dstSlice->PixelWidth());
                    }
                }
                else
                {
                    std::advance(srcIterator, _imageMaxWidth * _cellSize.height);
                }
                rowOffset++;
            }

            // Trigger a redraw of the affected rows in the renderer.
            const auto topRowOffset = std::max(_imageOriginCell.y, 0);
            const auto dirtyView = Viewport::FromExclusive({ 0, topRowOffset, page.Width(), rowOffset });
            page.Buffer().TriggerRedraw(dirtyView);

            // If the start of the image is now above the top of the page, we
            // won't be making any further updates to that content, so we can
            // erase it from our local buffer
            if (_imageOriginCell.y < page.Top())
            {
                const auto rowsToDelete = page.Top() - _imageOriginCell.y;
                _eraseImageBufferRows(rowsToDelete);
                _imageOriginCell.y += rowsToDelete;
            }
        }

        // On lower conformance levels, we also update the text colors.
        _updateTextColors();

        // And at the end of the sequence, we update the text cursor position.
        if (endOfSequence)
        {
            _updateTextCursor(page.Cursor());
        }
    }
}
