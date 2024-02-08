// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "../inc/RenderEngineBase.hpp"
#pragma hdrstop
using namespace Microsoft::Console;
using namespace Microsoft::Console::Render;

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

HRESULT RenderEngineBase::PrepareRenderInfo(const RenderFrameInfo& /*info*/) noexcept
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

// Routine Description:
// - Notifies us that we're about to circle the buffer, giving us a chance to
//   force a repaint before the buffer contents are lost.
// - The default implementation of flush, is to do nothing for most renderers.
// Arguments:
// - circled - ignored
// - pForcePaint - Always filled with false
// Return Value:
// - S_FALSE because we don't use this.
[[nodiscard]] HRESULT RenderEngineBase::InvalidateFlush(_In_ const bool /*circled*/, _Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);
    *pForcePaint = false;
    return S_FALSE;
}
