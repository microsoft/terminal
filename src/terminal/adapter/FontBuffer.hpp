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
        til::size GetCellSize() const;
        size_t GetTextCenteringHint() const noexcept;
        VTID GetDesignation() const noexcept;

    private:
        static constexpr size_t MAX_WIDTH = 16;
        static constexpr size_t MAX_HEIGHT = 32;
        static constexpr size_t MAX_CHARS = 96;

        void _buildCharsetId(const wchar_t ch);
        void _prepareCharacterBuffer();
        void _prepareNextCharacter();
        void _addSixelValue(const size_t value) noexcept;
        void _endOfSixelLine();
        void _endOfCharacter();

        std::tuple<size_t, size_t, size_t> _calculateDimensions() const;
        void _packAndCenterBitPatterns();
        void _fillUnusedCharacters();
        std::array<uint16_t, MAX_HEIGHT> _generateErrorGlyph();

        DispatchTypes::DrcsCellMatrix _cellMatrix;
        DispatchTypes::DrcsCellMatrix _pendingCellMatrix;
        size_t _cellHeight;
        size_t _pendingCellHeight;
        bool _sizeDeclaredAsMatrix;
        size_t _declaredWidth;
        size_t _declaredHeight;
        size_t _usedWidth;
        size_t _usedHeight;
        size_t _fullWidth;
        size_t _fullHeight;
        size_t _textWidth;
        size_t _textOffset;
        size_t _textCenteringHint;

        DispatchTypes::DrcsFontSet _fontSet;
        DispatchTypes::DrcsFontSet _pendingFontSet;
        DispatchTypes::DrcsFontUsage _fontUsage;
        DispatchTypes::DrcsFontUsage _pendingFontUsage;
        size_t _linesPerPage;
        size_t _columnsPerPage;
        bool _isTextFont;

        DispatchTypes::DrcsCharsetSize _charsetSize;
        DispatchTypes::DrcsCharsetSize _pendingCharsetSize;
        VTID _charsetId{ 0 };
        VTID _pendingCharsetId{ 0 };
        bool _charsetIdInitialized;
        VTIDBuilder _charsetIdBuilder;
        size_t _startChar;
        size_t _lastChar;
        size_t _currentChar;

        using buffer_type = std::array<uint16_t, MAX_HEIGHT * MAX_CHARS>;
        buffer_type _buffer;
        buffer_type::iterator _currentCharBuffer;
        bool _bufferCleared;
        size_t _sixelColumn;
        size_t _sixelRow;
    };
}
