/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- BgfxEngine.hpp

Abstract:
- OneCore implementation of the IRenderEngine interface.

Author(s):
- Hernan Gatta (HeGatta) 29-Mar-2017
--*/

// Typically, renderers live under the renderer/xxx top-level folder. This
// renderer however has strong ties to the interactivity library. More
// specifically, it makes use of the Console IO Server communication class.
// It is also a one-file renderer and I had problems with header file
// definitions. Placing this renderer in the OneCore Interactivity library fixes
// the header issues, and is more sensible given its ties to ConIoSrv.
// (hegatta, 2017)

#pragma once

#include "..\..\renderer\inc\RenderEngineBase.hpp"

namespace Microsoft::Console::Render
{
    class BgfxEngine final : public RenderEngineBase
    {
    public:
        BgfxEngine(PVOID SharedViewBase, LONG DisplayHeight, LONG DisplayWidth, LONG FontWidth, LONG FontHeight);
        ~BgfxEngine() override = default;

        // IRenderEngine Members
        [[nodiscard]] HRESULT Invalidate(const SMALL_RECT* const psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const COORD* const pcoordCursor) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const RECT* const prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const COORD* const pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT InvalidateCircling(_Out_ bool* const pForcePaint) noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(const std::basic_string_view<Cluster> clusters,
                                              const COORD coord,
                                              const bool trimLeft) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(GridLines const lines, COLORREF const color, size_t const cchLine, COORD const coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const SMALL_RECT rect) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(COLORREF const colorForeground,
                                                   COLORREF const colorBackground,
                                                   const WORD legacyColorAttribute,
                                                   const ExtendedAttributes extendedAttrs,
                                                   bool const isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(int const iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const SMALL_RECT srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo, int const iDpi) noexcept override;

        SMALL_RECT GetDirtyRectInChars() override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ COORD* const pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(_In_ const std::wstring& newTitle) noexcept override;

    private:
        ULONG_PTR _sharedViewBase;
        SIZE_T _runLength;

        LONG _displayHeight;
        LONG _displayWidth;

        COORD _fontSize;

        WORD _currentLegacyColorAttribute;
    };
}
