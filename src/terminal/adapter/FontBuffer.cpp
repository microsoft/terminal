// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "FontBuffer.hpp"

using namespace Microsoft::Console::VirtualTerminal;

FontBuffer::FontBuffer() noexcept
{
    SetEraseControl(DispatchTypes::DrcsEraseControl::AllRenditions);
};

bool FontBuffer::SetEraseControl(const DispatchTypes::DrcsEraseControl eraseControl) noexcept
{
    switch (eraseControl)
    {
    case DispatchTypes::DrcsEraseControl::AllChars:
    case DispatchTypes::DrcsEraseControl::AllRenditions:
        // Setting the current cell matrix to an invalid value will guarantee
        // that it's different from the pending cell matrix, and any change in
        // the font attributes will force the buffer to be cleared.
        _cellMatrix = DispatchTypes::DrcsCellMatrix::Invalid;
        return true;
    case DispatchTypes::DrcsEraseControl::ReloadedChars:
        return true;
    default:
        return false;
    }
}

bool FontBuffer::SetAttributes(const DispatchTypes::DrcsCellMatrix cellMatrix,
                               const VTParameter cellHeight,
                               const DispatchTypes::DrcsFontSet fontSet,
                               const DispatchTypes::DrcsFontUsage fontUsage) noexcept
{
    auto valid = true;

    if (valid)
    {
        // We don't yet support screen sizes in which the font is horizontally
        // or vertically compressed, so there is not much value in storing a
        // separate font for each of the screen sizes. However, we still need
        // to use these values to determine the cell size for which the font
        // was originally targeted, so we can resize it appropriately.
        switch (fontSet)
        {
        case DispatchTypes::DrcsFontSet::Default:
        case DispatchTypes::DrcsFontSet::Size80x24:
            _columnsPerPage = 80;
            _linesPerPage = 24;
            break;
        case DispatchTypes::DrcsFontSet::Size80x36:
            _columnsPerPage = 80;
            _linesPerPage = 36;
            break;
        case DispatchTypes::DrcsFontSet::Size80x48:
            _columnsPerPage = 80;
            _linesPerPage = 48;
            break;
        case DispatchTypes::DrcsFontSet::Size132x24:
            _columnsPerPage = 132;
            _linesPerPage = 24;
            break;
        case DispatchTypes::DrcsFontSet::Size132x36:
            _columnsPerPage = 132;
            _linesPerPage = 36;
            break;
        case DispatchTypes::DrcsFontSet::Size132x48:
            _columnsPerPage = 132;
            _linesPerPage = 48;
            break;
        default:
            valid = false;
            break;
        }
    }

    if (valid)
    {
        switch (fontUsage)
        {
        case DispatchTypes::DrcsFontUsage::Default:
        case DispatchTypes::DrcsFontUsage::Text:
            _isTextFont = true;
            break;
        case DispatchTypes::DrcsFontUsage::FullCell:
            _isTextFont = false;
            break;
        default:
            valid = false;
            break;
        }
    }

    if (valid)
    {
        switch (cellMatrix)
        {
        case DispatchTypes::DrcsCellMatrix::Invalid:
            valid = false;
            break;
        case DispatchTypes::DrcsCellMatrix::Size5x10:
            // Size 5x10 is only valid for text fonts.
            valid = _isTextFont;
            _sizeDeclaredAsMatrix = true;
            _declaredWidth = 5;
            _declaredHeight = 10;
            break;
        case DispatchTypes::DrcsCellMatrix::Size6x10:
            // Size 6x10 is only valid for text fonts,
            // unless it's a VT240 in 132-column mode.
            valid = _isTextFont || _columnsPerPage == 132;
            _sizeDeclaredAsMatrix = true;
            _declaredWidth = 6;
            _declaredHeight = 10;
            break;
        case DispatchTypes::DrcsCellMatrix::Size7x10:
            // Size 7x10 is only valid for text fonts.
            valid = _isTextFont;
            _sizeDeclaredAsMatrix = true;
            _declaredWidth = 7;
            _declaredHeight = 10;
            break;
        case DispatchTypes::DrcsCellMatrix::Default:
        default:
            // If we aren't given one of the predefined matrix sizes, then the
            // matrix parameter is a pixel width, and height is obtained from the
            // height parameter. This also applies for the default of 0, since a
            // 0 width is treated as unknown (we'll try and estimate the expected
            // width), and the height parameter can still give us the height.
            _sizeDeclaredAsMatrix = false;
            _declaredWidth = static_cast<VTInt>(cellMatrix);
            _declaredHeight = cellHeight.value_or(0);
            valid = (_declaredWidth <= MAX_WIDTH && _declaredHeight <= MAX_HEIGHT);
            break;
        }
    }

    // Save the pending attributes, but don't update the current values until we
    // are sure we have a valid sequence that can replace the current buffer.
    _pendingCellMatrix = cellMatrix;
    _pendingCellHeight = cellHeight.value_or(0);
    _pendingFontSet = fontSet;
    _pendingFontUsage = fontUsage;

    // Reset the used dimensions. These values will be determined by the extent
    // of the sixel data that we receive in the following string sequence.
    _usedWidth = 0;
    _usedHeight = 0;

    return valid;
}

