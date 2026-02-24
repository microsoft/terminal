// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/FontResource.hpp"

using namespace Microsoft::Console::Render;

namespace
{
    // The structures below are based on the Windows 3.0 font file format, which
    // was documented in Microsoft Knowledge Base article Q65123. Although no
    // longer hosted by Microsoft, it can still be found at the following URL:
    // https://web.archive.org/web/20140820153410/http://support.microsoft.com/kb/65123

    // For now we're only using fixed pitch single color fonts, but the rest
    // of the flags are included here for completeness.
    static constexpr DWORD DFF_FIXED = 0x0001;
    static constexpr DWORD DFF_PROPORTIONAL = 0x0002;
    static constexpr DWORD DFF_1COLOR = 0x0010;
    static constexpr DWORD DFF_16COLOR = 0x0020;
    static constexpr DWORD DFF_256COLOR = 0x0040;
    static constexpr DWORD DFF_RGBCOLOR = 0x0080;

    // DRCS soft fonts only require 96 characters at most.
    static constexpr size_t CHAR_COUNT = 96;

#pragma pack(push, 1)
    struct GLYPHENTRY
    {
        WORD geWidth;
        DWORD geOffset;
    };

    struct FONTINFO
    {
        WORD dfVersion;
        DWORD dfSize;
        CHAR dfCopyright[60];
        WORD dfType;
        WORD dfPoints;
        WORD dfVertRes;
        WORD dfHorizRes;
        WORD dfAscent;
        WORD dfInternalLeading;
        WORD dfExternalLeading;
        BYTE dfItalic;
        BYTE dfUnderline;
        BYTE dfStrikeOut;
        WORD dfWeight;
        BYTE dfCharSet;
        WORD dfPixWidth;
        WORD dfPixHeight;
        BYTE dfPitchAndFamily;
        WORD dfAvgWidth;
        WORD dfMaxWidth;
        BYTE dfFirstChar;
        BYTE dfLastChar;
        BYTE dfDefaultChar;
        BYTE dfBreakChar;
        WORD dfWidthBytes;
        DWORD dfDevice;
        DWORD dfFace;
        DWORD dfBitsPointer;
        DWORD dfBitsOffset;
        BYTE dfReserved;
        DWORD dfFlags;
        WORD dfAspace;
        WORD dfBspace;
        WORD dfCspace;
        DWORD dfColorPointer;
        DWORD dfReserved1[4];
        GLYPHENTRY dfCharTable[CHAR_COUNT];
        CHAR szFaceName[LF_FACESIZE];
    };
#pragma pack(pop)
}

FontResource::FontResource(const std::span<const uint16_t> bitPattern,
                           const til::size sourceSize,
                           const til::size targetSize,
                           const size_t centeringHint) :
    _bitPattern{ bitPattern.begin(), bitPattern.end() },
    _sourceSize{ sourceSize },
    _targetSize{ targetSize },
    _centeringHint{ centeringHint }
{
}

void FontResource::SetTargetSize(const til::size targetSize)
{
    if (_targetSize != targetSize)
    {
        _targetSize = targetSize;
        _fontHandle = nullptr;
    }
}

FontResource::operator HFONT()
{
    if (!_fontHandle && !_bitPattern.empty())
    {
        _regenerateFont();
    }
    return _fontHandle.get();
}

