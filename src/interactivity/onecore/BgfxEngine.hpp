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

#include "../../renderer/inc/RenderEngineBase.hpp"

namespace Microsoft::Console::Render
{
    class BgfxEngine final : public RenderEngineBase
    {
    public:
        BgfxEngine(PVOID SharedViewBase, LONG DisplayHeight, LONG DisplayWidth, LONG FontWidth, LONG FontHeight) noexcept;

        // IRenderEngine Members
        [[nodiscard]] HRESULT Invalidate(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateCursor(const til::rect* psrRegion) noexcept override;
        [[nodiscard]] HRESULT InvalidateSystem(const til::rect* prcDirtyClient) noexcept override;
        [[nodiscard]] HRESULT InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept override;
        [[nodiscard]] HRESULT InvalidateScroll(const til::point* pcoordDelta) noexcept override;
        [[nodiscard]] HRESULT InvalidateAll() noexcept override;
        [[nodiscard]] HRESULT PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept override;

        [[nodiscard]] HRESULT StartPaint() noexcept override;
        [[nodiscard]] HRESULT EndPaint() noexcept override;
        [[nodiscard]] HRESULT Present() noexcept override;

        [[nodiscard]] HRESULT ScrollFrame() noexcept override;

        [[nodiscard]] HRESULT PaintBackground() noexcept override;
        [[nodiscard]] HRESULT PaintBufferLine(const std::span<const Cluster> clusters,
                                              const til::point coord,
                                              const bool trimLeft,
                                              const bool lineWrapped) noexcept override;
        [[nodiscard]] HRESULT PaintBufferGridLines(GridLineSet const lines, COLORREF const color, size_t const cchLine, til::point const coordTarget) noexcept override;
        [[nodiscard]] HRESULT PaintSelection(const til::rect& rect) noexcept override;

        [[nodiscard]] HRESULT PaintCursor(const CursorOptions& options) noexcept override;

        [[nodiscard]] HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                   const RenderSettings& renderSettings,
                                                   const gsl::not_null<IRenderData*> pData,
                                                   const bool usingSoftFont,
                                                   const bool isSettingDefaultBrushes) noexcept override;
        [[nodiscard]] HRESULT UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept override;
        [[nodiscard]] HRESULT UpdateDpi(const int iDpi) noexcept override;
        [[nodiscard]] HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept override;

        [[nodiscard]] HRESULT GetProposedFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo, const int iDpi) noexcept override;

        [[nodiscard]] HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept override;
        [[nodiscard]] HRESULT GetFontSize(_Out_ til::size* pFontSize) noexcept override;
        [[nodiscard]] HRESULT IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept override;

    protected:
        [[nodiscard]] HRESULT _DoUpdateTitle(_In_ const std::wstring_view newTitle) noexcept override;

    private:
        std::byte* _sharedViewBase;
        SIZE_T _runLength;

        SIZE_T _displayHeight;
        SIZE_T _displayWidth;
        til::rect _dirtyArea;

        til::size _fontSize;

        WORD _currentLegacyColorAttribute;
    };
}