bool FontBuffer::SetStartChar(const VTParameter startChar,
                              const DispatchTypes::DrcsCharsetSize charsetSize) noexcept
{
    switch (charsetSize)
    {
    case DispatchTypes::DrcsCharsetSize::Size94:
        _startChar = startChar.value_or(1);
        break;
    case DispatchTypes::DrcsCharsetSize::Size96:
        _startChar = startChar.value_or(0);
        break;
    default:
        return false;
    }

    _currentChar = _startChar;
    _pendingCharsetSize = charsetSize;
    _charsetIdInitialized = false;
    _charsetIdBuilder.Clear();

    return true;
}

void FontBuffer::AddSixelData(const wchar_t ch)
{
    if (!_charsetIdInitialized)
    {
        _buildCharsetId(ch);
    }
    else if (ch >= L'?' && ch <= L'~')
    {
        _addSixelValue(ch - L'?');
    }
    else if (ch == L'/')
    {
        _endOfSixelLine();
    }
    else if (ch == L';')
    {
        _endOfCharacter();
    }
}

bool FontBuffer::FinalizeSixelData()
{
    // If the charset ID hasn't been initialized this isn't a valid update.
    RETURN_BOOL_IF_FALSE(_charsetIdInitialized);

    // Flush the current line to make sure we take all the used positions
    // into account when calculating the font dimensions.
    _endOfSixelLine();

    // If the buffer has been cleared, we'll need to recalculate the dimensions
    // using the latest attributes, adjust the character bit patterns to fit
    // their true size, and fill in unused buffer positions with an error glyph.
    if (_bufferCleared)
    {
        std::tie(_fullWidth, _fullHeight, _textWidth) = _calculateDimensions();
        _packAndCenterBitPatterns();
        _fillUnusedCharacters();
    }

    return true;
}

std::span<const uint16_t> FontBuffer::GetBitPattern() const noexcept
{
    return { _buffer.data(), gsl::narrow_cast<size_t>(MAX_CHARS * _fullHeight) };
}

til::size FontBuffer::GetCellSize() const noexcept
{
    return { _fullWidth, _fullHeight };
}

size_t FontBuffer::GetTextCenteringHint() const noexcept
{
    return gsl::narrow_cast<size_t>(_textCenteringHint);
}

VTID FontBuffer::GetDesignation() const noexcept
{
    return _charsetId;
}

void FontBuffer::_buildCharsetId(const wchar_t ch)
{
    // Note that we ignore any characters that are not valid in this state.
    if (ch >= 0x20 && ch <= 0x2F)
    {
        _charsetIdBuilder.AddIntermediate(ch);
    }
    else if (ch >= 0x30 && ch <= 0x7E)
    {
        _pendingCharsetId = _charsetIdBuilder.Finalize(ch);
        _charsetIdInitialized = true;
        _prepareCharacterBuffer();
    }
}

