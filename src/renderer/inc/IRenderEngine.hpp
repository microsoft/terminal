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

#include <d2d1.h>

#include "CursorOptions.h"
#include "Cluster.hpp"
#include "FontInfoDesired.hpp"
#include "IRenderData.hpp"
#include "RenderSettings.hpp"
#include "../../buffer/out/LineRendition.hpp"
#include "../../buffer/out/GraphicsImage.hpp"
#include "../../buffer/out/ImageSlice.hpp"

#pragma warning(push)
#pragma warning(disable : 4100) // '...': unreferenced formal parameter
namespace Microsoft::Console::Render
{
    // A horizontal strip of a graphics placement, covering a single row.
    struct GraphicsPaintInfo
    {
        std::shared_ptr<const GraphicsImage> image;
        GraphicsGeometry geometry;
        // The buffer column of the placement's top-left cell.
        til::CoordType anchorColumn = 0;
        // The row of the placement that is being painted.
        til::CoordType rowIndex = 0;
        // The range of buffer columns the strip is clipped to.
        til::CoordType clipLeft = 0;
        til::CoordType clipRight = 0;
        til::CoordType targetRow = 0;
        til::CoordType viewportLeft = 0;
        int32_t z = 0;
        uint32_t order = 0;
        // Uniquely identifies the placement's appearance, for caching.
        uint64_t key = 0;
    };

    struct GraphicsStrip
    {
        // The part of the image to draw, in image pixels.
        GraphicsRect source;
        // Where to draw it, in pixels relative to the top-left corner of the target row.
        til::rect target;
    };

    // Computes the part of a placement that's visible in the target row, given the
    // actual cell size and the width of the viewport in cells. Returns false if
    // nothing is visible.
    inline bool ComputeGraphicsStrip(const GraphicsPaintInfo& info, const til::size cellSize, const til::CoordType viewportWidth, GraphicsStrip& strip) noexcept
    {
        const auto dest = info.geometry.Destination(cellSize);
        if (dest.right <= dest.left || dest.bottom <= dest.top)
        {
            return false;
        }

        // These coordinates are relative to the placement's top-left cell,
        // until they get translated to the target row at the very end.
        const auto cellWidth = static_cast<float>(cellSize.width);
        const auto cellHeight = static_cast<float>(cellSize.height);
        const auto originX = static_cast<float>(static_cast<int64_t>(info.anchorColumn) - info.viewportLeft) * cellWidth;
        const auto bandTop = static_cast<float>(info.rowIndex) * cellHeight;
        const auto clipLeft = std::max(static_cast<float>(static_cast<int64_t>(info.clipLeft) - info.anchorColumn) * cellWidth, -originX);
        const auto clipRight = std::min(static_cast<float>(static_cast<int64_t>(info.clipRight) - info.anchorColumn) * cellWidth, static_cast<float>(viewportWidth) * cellWidth - originX);
        const GraphicsRect visible{
            std::max(dest.left, clipLeft),
            std::max(dest.top, bandTop),
            std::min(dest.right, clipRight),
            std::min(dest.bottom, bandTop + cellHeight),
        };
        strip.target = {
            lroundf(visible.left + originX),
            lroundf(visible.top - bandTop),
            lroundf(visible.right + originX),
            lroundf(visible.bottom - bandTop),
        };
        if (strip.target.right <= strip.target.left || strip.target.bottom <= strip.target.top)
        {
            return false;
        }

        const auto& source = info.geometry.source;
        const auto scaleX = static_cast<float>(source.width()) / (dest.right - dest.left);
        const auto scaleY = static_cast<float>(source.height()) / (dest.bottom - dest.top);
        strip.source = {
            source.left + (visible.left - dest.left) * scaleX,
            source.top + (visible.top - dest.top) * scaleY,
            source.left + (visible.right - dest.left) * scaleX,
            source.top + (visible.bottom - dest.top) * scaleY,
        };
        return true;
    }

    struct RenderFrameInfo
    {
        std::span<const til::point_span> searchHighlights;
        const til::point_span* searchHighlightFocused;
        std::span<const til::point_span> selectionSpans;
        til::color selectionBackground;
    };

