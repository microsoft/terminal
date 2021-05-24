/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SixelParser.hpp

Abstract:
- This class handles the parsing of the Sixel image format.
--*/

#pragma once

#include "til.h"
#include "DispatchTypes.hpp"

class Cursor;
class TextBuffer;

namespace Microsoft::Console::VirtualTerminal
{
    class AdaptDispatch;
    class Page;
    class StateMachine;

    class SixelParser
    {
    public:
        static constexpr VTInt DefaultConformance = 9;

        static til::size CellSizeForLevel(const VTInt conformanceLevel) noexcept;
        static size_t MaxColorsForLevel(const VTInt conformanceLevel) noexcept;

        SixelParser(AdaptDispatch& dispatcher, const StateMachine& stateMachine, const VTInt conformanceLevel = DefaultConformance) noexcept;
        void SoftReset();
        void SetDisplayMode(const bool enabled) noexcept;
        std::function<bool(wchar_t)> DefineImage(const VTInt macroParameter, const DispatchTypes::SixelBackground backgroundSelect, const VTParameter backgroundColor);

    private:
        // NB: If we want to support more than 256 colors, we'll also need to
        // change the IndexType to uint16_t, and use a bit field in IndexPixel
        // to retain the 16-bit size.
        static constexpr size_t MAX_COLORS = 256;
        using IndexType = uint8_t;
        struct IndexedPixel
        {
            uint8_t transparent = false;
            IndexType colorIndex = 0;
        };

        AdaptDispatch& _dispatcher;
        const StateMachine& _stateMachine;
        const VTInt _conformanceLevel;

        void _parseCommandChar(const wchar_t ch);
        void _parseParameterChar(const wchar_t ch);
        int _applyPendingCommand();
        void _executeCarriageReturn() noexcept;
        void _executeNextLine();
        void _executeMoveToHome();

        enum class States : size_t
        {
            Normal,
            Attributes,
            Color,
            Repeat
        };
        States _state = States::Normal;
        std::vector<VTParameter> _parameters;

        bool _initTextBufferBoundaries();
        void _initRasterAttributes(const VTInt macroParameter, const DispatchTypes::SixelBackground backgroundSelect) noexcept;
        void _updateRasterAttributes(const VTParameters& rasterAttributes);
        void _scrollTextBuffer(Page& page, const int scrollAmount);
        void _updateTextCursor(Cursor& cursor) noexcept;

        const til::size _cellSize;
        bool _displayMode = true;
        til::rect _textMargins;
        til::point _textCursor;
        bool _textCursorWasVisible;
        til::CoordType _availablePixelWidth;
        til::CoordType _availablePixelHeight;
        til::CoordType _pixelAspectRatio;
        til::CoordType _sixelHeight;
        til::CoordType _segmentHeight;
        til::CoordType _pendingTextScrollCount;
        til::size _backgroundSize;
        bool _backgroundFillRequired;

        void _initColorMap(const VTParameter backgroundColor);
        void _defineColor(const VTParameters& colorParameters);
        void _defineColor(const size_t colorNumber, const COLORREF color);
        COLORREF _colorFromIndex(const IndexType tableIndex) const noexcept;
        static constexpr RGBQUAD _makeRGBQUAD(const COLORREF color) noexcept;
        void _updateTextColors();

        std::array<size_t, MAX_COLORS> _colorMap = {};
        std::array<bool, MAX_COLORS> _colorMapUsed = {};
        std::array<COLORREF, MAX_COLORS> _colorTable = {};
        const size_t _maxColors;
        size_t _colorsUsed;
        size_t _colorsAvailable;
        bool _colorTableChanged;
        IndexedPixel _foregroundPixel;

        void _initImageBuffer();
        void _resizeImageBuffer(const til::CoordType requiredHeight);
        void _fillImageBackground();
        void _writeToImageBuffer(const int sixelValue, const int repeatCount);
        void _eraseImageBufferRows(const int rowCount, const til::CoordType startRow = 0) noexcept;
        void _maybeFlushImageBuffer(const bool endOfSequence = false);

        std::vector<IndexedPixel> _imageBuffer;
        til::point _imageOriginCell;
        til::point _imageCursor;
        til::CoordType _imageWidth;
        til::CoordType _imageMaxWidth;
        size_t _imageLineCount;
        size_t _lastFlushLine;
        std::chrono::steady_clock::time_point _lastFlushTime;
    };
}
