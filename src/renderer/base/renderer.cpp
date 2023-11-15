// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "renderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

using PointTree = interval_tree::IntervalTree<til::point, size_t>;

static constexpr auto maxRetriesForRenderEngine = 3;
// The renderer will wait this number of milliseconds * how many tries have elapsed before trying again.
static constexpr auto renderBackoffBaseTimeMilliseconds{ 150 };

#define FOREACH_ENGINE(var)   \
    for (auto var : _engines) \
        if (!var)             \
            break;            \
        else

// Routine Description:
// - Creates a new renderer controller for a console.
// Arguments:
// - pData - The interface to console data structures required for rendering
// - pEngine - The output engine for targeting each rendering frame
// Return Value:
// - An instance of a Renderer.
Renderer::Renderer(const RenderSettings& renderSettings,
                   IRenderData* pData,
                   _In_reads_(cEngines) IRenderEngine** const rgpEngines,
                   const size_t cEngines,
                   std::unique_ptr<RenderThread> thread) :
    _renderSettings(renderSettings),
    _pData(pData),
    _pThread{ std::move(thread) }
{
    for (size_t i = 0; i < cEngines; i++)
    {
        AddRenderEngine(rgpEngines[i]);
    }
}

// Routine Description:
// - Destroys an instance of a renderer
// Arguments:
// - <none>
// Return Value:
// - <none>
Renderer::~Renderer()
{
    // RenderThread blocks until it has shut down.
    _destructing = true;
    _pThread.reset();
}

// Routine Description:
// - Walks through the console data structures to compose a new frame based on the data that has changed since last call and outputs it to the connected rendering engine.
// Arguments:
// - <none>
// Return Value:
// - HRESULT S_OK, GDI error, Safe Math error, or state/argument errors.
[[nodiscard]] HRESULT Renderer::PaintFrame()
{
    RenderData data;
    _pData->UpdateRenderData(data);

    RenderingPayload payload;

    FOREACH_ENGINE(engine)
    {
        auto tries = maxRetriesForRenderEngine;
        while (tries > 0)
        {
            if (_destructing)
            {
                return S_FALSE;
            }

            try
            {
                engine->Render(payload);
                break;
            }
            CATCH_LOG();

            if (--tries == 0)
            {
                // Stop trying.
                _pThread->DisablePainting();
                if (_pfnRendererEnteredErrorState)
                {
                    _pfnRendererEnteredErrorState();
                }
                // If there's no callback, we still don't want to FAIL_FAST: the renderer going black
                // isn't near as bad as the entire application aborting. We're a component. We shouldn't
                // abort applications that host us.
                return S_FALSE;
            }

            // Add a bit of backoff.
            // Sleep 150ms, 300ms, 450ms before failing out and disabling the renderer.
            Sleep(renderBackoffBaseTimeMilliseconds * (maxRetriesForRenderEngine - tries));
        }
    }

    return S_OK;
}

void Renderer::NotifyPaintFrame() noexcept
{
    if (_pThread)
    {
        _pThread->NotifyPaint();
    }
}

void Renderer::TriggerSystemRedraw(const til::rect* const prcDirtyClient)
{
}

void Renderer::TriggerRedraw(const Viewport& region)
{
}

void Renderer::TriggerRedraw(const til::point* const pcoord)
{
}

void Renderer::TriggerRedrawCursor(const til::point* const pcoord)
{
}

void Renderer::TriggerRedrawAll(const bool backgroundChanged, const bool frameChanged)
{
}

void Renderer::TriggerTeardown() noexcept
{
}

void Renderer::TriggerSelection()
{
}

void Renderer::UpdateSoftFont(const std::span<const uint16_t> bitPattern, const til::size cellSize, const size_t centeringHint)
{
    // We reserve PUA code points U+EF20 to U+EF7F for soft fonts, but the range
    // that we test for in _IsSoftFontChar will depend on the size of the active
    // bitPattern. If it's empty (i.e. no soft font is set), then nothing will
    // match, and those code points will be treated the same as everything else.
    const auto softFontCharCount = cellSize.height ? bitPattern.size() / cellSize.height : 0;
    _lastSoftFontChar = _firstSoftFontChar + softFontCharCount - 1;

    FOREACH_ENGINE(pEngine)
    {
    }
    TriggerRedrawAll();
}

bool Renderer::s_IsSoftFontChar(const std::wstring_view& v, const size_t firstSoftFontChar, const size_t lastSoftFontChar)
{
    return v.size() == 1 && v[0] >= firstSoftFontChar && v[0] <= lastSoftFontChar;
}

bool Renderer::IsGlyphWideByFont(const std::wstring_view&)
{
    return false;
}

void Renderer::EnablePainting()
{
    if (_pThread)
    {
        _pThread->EnablePainting();
    }
}

void Renderer::WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs)
{
    _pThread->WaitForPaintCompletionAndDisable(dwTimeoutMs);
}

void Renderer::AddRenderEngine(_In_ IRenderEngine* const pEngine)
{
    THROW_HR_IF_NULL(E_INVALIDARG, pEngine);

    for (auto& p : _engines)
    {
        if (!p)
        {
            p = pEngine;
            return;
        }
    }

    THROW_HR_MSG(E_UNEXPECTED, "engines array is full");
}

void Renderer::RemoveRenderEngine(_In_ IRenderEngine* const pEngine)
{
    THROW_HR_IF_NULL(E_INVALIDARG, pEngine);

    for (auto& p : _engines)
    {
        if (p == pEngine)
        {
            p = nullptr;
            return;
        }
    }
}

void Renderer::WaitUntilCanRender()
{
    FOREACH_ENGINE(pEngine)
    {
        pEngine->WaitUntilCanRender();
    }
}