void FontBuffer::_prepareCharacterBuffer()
{
    // If any of the attributes have changed since the last time characters
    // were downloaded, the font dimensions will need to be recalculated, and
    // the buffer will need to be cleared. Otherwise we'll just be adding to
    // the existing font, assuming the current dimensions.
    if (_cellMatrix != _pendingCellMatrix ||
        _cellHeight != _pendingCellHeight ||
        _fontSet != _pendingFontSet ||
        _fontUsage != _pendingFontUsage ||
        _charsetSize != _pendingCharsetSize ||
        _charsetId != _pendingCharsetId)
    {
        // Replace the current attributes with the pending values.
        _cellMatrix = _pendingCellMatrix;
        _cellHeight = _pendingCellHeight;
        _fontSet = _pendingFontSet;
        _fontUsage = _pendingFontUsage;
        _charsetSize = _pendingCharsetSize;
        _charsetId = _pendingCharsetId;

        // Reset the font dimensions to the maximum supported size, since we
        // can't be certain of the intended size until we've received all of
        // the sixel data. These values will be recalculated once we can work
        // out the terminal type that the font was originally designed for.
        _fullWidth = MAX_WIDTH;
        _fullHeight = MAX_HEIGHT;
        _textWidth = MAX_WIDTH;
        _textOffset = 0;

        // Clear the buffer.
        _buffer.fill(0);
        _bufferCleared = true;
    }
    else
    {
        _bufferCleared = false;
    }

    _prepareNextCharacter();
}

void FontBuffer::_prepareNextCharacter()
{
    _lastChar = _currentChar;
    _currentCharBuffer = std::next(_buffer.begin(), gsl::narrow_cast<size_t>(_currentChar * _fullHeight));
    _sixelColumn = 0;
    _sixelRow = 0;

    // If the buffer hasn't been cleared, we'll need to clear each character
    // position individually, before adding any new sixel data.
    if (!_bufferCleared && _currentChar < MAX_CHARS)
    {
        std::fill_n(_currentCharBuffer, _fullHeight, uint16_t{ 0 });
    }
}

void FontBuffer::_addSixelValue(const VTInt value) noexcept
{
    if (_currentChar < MAX_CHARS && _sixelColumn < _textWidth)
    {
        // Each sixel updates six pixels of a single column, so we setup a bit
        // mask for the column we want to update, and then set that bit in each
        // row for which there is a corresponding "on" bit in the input value.
        const auto outputColumnBit = (0x8000 >> (_sixelColumn + _textOffset));
        auto outputIterator = _currentCharBuffer;
        auto inputValueMask = 1;
        for (VTInt i = 0; i < 6 && _sixelRow + i < _fullHeight; i++)
        {
            *outputIterator |= (value & inputValueMask) ? outputColumnBit : 0;
            ++outputIterator;
            inputValueMask <<= 1;
        }
    }
    _sixelColumn++;
}

void FontBuffer::_endOfSixelLine()
{
    // Move down six rows to the get to the next sixel position.
    std::advance(_currentCharBuffer, 6);
    _sixelRow += 6;

    // Keep track of the maximum width and height covered by the sixel data.
    _usedWidth = std::max(_usedWidth, _sixelColumn);
    _usedHeight = std::max(_usedHeight, _sixelRow);

    // Reset the column number to the start of the next line.
    _sixelColumn = 0;
}

void FontBuffer::_endOfCharacter()
{
    _endOfSixelLine();
    _currentChar++;
    _prepareNextCharacter();
}