    enum class GridLines
    {
        None,
        Top,
        Bottom,
        Left,
        Right,
        Underline,
        DoubleUnderline,
        CurlyUnderline,
        DottedUnderline,
        DashedUnderline,
        Strikethrough,
        HyperlinkUnderline
    };
    using GridLineSet = til::enumset<GridLines>;

    class __declspec(novtable) IRenderEngine
    {
    public:
#pragma warning(suppress : 26432) // If you define or delete any default operation in the type '...', define or delete them all (c.21).
        virtual ~IRenderEngine() = default;

        [[nodiscard]] virtual HRESULT StartPaint() noexcept = 0; // Returns S_FALSE, to skip EndPaint(), Present(), etc.
        [[nodiscard]] virtual HRESULT EndPaint() noexcept = 0;
        [[nodiscard]] virtual bool RequiresContinuousRedraw() noexcept = 0;
        [[nodiscard]] virtual bool WaitUntilCanRender(HANDLE shutdownEvent) noexcept = 0; // Returns false, if shutdownEvent is signaled
        [[nodiscard]] virtual HRESULT Present(HANDLE shutdownEvent) noexcept = 0;
        [[nodiscard]] virtual HRESULT ScrollFrame() noexcept = 0;
        [[nodiscard]] virtual HRESULT Invalidate(const til::rect* psrRegion) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateCursor(const til::rect* psrRegion) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateSystem(const til::rect* prcDirtyClient) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateSelection(std::span<const til::rect> selections) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateHighlight(std::span<const til::point_span> highlights, const TextBuffer& buffer) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateScroll(const til::point* pcoordDelta) noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateAll() noexcept = 0;
        [[nodiscard]] virtual HRESULT InvalidateTitle(std::wstring_view proposedTitle) noexcept = 0;
        [[nodiscard]] virtual HRESULT NotifyNewText(const std::wstring_view newText) noexcept = 0;
        [[nodiscard]] virtual HRESULT PrepareRenderInfo(RenderFrameInfo info) noexcept = 0;
        [[nodiscard]] virtual HRESULT ResetLineTransform() noexcept = 0;
        [[nodiscard]] virtual HRESULT PrepareLineTransform(LineRendition lineRendition, til::CoordType targetRow, til::CoordType viewportLeft) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintBackground() noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintBufferLine(std::span<const Cluster> clusters, til::point coord, bool fTrimLeft) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintBufferGridLines(GridLineSet lines, COLORREF gridlineColor, COLORREF underlineColor, size_t cchLine, til::point coordTarget) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintImageSlice(const ImageSlice& imageSlice, til::CoordType targetRow, til::CoordType viewportLeft) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintGraphics(const GraphicsPaintInfo& info) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintSelection(const til::rect& rect) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintCursor(const CursorOptions& options) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateDrawingBrushes(const TextAttribute& textAttributes, const RenderSettings& renderSettings, gsl::not_null<IRenderData*> pData, bool usingSoftFont, bool isSettingDefaultBrushes) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateSoftFont(std::span<const uint16_t> bitPattern, til::size cellSize, size_t centeringHint) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateDpi(int iDpi) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept = 0;
        [[nodiscard]] virtual HRESULT GetProposedFont(const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo, int iDpi) noexcept = 0;
        [[nodiscard]] virtual HRESULT GetDirtyArea(std::span<const til::rect>& area) noexcept = 0;
        [[nodiscard]] virtual HRESULT GetFontSize(_Out_ til::size* pFontSize) noexcept = 0;
        [[nodiscard]] virtual HRESULT IsGlyphWideByFont(std::wstring_view glyph, _Out_ bool* pResult) noexcept = 0;
        [[nodiscard]] virtual HRESULT UpdateTitle(std::wstring_view newTitle) noexcept = 0;
        virtual void UpdateHyperlinkHoveredId(const uint16_t hoveredId) noexcept = 0;
    };
}
#pragma warning(pop)
