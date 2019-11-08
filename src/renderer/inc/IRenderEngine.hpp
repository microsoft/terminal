/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IRenderEngine.hpp

Abstract:
- This serves as the entry point for a specific graphics engine specific renderer.

Author(s):
- Michael Niksa (MiNiksa) 17-Nov-2015
--*/

#pragma once

#include "../../inc/conattrs.hpp"
#include "Cluster.hpp"
#include "FontInfoDesired.hpp"

namespace Microsoft::Console::Render
{
    class IRenderEngine
    {
    public:
        enum GridLines
        {
            None = 0x0,
            Top = 0x1,
            Bottom = 0x2,
            Left = 0x4,
            Right = 0x8
        };

        struct CursorOptions
        {
            // Character cell in the grid to draw at
            // This is relative to the viewport, not the buffer.
            COORD coordCursor;

            // For an underscore type _ cursor, how tall it should be as a % of cell height
            ULONG ulCursorHeightPercent;

            // For a vertical bar type | cursor, how many pixels wide it should be per ease of access preferences
            ULONG cursorPixelWidth;

            // Whether to draw the cursor 2 cells wide (+X from the coordinate given)
            bool fIsDoubleWidth;

            // Chooses a special cursor type like a full box, a vertical bar, etc.
            CursorType cursorType;

            // Specifies to use the color below instead of the default color
            bool fUseColor;

            // Color to use for drawing instead of the default
            COLORREF cursorColor;

            // Is the cursor currently visually visible?
            // If the cursor has blinked off, this is false.
            // if the cursor has blinked on, this is true.
            bool isOn;
        };

        virtual ~IRenderEngine() = 0;

    protected:
        IRenderEngine() = default;
        IRenderEngine(const IRenderEngine&) = default;
        IRenderEngine(IRenderEngine&&) = default;
        IRenderEngine& operator=(const IRenderEngine&) = default;
        IRenderEngine& operator=(IRenderEngine&&) = default;

    public:
        [[nodiscard]] virtual HRESULT StartPaint() noexcept = 0;
        [[nodiscard]] virtual HRESULT EndPaint() noexcept = 0;
        [[nodiscard]] virtual HRESULT Present() noexcept = 0;

        [[nodiscard]] virtual HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept = 0;

        [[nodiscard]] virtual HRESULT ScrollFrame() noexcept = 0;

        [[nodiscard]] virtual HRESULT Invalidate(const SMALL_RECT* const psrRegion) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateCursor(const COORD* const pcoordCursor) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateSystem(const RECT* const prcDirtyClient) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateAll() noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateCircling(_Out_ bool* const pForcePaint) noexcept = 0;

        [[nodiscard]] virtual HRESULT InvalidateTitle(const std::wstring& proposedTitle) noexcept = 0;

        [[nodiscard]] virtual HRESULT PaintBackground() noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                                      const COORD coord,
                                                      const bool fTrimLeft) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintBufferGridLines(const GridLines lines,
                                                           const COLORREF color,
                                                           const size_t cchLine,
                                                           const COORD coordTarget) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintSelection(const SMALL_RECT rect) noexcept = 0;

        [[nodiscard]] virtual HRESULT PaintCursor(const CursorOptions& options) noexcept = 0;

        [[nodiscard]] virtual HRESULT UpdateDrawingBrushes(const COLORREF colorForeground,
                                                           const COLORREF colorBackground,
                                                           const WORD legacyColorAttribute,
                                                           const ExtendedAttributes extendedAttrs,
                                                           const bool isSettingDefaultBrushes) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired,
                                                 _Out_ FontInfo& FontInfo) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateDpi(const int iDpi) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateViewport(const SMALL_RECT srNewViewport) noexcept = 0;

        [[nodiscard]] virtual HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired,
                                                      _Out_ FontInfo& FontInfo,
                                                      const int iDpi) noexcept = 0;

        virtual SMALL_RECT GetDirtyRectInChars() = 0;
        [[nodiscard]] virtual HRESULT GetFontSize(_Out_ COORD* const pFontSize) noexcept = 0;
        [[nodiscard]] virtual HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateTitle(const std::wstring& newTitle) noexcept = 0;
    };

    inline Microsoft::Console::Render::IRenderEngine::~IRenderEngine() {}
}

DEFINE_ENUM_FLAG_OPERATORS(Microsoft::Console::Render::IRenderEngine::GridLines)