std::tuple<VTInt, VTInt, VTInt> FontBuffer::_calculateDimensions() const
{
    // If the size is declared as a matrix, this is most likely a VT2xx font,
    // typically with a cell size of 10x10. However, in 132-column mode, the
    // VT240 has a cell size of 6x10, but that's only for widths of 6 or less.
    if (_sizeDeclaredAsMatrix)
    {
        if (_columnsPerPage == 132 && _declaredWidth <= 6)
        {
            // 6x10 cell with no clipping.
            return { 6, 10, 0 };
        }
        else
        {
            // 10x10 cell with text clipped to 8 pixels.
            return { 10, 10, 8 };
        }
    }

    // If we've been given explicit dimensions, and this is not a text font,
    // then we assume those dimensions are the exact cell size.
    if (_declaredWidth && _declaredHeight && !_isTextFont)
    {
        // Since this is not a text font, no clipping is required.
        return { _declaredWidth, _declaredHeight, 0 };
    }

    // For most of the cases that follow, a text font will be clipped within
    // the bounds of the declared width (if given). There are only a few cases
    // where we'll need to use a hard-coded text width, and that's when the
    // font appears to be targeting a VT2xx.
    const auto textWidth = _isTextFont ? _declaredWidth : 0;

    // If the lines per page isn't 24, this must be targeting a VT420 or VT5xx.
    // The cell width is 6 for 132 columns, and 10 for 80 columns.
    // The cell height is 8 for 48 lines and 10 for 36 lines.
    if (_linesPerPage != 24)
    {
        const auto cellWidth = _columnsPerPage == 132 ? 6 : 10;
        const auto cellHeight = _linesPerPage == 48 ? 8 : 10;
        return { cellWidth, cellHeight, textWidth };
    }

    // Now we're going to test whether the dimensions are in range for a number
    // of known terminals. We use the declared dimensions if given, otherwise
    // estimate the size from the used sixel values. If comparing a sixel-based
    // height, though, we need to round up the target cell height to account for
    // the fact that our used height will always be a multiple of six.
    const auto inRange = [=](const VTInt cellWidth, const VTInt cellHeight) {
        const auto sixelHeight = (cellHeight + 5) / 6 * 6;
        const auto heightInRange = _declaredHeight ? _declaredHeight <= cellHeight : _usedHeight <= sixelHeight;
        const auto widthInRange = _declaredWidth ? _declaredWidth <= cellWidth : _usedWidth <= cellWidth;
        return heightInRange && widthInRange;
    };

    // In the case of a VT2xx font, you could only use a matrix size (which
    // we've dealt with above), or a default size, so the tests below are only
    // applicable for a VT2xx when no explicit dimensions have been declared.
    const auto noDeclaredSize = _declaredWidth == 0 && _declaredHeight == 0;

    if (_columnsPerPage == 80)
    {
        if (inRange(8, 10) && noDeclaredSize)
        {
            // VT2xx - 10x10 cell with text clipped to 8 pixels.
            return { 10, 10, 8 };
        }
        else if (inRange(15, 12))
        {
            // VT320 - 15x12 cell with default text width.
            return { 15, 12, textWidth };
        }
        else if (inRange(10, 16))
        {
            // VT420 & VT5xx - 10x16 cell with default text width.
            return { 10, 16, textWidth };
        }
        else if (inRange(10, 20))
        {
            // VT340 - 10x20 cell with default text width.
            return { 10, 20, textWidth };
        }
        else if (inRange(12, 30))
        {
            // VT382 - 12x30 cell with default text width.
            return { 12, 30, textWidth };
        }
        else
        {
            // If all else fails, assume the maximum size.
            return { MAX_WIDTH, MAX_HEIGHT, textWidth };
        }
    }
    else
    {
        if (inRange(6, 10) && noDeclaredSize)
        {
            // VT240 - 6x10 cell with no clipping.
            return { 6, 10, 0 };
        }
        else if (inRange(9, 12))
        {
            // VT320 - 9x12 cell with default text width.
            return { 9, 12, textWidth };
        }
        else if (inRange(6, 16))
        {
            // VT420 & VT5xx - 6x16 cell with default text width.
            return { 6, 16, textWidth };
        }
        else if (inRange(6, 20))
        {
            // VT340 - 6x20 cell with default text width.
            return { 6, 20, textWidth };
        }
        else if (inRange(7, 30))
        {
            // VT382 - 7x30 cell with default text width.
            return { 7, 30, textWidth };
        }
        else
        {
            // If all else fails, assume the maximum size.
            return { MAX_WIDTH, MAX_HEIGHT, textWidth };
        }
    }
}

void FontBuffer::_packAndCenterBitPatterns() noexcept
{
    // If this is a text font, we'll clip the bits up to the text width and
    // center them within the full cell width. For a full cell font we'll just
    // use all of the bits, and no offset will be required.
    _textWidth = _textWidth ? _textWidth : _fullWidth;
    _textWidth = std::min(_textWidth, _fullWidth);
    _textOffset = (_fullWidth - _textWidth) / 2;
    const auto textClippingMask = ~(0xFFFF >> _textWidth);

    // If the text is given an explicit width, we check to what extent the
    // content is offset from center. Knowing that information will enable the
    // renderer to scale the font more symmetrically.
    _textCenteringHint = _declaredWidth ? _fullWidth - (_declaredWidth + _textOffset * 2) : 0;

    // Initially the characters are written to the buffer assuming the maximum
    // cell height, but now that we know the true height, we need to pack the
    // buffer data so that each character occupies the exact number of scanlines
    // that are required.
    for (size_t srcLine = 0, dstLine = 0; srcLine < _buffer.size(); srcLine++)
    {
        if (gsl::narrow_cast<VTInt>(srcLine % MAX_HEIGHT) < _fullHeight)
        {
            auto characterScanline = til::at(_buffer, srcLine);
            characterScanline &= textClippingMask;
            characterScanline >>= _textOffset;
            til::at(_buffer, dstLine++) = characterScanline;
        }
    }
}

