/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- FontBuffer.hpp

Abstract:
- This manages the construction and storage of font definitions for the VT DECDLD control sequence.
--*/

#pragma once

#include "DispatchTypes.hpp"

namespace Microsoft::Console::VirtualTerminal
{
    class FontBuffer
    {
    public:
        FontBuffer() noexcept;
        ~FontBuffer() = default;
        bool SetEraseControl(const DispatchTypes::DrcsEraseControl eraseControl) noexcept;
        bool SetAttributes(const DispatchTypes::DrcsCellMatrix cellMatrix,
                           const VTParameter cellHeight,
                           const DispatchTypes::DrcsFontSet fontSet,
                           const DispatchTypes::DrcsFontUsage fontUsage) noexcept;
        bool SetStartChar(const VTParameter startChar,
                          const DispatchTypes::DrcsCharsetSize charsetSize) noexcept;
        void AddSixelData(const wchar_t ch);
        bool FinalizeSixelData();

        gsl::span<const uint16_t> GetBitPattern() const noexcept;
        til::size GetCellSize() const noexcept;
        til::CoordType GetTextCenteringHint() const noexcept;
        VTID GetDesignation() const noexcept;

    private:
        static constexpr til::CoordType MAX_WIDTH = 16;
        static constexpr til::CoordType MAX_HEIGHT = 32;
        static constexpr size_t MAX_CHARS = 96;

        void _buildCharsetId(const wchar_t ch);
        void _prepareCharacterBuffer();
        void _prepareNextCharacter();
        void _addSixelValue(const til::CoordType value) noexcept;
        void _endOfSixelLine();
        void _endOfCharacter();

        std::tuple<til::CoordType, til::CoordType, til::CoordType> _calculateDimensions() const;
        void _packAndCenterBitPatterns();
        void _fillUnusedCharacters();
        std::array<uint16_t, MAX_HEIGHT> _generateErrorGlyph();

        DispatchTypes::DrcsCellMatrix _cellMatrix;
        DispatchTypes::DrcsCellMatrix _pendingCellMatrix;
        til::CoordType _cellHeight;
        til::CoordType _pendingCellHeight;
        bool _sizeDeclaredAsMatrix;
        til::CoordType _declaredWidth;
        til::CoordType _declaredHeight;
        til::CoordType _usedWidth;
        til::CoordType _usedHeight;
        til::CoordType _fullWidth;
        til::CoordType _fullHeight;
        til::CoordType _textWidth;
        til::CoordType _textOffset;
        til::CoordType _textCenteringHint;

        DispatchTypes::DrcsFontSet _fontSet;
        DispatchTypes::DrcsFontSet _pendingFontSet;
        DispatchTypes::DrcsFontUsage _fontUsage;
        DispatchTypes::DrcsFontUsage _pendingFontUsage;
        til::CoordType _linesPerPage;
        til::CoordType _columnsPerPage;
        bool _isTextFont;

        DispatchTypes::DrcsCharsetSize _charsetSize;
        DispatchTypes::DrcsCharsetSize _pendingCharsetSize;
        VTID _charsetId{ 0 };
        VTID _pendingCharsetId{ 0 };
        bool _charsetIdInitialized;
        VTIDBuilder _charsetIdBuilder;
        til::CoordType _startChar;
        til::CoordType _lastChar;
        til::CoordType _currentChar;

        using buffer_type = std::array<uint16_t, MAX_HEIGHT * MAX_CHARS>;
        buffer_type _buffer;
        buffer_type::iterator _currentCharBuffer;
        bool _bufferCleared;
        til::CoordType _sixelColumn;
        til::CoordType _sixelRow;
    };
}
