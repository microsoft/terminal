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

        std::span<const uint16_t> GetBitPattern() const noexcept;
        til::size GetCellSize() const noexcept;
        size_t GetTextCenteringHint() const noexcept;
        VTID GetDesignation() const noexcept;

    private:
        static constexpr VTInt MAX_WIDTH = 16;
        static constexpr VTInt MAX_HEIGHT = 32;
        static constexpr VTInt MAX_CHARS = 96;

        void _buildCharsetId(const wchar_t ch);
        void _prepareCharacterBuffer();
        void _prepareNextCharacter();
        void _addSixelValue(const VTInt value) noexcept;
        void _endOfSixelLine();
        void _endOfCharacter();

        std::tuple<VTInt, VTInt, VTInt> _calculateDimensions() const;
        void _packAndCenterBitPatterns() noexcept;
        void _fillUnusedCharacters();
        std::array<uint16_t, MAX_HEIGHT> _generateErrorGlyph();

        DispatchTypes::DrcsCellMatrix _cellMatrix;
        DispatchTypes::DrcsCellMatrix _pendingCellMatrix;
        VTInt _cellHeight;
        VTInt _pendingCellHeight;
        bool _sizeDeclaredAsMatrix;
        VTInt _declaredWidth;
        VTInt _declaredHeight;
        VTInt _usedWidth;
        VTInt _usedHeight;
        VTInt _fullWidth;
        VTInt _fullHeight;
        VTInt _textWidth;
        VTInt _textOffset;
        size_t _textCenteringHint;

        DispatchTypes::DrcsFontSet _fontSet;
        DispatchTypes::DrcsFontSet _pendingFontSet;
        DispatchTypes::DrcsFontUsage _fontUsage;
        DispatchTypes::DrcsFontUsage _pendingFontUsage;
        VTInt _linesPerPage;
        VTInt _columnsPerPage;
        bool _isTextFont;

        DispatchTypes::DrcsCharsetSize _charsetSize;
        DispatchTypes::DrcsCharsetSize _pendingCharsetSize;
        VTID _charsetId{ 0 };
        VTID _pendingCharsetId{ 0 };
        bool _charsetIdInitialized;
        VTIDBuilder _charsetIdBuilder;
        VTInt _startChar;
        VTInt _lastChar;
        VTInt _currentChar;

        using buffer_type = std::array<uint16_t, MAX_HEIGHT * MAX_CHARS>;
        buffer_type _buffer;
        buffer_type::iterator _currentCharBuffer;
        bool _bufferCleared;
        VTInt _sixelColumn;
        VTInt _sixelRow;
    };
}