void FontBuffer::_fillUnusedCharacters()
{
    // Every character in the buffer that hasn't been uploaded will be replaced
    // with an error glyph (a reverse question mark). This includes every
    // character prior to the start char, or after the last char.
    const auto errorPattern = _generateErrorGlyph();
    for (VTInt ch = 0; ch < MAX_CHARS; ch++)
    {
        if (ch < _startChar || ch > _lastChar)
        {
            auto charBuffer = std::next(_buffer.begin(), gsl::narrow_cast<size_t>(ch * _fullHeight));
            std::copy_n(errorPattern.begin(), _fullHeight, charBuffer);
        }
    }
}

std::array<uint16_t, FontBuffer::MAX_HEIGHT> FontBuffer::_generateErrorGlyph()
{
    // We start with a bit pattern for a reverse question mark covering the
    // maximum font resolution that we might need.
    constexpr std::array<uint16_t, MAX_HEIGHT> inputBitPattern = {
        0b000000000000000,
        0b000000000000000,
        0b000000000000000,
        0b000000000000000,
        0b000000000000000,
        0b000000000000000,
        0b001111111110000,
        0b011111111111000,
        0b111000000011100,
        0b111000000011100,
        0b111000000000000,
        0b111000000000000,
        0b111100000000000,
        0b011111000000000,
        0b000011110000000,
        0b000001110000000,
        0b000001110000000,
        0b000001110000000,
        0b000001110000000,
        0b000000000000000,
        0b000001110000000,
        0b000001110000000,
        0b000001110000000,
    };

    // Then for each possible width and height, we have hard-coded bit masks
    // indicating a range of columns and rows to select from the base bitmap
    // to produce a scaled down version of reasonable quality.
    constexpr std::array<uint32_t, MAX_WIDTH + 1> widthMasks = {
        // clang-format off
        0, 1, 3, 32771, 8457, 9481, 9545, 9673, 42441, 26061,
        58829, 28141, 60909, 63453, 63485, 65533, 65535
        // clang-format on
    };
    constexpr std::array<uint32_t, MAX_HEIGHT + 1> heightMasks = {
        // clang-format off
        0, 1, 3, 7, 15, 1613952, 10002560, 10002816, 10068352, 10068353,
        26845569, 26847617, 26847619, 26864003, 28961155, 28961219,
        28961731, 62516163, 62516167, 129625031, 129625039, 129756111,
        263973839, 263974863, 268169167, 536604623, 536608719, 536608735,
        536870879, 1073741791, 2147483615, 2147483647, 4294967295
        // clang-format on
    };

    const auto widthMask = widthMasks.at(_fullWidth);
    const auto heightMask = heightMasks.at(_fullHeight);

    auto outputBitPattern = std::array<uint16_t, MAX_HEIGHT>{};
    auto outputIterator = outputBitPattern.begin();
    for (auto y = 0; y < MAX_HEIGHT; y++)
    {
        const auto yBit = (1 << y);
        if (heightMask & yBit)
        {
            const auto inputScanline = til::at(inputBitPattern, y);
            uint16_t outputScanline = 0;
            for (auto x = MAX_WIDTH; x-- > 0;)
            {
                const auto xBit = 1 << x;
                if (widthMask & xBit)
                {
                    outputScanline <<= 1;
                    outputScanline |= (inputScanline & xBit) ? 1 : 0;
                }
            }
            outputScanline <<= (MAX_WIDTH - _fullWidth);
            *(outputIterator++) = outputScanline;
        }
    }
    return outputBitPattern;
}
