// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/RenderEngineBase.hpp"
#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;

[[nodiscard]] HRESULT RenderEngineBase::InvalidateSelection(std::span<const til::rect> /*selections*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT RenderEngineBase::InvalidateHighlight(std::span<const til::point_span> /*highlights*/, const TextBuffer& /*renditions*/) noexcept
{
    return S_OK;
}

HRESULT RenderEngineBase::InvalidateTitle(const std::wstring_view proposedTitle) noexcept
{
    if (proposedTitle != _lastFrameTitle)
    {
        _titleChanged = true;
    }

    return S_OK;
}

HRESULT RenderEngineBase::UpdateTitle(const std::wstring_view newTitle) noexcept
{
    auto hr = S_FALSE;
    if (newTitle != _lastFrameTitle)
    {
        RETURN_IF_FAILED(_DoUpdateTitle(newTitle));
        _lastFrameTitle = newTitle;
        _titleChanged = false;
        hr = S_OK;
    }
    return hr;
}

HRESULT RenderEngineBase::NotifyNewText(const std::wstring_view /*newText*/) noexcept
{
    return S_FALSE;
}

HRESULT RenderEngineBase::UpdateSoftFont(const std::span<const uint16_t> /*bitPattern*/,
                                         const til::size /*cellSize*/,
                                         const size_t /*centeringHint*/) noexcept
{
    return S_FALSE;
}

HRESULT RenderEngineBase::PrepareRenderInfo(RenderFrameInfo /*info*/) noexcept
{
    return S_FALSE;
}

HRESULT RenderEngineBase::ResetLineTransform() noexcept
{
    return S_FALSE;
}

HRESULT RenderEngineBase::PrepareLineTransform(const LineRendition /*lineRendition*/,
                                               const til::CoordType /*targetRow*/,
                                               const til::CoordType /*viewportLeft*/) noexcept
{
    return S_FALSE;
}

HRESULT RenderEngineBase::PaintImageSlice(const ImageSlice& /*imageSlice*/,
                                          const til::CoordType /*targetRow*/,
                                          const til::CoordType /*viewportLeft*/) noexcept
{
    return S_FALSE;
}

// Method Description:
// - By default, no one should need continuous redraw. It ruins performance
//   in terms of CPU, memory, and battery life to just paint forever.
//   That's why we sleep when there's nothing to draw.
//   But if you REALLY WANT to do special effects... you need to keep painting.
[[nodiscard]] bool RenderEngineBase::RequiresContinuousRedraw() noexcept
{
    return false;
}

// Method Description:
// - Blocks until the engine is able to render without blocking.
void RenderEngineBase::WaitUntilCanRender() noexcept
{
    // Throttle the render loop a bit by default (~60 FPS), improving throughput.
    Sleep(8);
}

void RenderEngineBase::UpdateHyperlinkHoveredId(const uint16_t /*hoveredId*/) noexcept
{
}
