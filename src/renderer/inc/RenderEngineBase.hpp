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
    struct BufferLineRenderData
    {
        IRenderData* pData;
        const TextBuffer& buffer;
        Microsoft::Console::Types::Viewport bufferLine;
        Microsoft::Console::Types::Viewport visible;
        COORD screenPosition;
        LineRendition lineRendention;
        bool lineWrapped;
        bool globalInvert;
        bool gridLineDrawingAllowed;
    };

    class RenderEngineBase : public IRenderEngine
    {
    public:
        ~RenderEngineBase() = 0;

    protected:
        RenderEngineBase();
        RenderEngineBase(const RenderEngineBase&) = default;
        RenderEngineBase(RenderEngineBase&&) = default;
        RenderEngineBase& operator=(const RenderEngineBase&) = default;
        RenderEngineBase& operator=(RenderEngineBase&&) = default;

        [[nodiscard]] std::optional<CursorOptions> _GetCursorInfo(IRenderData* pData);
        void _LoopDirtyLines(IRenderData* pData, std::function<void(BufferLineRenderData&)> action);
        void _LoopOverlay(IRenderData* pData, std::function<void(BufferLineRenderData&)> action);
        void _LoopSelection(IRenderData* pData, std::function<void(SMALL_RECT)> action);

        BufferLineRenderData _CalculateRenderDataForDirtyLine(const TextBuffer& buffer,
                                         Microsoft::Console::Types::Viewport visible,
                                         Microsoft::Console::Types::Viewport redraw,
                                         SHORT row) const;

        GridLines _CalculateGridLines(IRenderData* pData,
                                      const TextAttribute textAttribute,
                                      const COORD coordTarget);

        static GridLines s_GetGridlines(const TextAttribute& textAttribute) noexcept;

        std::vector<SMALL_RECT> _GetSelectionRects(IRenderData* pData) const;

        static bool s_IsAllSpaces(const std::wstring_view v)
        {
            // first non-space char is not found (is npos)
            return v.find_first_not_of(L" ") == decltype(v)::npos;
        }

    public:
        [[nodiscard]] HRESULT InvalidateTitle(const std::wstring_view proposedTitle) noexcept override;

        [[nodiscard]] virtual bool RequiresContinuousRedraw() noexcept override;

        void WaitUntilCanRender() noexcept override;

        void UpdateLastHoveredInterval(const std::optional<interval_tree::IntervalTree<til::point, size_t>::interval>& newInterval)
        {
            _hoveredInterval = newInterval;
        }

    protected:
        bool _titleChanged;
        std::wstring _lastFrameTitle;

        std::vector<Cluster> _clusterBuffer;
        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _hoveredInterval;
    };

    inline Microsoft::Console::Render::RenderEngineBase::~RenderEngineBase() {}
}