void FontResource::_regenerateFont()
{
    const auto targetWidth = _targetSize.narrow_width<WORD>();
    const auto targetHeight = _targetSize.narrow_height<WORD>();
    const auto charSizeInBytes = (targetWidth + 7) / 8 * targetHeight;

    const DWORD fontBitmapSize = charSizeInBytes * CHAR_COUNT;
    const DWORD fontResourceSize = sizeof(FONTINFO) + fontBitmapSize;

    auto fontResourceBuffer = std::vector<byte>(fontResourceSize);
    void* fontResourceBufferPointer = fontResourceBuffer.data();
    auto& fontResource = *static_cast<FONTINFO*>(fontResourceBufferPointer);

    fontResource.dfVersion = 0x300;
    fontResource.dfSize = fontResourceSize;
    fontResource.dfWeight = FW_NORMAL;
    fontResource.dfCharSet = OEM_CHARSET;
    fontResource.dfPixWidth = targetWidth;
    fontResource.dfPixHeight = targetHeight;
    fontResource.dfPitchAndFamily = FIXED_PITCH | FF_DONTCARE;
    fontResource.dfAvgWidth = targetWidth;
    fontResource.dfMaxWidth = targetWidth;
    fontResource.dfFirstChar = L' ';
    fontResource.dfLastChar = fontResource.dfFirstChar + CHAR_COUNT - 1;
    fontResource.dfFace = offsetof(FONTINFO, szFaceName);
    fontResource.dfBitsOffset = sizeof(FONTINFO);
    fontResource.dfFlags = DFF_FIXED | DFF_1COLOR;

    // We use an atomic counter to create a locally-unique name for the font.
    static std::atomic<uint64_t> faceNameCounter;
    sprintf_s(fontResource.szFaceName, "WTSOFTFONT%016llX", faceNameCounter++);

    // Each character has a fixed size and position in the font bitmap, but we
    // still need to fill in the header table with that information.
    for (auto i = 0u; i < std::size(fontResource.dfCharTable); i++)
    {
        const auto charOffset = fontResource.dfBitsOffset + charSizeInBytes * i;
        fontResource.dfCharTable[i].geOffset = charOffset;
        fontResource.dfCharTable[i].geWidth = targetWidth;
    }

    // Raster fonts aren't generally scalable, so we need to resize the bit
    // patterns for the character glyphs to the requested target size, and
    // copy the results into the resource structure.
    auto fontResourceSpan = std::span<byte>(fontResourceBuffer);
    _resizeBitPattern(fontResourceSpan.subspan(fontResource.dfBitsOffset));

    DWORD fontCount = 0;
    _resourceHandle.reset(AddFontMemResourceEx(&fontResource, fontResourceSize, nullptr, &fontCount));
    LOG_HR_IF_NULL(E_FAIL, _resourceHandle.get());

    // Once the resource has been registered, we should be able to create the
    // font by using the same name and attributes as were set in the resource.
    LOGFONTA logFont = {};
    logFont.lfHeight = fontResource.dfPixHeight;
    logFont.lfWidth = fontResource.dfPixWidth;
    logFont.lfCharSet = fontResource.dfCharSet;
    logFont.lfOutPrecision = OUT_RASTER_PRECIS;
    logFont.lfPitchAndFamily = fontResource.dfPitchAndFamily;
    strcpy_s(logFont.lfFaceName, fontResource.szFaceName);
    _fontHandle.reset(CreateFontIndirectA(&logFont));
    LOG_HR_IF_NULL(E_FAIL, _fontHandle.get());
}

