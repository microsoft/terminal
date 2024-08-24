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
#include "../../buffer/out/ImageSlice.hpp"

#pragma warning(push)
#pragma warning(disable : 4100) // '...': unreferenced formal parameter
namespace Microsoft::Console::Render
{
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

        [[nodiscard]] virtual HRESULT StartPaint() noexcept = 0;
        [[nodiscard]] virtual HRESULT EndPaint() noexcept = 0;
        [[nodiscard]] virtual bool RequiresContinuousRedraw() noexcept = 0;
        virtual void WaitUntilCanRender() noexcept = 0;
        [[nodiscard]] virtual HRESULT Present() noexcept = 0;
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
        [[nodiscard]] virtual HRESULT PaintBufferLine(std::span<const Cluster> clusters, til::point coord, bool fTrimLeft, bool lineWrapped) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintBufferGridLines(GridLineSet lines, COLORREF gridlineColor, COLORREF underlineColor, size_t cchLine, til::point coordTarget) noexcept = 0;
        [[nodiscard]] virtual HRESULT PaintImageSlice(const ImageSlice& imageSlice, til::CoordType targetRow, til::CoordType viewportLeft) noexcept = 0;
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
