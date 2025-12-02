/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- RenderEngineBase.hpp

Abstract:
- Implements a set of functions with common behavior across all render engines.
  For example, the behavior for setting the title. The title may change many
  times in the course of a single frame, but the RenderEngine should only
  actually perform its update operation if at the start of a frame, the new
  window title will be different then the last frames, and it should only ever
  update the title once per frame.

Author(s):
- Mike Griese (migrie) 10-July-2018
--*/
#include "IRenderEngine.hpp"

#pragma once
namespace Microsoft::Console::Render
{
    class RenderEngineBase : public IRenderEngine
    {
    public:
        [[nodiscard]] HRESULT InvalidateSelection(std::span<const til::rect> selections) noexcept override;
        [[nodiscard]] HRESULT InvalidateHighlight(std::span<const til::point_span> highlights, const TextBuffer& buffer) noexcept override;
        [[nodiscard]] HRESULT InvalidateTitle(const std::wstring_view proposedTitle) noexcept override;

        [[nodiscard]] HRESULT UpdateTitle(const std::wstring_view newTitle) noexcept override;

        [[nodiscard]] HRESULT NotifyNewText(const std::wstring_view newText) noexcept override;

        [[nodiscard]] HRESULT UpdateSoftFont(const std::span<const uint16_t> bitPattern,
                                             const til::size cellSize,
                                             const size_t centeringHint) noexcept override;

        [[nodiscard]] HRESULT PrepareRenderInfo(RenderFrameInfo info) noexcept override;

        [[nodiscard]] HRESULT ResetLineTransform() noexcept override;
        [[nodiscard]] HRESULT PrepareLineTransform(const LineRendition lineRendition,
                                                   const til::CoordType targetRow,
                                                   const til::CoordType viewportLeft) noexcept override;

        [[nodiscard]] HRESULT PaintImageSlice(const ImageSlice& imageSlice,
                                              const til::CoordType targetRow,
                                              const til::CoordType viewportLeft) noexcept override;

        [[nodiscard]] bool RequiresContinuousRedraw() noexcept override;

        void WaitUntilCanRender() noexcept override;
        void UpdateHyperlinkHoveredId(const uint16_t hoveredId) noexcept override;

    protected:
        [[nodiscard]] virtual HRESULT _DoUpdateTitle(const std::wstring_view newTitle) noexcept = 0;

        bool _titleChanged = false;
        std::wstring _lastFrameTitle;
    };
}