void FontResource::_resizeBitPattern(std::span<byte> targetBuffer)
{
    auto sourceWidth = _sourceSize.width;
    auto targetWidth = _targetSize.width;
    const auto sourceHeight = _sourceSize.height;
    const auto targetHeight = _targetSize.height;

    // If the text in the font is not perfectly centered, the _centeringHint
    // gives us the offset needed to correct that misalignment. So to ensure
    // that any inserted or deleted columns are evenly spaced around the center
    // point of the glyphs, we need to adjust the source and target widths by
    // that amount (proportionally) before calculating the scaling increments.
    targetWidth -= std::lround((double)_centeringHint * targetWidth / sourceWidth);
    sourceWidth -= gsl::narrow_cast<int>(_centeringHint);

    // The way the scaling works is by iterating over the target range, and
    // calculating the source offsets that correspond to each target position.
    // We achieve that by incrementing the source offset every iteration by an
    // integer value that is the quotient of the source and target dimensions.
    // Because this is an integer division, we're going to be off by a certain
    // fraction on each iteration, so we need to keep track of that accumulated
    // error using the modulus of the division. Once the error total exceeds
    // the target dimension (more or less), we add another pixel to compensate
    // for the error, and reset the error total.
    const auto createIncrementFunction = [](const auto sourceDimension, const auto targetDimension) {
        const auto increment = sourceDimension / targetDimension;
        const auto errorIncrement = sourceDimension % targetDimension * 2;
        const auto errorThreshold = targetDimension * 2 - std::min(sourceDimension, targetDimension);
        const auto errorReset = targetDimension * 2;

        return [=](auto& errorTotal) {
            errorTotal += errorIncrement;
            if (errorTotal > errorThreshold)
            {
                errorTotal -= errorReset;
                return increment + 1;
            }
            return increment;
        };
    };
    const auto columnIncrement = createIncrementFunction(sourceWidth, targetWidth);
    const auto lineIncrement = createIncrementFunction(sourceHeight, targetHeight);

    // Once we've calculated the scaling increments, taking the centering hint
    // into account, we reset the target width back to its original value.
    targetWidth = _targetSize.width;

    auto targetBufferPointer = targetBuffer.begin();
    for (auto ch = 0; ch < CHAR_COUNT; ch++)
    {
        // Bits are read from the source from left to right - MSB to LSB. The source
        // column is a single bit representing the 1-based position. The reason for
        // this will become clear in the mask calculation below.
        auto sourceColumn = 1 << 16;
        auto sourceColumnError = 0;

        // The target format expects the character bitmaps to be laid out in columns
        // of 8 bits. So we generate 8 bits from each scanline until we've covered
        // the full target height. Then we start again from the top with the next 8
        // bits of the line, until we've covered the full target width.
        for (auto targetX = 0; targetX < targetWidth; targetX += 8)
        {
            auto sourceLine = std::next(_bitPattern.begin(), ch * sourceHeight);
            auto sourceLineError = 0;

            // Since we're going to be reading from the same horizontal offset for each
            // target line, we save the state here so we can reset it every iteration.
            const auto initialSourceColumn = sourceColumn;
            const auto initialSourceColumnError = sourceColumnError;

            for (auto targetY = 0; targetY < targetHeight; targetY++)
            {
                sourceColumn = initialSourceColumn;
                sourceColumnError = initialSourceColumnError;

                // For a particular target line, we calculate the span of source lines from
                // which it is derived, then OR those values together. We don't want the
                // source value to be zero, though, so we must read at least one line.
                const auto lineSpan = lineIncrement(sourceLineError);
                auto sourceValue = 0;
                for (auto i = 0; i < std::max(lineSpan, 1); i++)
                {
                    sourceValue |= sourceLine[i];
                }
                std::advance(sourceLine, lineSpan);

                // From the combined value of the source lines, we now need to extract eight
                // bits to make up the next byte in the target at the current X offset.
                byte targetValue = 0;
                for (auto targetBit = 0; targetBit < 8; targetBit++)
                {
                    targetValue <<= 1;
                    if (targetX + targetBit < targetWidth)
                    {
                        // As with the line iteration, we first need to calculate the span of source
                        // columns from which the target bit is derived. We shift our source column
                        // position right by that amount to determine the next column position, then
                        // subtract those two values to obtain a mask. For example, if we're reading
                        // from columns 6 to 3 (exclusively), the initial column position is 1<<6,
                        // the next column position is 1<<3, so the mask is 64-8=56, or 00111000.
                        // Again we don't want this mask to be zero, so if the span is zero, we need
                        // to shift an additional bit to make sure we cover at least one column.
                        const auto columnSpan = columnIncrement(sourceColumnError);
                        const auto nextSourceColumn = sourceColumn >> columnSpan;
                        const auto sourceMask = sourceColumn - (nextSourceColumn >> (columnSpan ? 0 : 1));
                        sourceColumn = nextSourceColumn;
                        targetValue |= (sourceValue & sourceMask) ? 1 : 0;
                    }
                }
                *(targetBufferPointer++) = targetValue;
            }
        }
    }
}
