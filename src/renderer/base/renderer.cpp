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

// Routine Description:
// - Creates a new renderer controller for a console.
// Arguments:
// - pData - The interface to console data structures required for rendering
// - pEngine - The output engine for targeting each rendering frame
// Return Value:
// - An instance of a Renderer.
// NOTE: CAN THROW IF MEMORY ALLOCATION FAILS.
Renderer::Renderer(IRenderData* pData,
                   _In_reads_(cEngines) IRenderEngine** const rgpEngines,
                   const size_t cEngines,
                   std::unique_ptr<IRenderThread> thread) :
    _pData(THROW_HR_IF_NULL(E_INVALIDARG, pData)),
    _pThread{ std::move(thread) },
    _destructing{ false },
    _clusterBuffer{}
{
    for (size_t i = 0; i < cEngines; i++)
    {
        IRenderEngine* engine = rgpEngines[i];
        // NOTE: THIS CAN THROW IF MEMORY ALLOCATION FAILS.
        AddRenderEngine(engine);
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
    if (_destructing)
    {
        return S_FALSE;
    }

    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        auto tries = maxRetriesForRenderEngine;
        while (tries > 0)
        {
            if (_destructing)
            {
                return S_FALSE;
            }

            const auto hr = _PaintFrameForEngine(pEngine);
            if (E_PENDING == hr)
            {
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
                continue;
            }
            LOG_IF_FAILED(hr);
            break;
        }
    }

    return S_OK;
}

[[nodiscard]] HRESULT Renderer::_PaintFrameForEngine(_In_ IRenderEngine* const pEngine) noexcept
try
{
    FAIL_FAST_IF_NULL(pEngine); // This is a programming error. Fail fast.

    _pData->LockConsole();
    auto unlock = wil::scope_exit([&]() {
        _pData->UnlockConsole();
    });

    // Last chance check if anything scrolled without an explicit invalidate notification since the last frame.
    _CheckViewportAndScroll();

    // Try to start painting a frame
    HRESULT const hr = pEngine->StartPaint();
    RETURN_IF_FAILED(hr);

    // Return early if there's nothing to paint.
    // The renderer itself tracks if there's something to do with the title, the
    //      engine won't know that.
    if (S_FALSE == hr)
    {
        return S_OK;
    }

    auto endPaint = wil::scope_exit([&]() {
        LOG_IF_FAILED(pEngine->EndPaint());

        // If the engine tells us it really wants to redraw immediately,
        // tell the thread so it doesn't go to sleep and ticks again
        // at the next opportunity.
        if (pEngine->RequiresContinuousRedraw())
        {
            _NotifyPaintFrame();
        }
    });

    RETURN_IF_FAILED(pEngine->PaintFrame(_pData));

    // Force scope exit end paint to finish up collecting information and possibly painting
    endPaint.reset();

    // Force scope exit unlock to let go of global lock so other threads can run
    unlock.reset();

    // Trigger out-of-lock presentation for renderers that can support it
    RETURN_IF_FAILED(pEngine->Present());

    // As we leave the scope, EndPaint will be called (declared above)
    return S_OK;
}
CATCH_RETURN()

void Renderer::_NotifyPaintFrame()
{
    // If we're running in the unittests, we might not have a render thread.
    if (_pThread)
    {
        // The thread will provide throttling for us.
        _pThread->NotifyPaint();
    }
}

// Routine Description:
// - Called when the system has requested we redraw a portion of the console.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerSystemRedraw(const RECT* const prcDirtyClient)
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->InvalidateSystem(prcDirtyClient));
    });

    _NotifyPaintFrame();
}

// Routine Description:
// - Called when a particular region within the console buffer has changed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerRedraw(const Viewport& region)
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        const bool needRedraw = pEngine->TriggerRedraw(_pData, region);
        if (needRedraw)
        {
            _NotifyPaintFrame();
        }
    });
}

// Routine Description:
// - Called when a particular coordinate within the console buffer has changed.
// Arguments:
// - pcoord: The buffer-space coordinate that has changed.
// Return Value:
// - <none>
void Renderer::TriggerRedraw(const COORD* const pcoord)
{
    TriggerRedraw(Viewport::FromCoord(*pcoord)); // this will notify to paint if we need it.
}

// Routine Description:
// - Called when the cursor has moved in the buffer. Allows for RenderEngines to
//      differentiate between cursor movements and other invalidates.
//   Visual Renderers (ex GDI) should invalidate the position, while the VT
//      engine ignores this. See MSFT:14711161.
// Arguments:
// - pcoord: The buffer-space position of the cursor.
// Return Value:
// - <none>
void Renderer::TriggerRedrawCursor(const COORD* const pcoord)
{
    // We first need to make sure the cursor position is within the buffer,
    // otherwise testing for a double width character can throw an exception.
    const auto& buffer = _pData->GetTextBuffer();
    if (buffer.GetSize().IsInBounds(*pcoord))
    {
        // We then calculate the region covered by the cursor. This requires
        // converting the buffer coordinates to an equivalent range of screen
        // cells for the cursor, taking line rendition into account.
        const LineRendition lineRendition = buffer.GetLineRendition(pcoord->Y);
        const SHORT cursorWidth = _pData->IsCursorDoubleWidth() ? 2 : 1;
        const SMALL_RECT cursorRect = { pcoord->X, pcoord->Y, pcoord->X + cursorWidth - 1, pcoord->Y };
        Viewport cursorView = Viewport::FromInclusive(BufferToScreenLine(cursorRect, lineRendition));

        // The region is clamped within the viewport boundaries and we only
        // trigger a redraw if the region is not empty.
        Viewport view = _pData->GetViewport();
        cursorView = view.Clamp(cursorView);

        if (cursorView.IsValid())
        {
            const SMALL_RECT updateRect = view.ConvertToOrigin(cursorView).ToExclusive();
            for (IRenderEngine* pEngine : _rgpEngines)
            {
                LOG_IF_FAILED(pEngine->InvalidateCursor(&updateRect));
            }

            _NotifyPaintFrame();
        }
    }
}

// Routine Description:
// - Called when something that changes the output state has occurred and the entire frame is now potentially invalid.
// - NOTE: Use sparingly. Try to reduce the refresh region where possible. Only use when a global state change has occurred.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerRedrawAll()
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->InvalidateAll());
    });

    _NotifyPaintFrame();
}

// Method Description:
// - Called when the host is about to die, to give the renderer one last chance
//      to paint before the host exits.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerTeardown() noexcept
{
    // We need to shut down the paint thread on teardown.
    _pThread->WaitForPaintCompletionAndDisable(INFINITE);

    // Then walk through and do one final paint on the caller's thread.
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        bool fEngineRequestsRepaint = false;
        HRESULT hr = pEngine->PrepareForTeardown(&fEngineRequestsRepaint);
        LOG_IF_FAILED(hr);

        if (SUCCEEDED(hr) && fEngineRequestsRepaint)
        {
            LOG_IF_FAILED(_PaintFrameForEngine(pEngine));
        }
    }
}

// Routine Description:
// - Called when the selected area in the console has changed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerSelection()
{
    try
    {
        std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
            LOG_IF_FAILED(pEngine->TriggerSelection(_pData));
        });

        _NotifyPaintFrame();
    }
    CATCH_LOG();
}

// Routine Description:
// - Called when we want to check if the viewport has moved and scroll accordingly if so.
// Arguments:
// - <none>
// Return Value:
// - True if something changed and we scrolled. False otherwise.
bool Renderer::_CheckViewportAndScroll()
{
    for (auto engine : _rgpEngines)
    {
        COORD coordDelta = engine->UpdateViewport(_pData);
        if (coordDelta.X != 0 || coordDelta.Y != 0)
        {
            LOG_IF_FAILED(engine->TriggerScroll(&coordDelta));
        }
    }

    return false;
}

// Routine Description:
// - Called when a scroll operation has occurred by manipulating the viewport.
// - This is a special case as calling out scrolls explicitly drastically improves performance.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerScroll()
{
    if (_CheckViewportAndScroll())
    {
        _NotifyPaintFrame();
    }
}

// Routine Description:
// - Called when a scroll operation wishes to explicitly adjust the frame by the given coordinate distance.
// - This is a special case as calling out scrolls explicitly drastically improves performance.
// - This should only be used when the viewport is not modified. It lets us know we can "scroll anyway" to save perf,
//   because the backing circular buffer rotated out from behind the viewport.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerScroll(const COORD* const pcoordDelta)
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->TriggerScroll(pcoordDelta));
    });

    _NotifyPaintFrame();
}

// Routine Description:
// - Called when the text buffer is about to circle its backing buffer.
//      A renderer might want to get painted before that happens.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerCircling()
{
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        bool fEngineRequestsRepaint = false;
        HRESULT hr = pEngine->InvalidateCircling(&fEngineRequestsRepaint);
        LOG_IF_FAILED(hr);

        if (SUCCEEDED(hr) && fEngineRequestsRepaint)
        {
            LOG_IF_FAILED(_PaintFrameForEngine(pEngine));
        }
    }
}

// Routine Description:
// - Called when the title of the console window has changed. Indicates that we
//      should update the title on the next frame.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerTitleChange()
{
    const auto newTitle = _pData->GetConsoleTitle();
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        LOG_IF_FAILED(pEngine->InvalidateTitle(newTitle));
    }
    _NotifyPaintFrame();
}

// Routine Description:
// - Called when a change in font or DPI has been detected.
// Arguments:
// - iDpi - New DPI value
// - FontInfoDesired - A description of the font we would like to have.
// - FontInfo - Data that will be fixed up/filled on return with the chosen font data.
// Return Value:
// - <none>
void Renderer::TriggerFontChange(const int iDpi, const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo)
{
    std::for_each(_rgpEngines.begin(), _rgpEngines.end(), [&](IRenderEngine* const pEngine) {
        LOG_IF_FAILED(pEngine->UpdateDpi(iDpi));
        LOG_IF_FAILED(pEngine->UpdateFont(FontInfoDesired, FontInfo));
    });

    _NotifyPaintFrame();
}

// Routine Description:
// - Get the information on what font we would be using if we decided to create a font with the given parameters
// - This is for use with speculative calculations.
// Arguments:
// - iDpi - The DPI of the target display
// - pFontInfoDesired - A description of the font we would like to have.
// - pFontInfo - Data that will be fixed up/filled on return with the chosen font data.
// Return Value:
// - S_OK if set successfully or relevant GDI error via HRESULT.
[[nodiscard]] HRESULT Renderer::GetProposedFont(const int iDpi, const FontInfoDesired& FontInfoDesired, _Out_ FontInfo& FontInfo)
{
    // If there's no head, return E_FAIL. The caller should decide how to
    //      handle this.
    // Currently, the only caller is the WindowProc:WM_GETDPISCALEDSIZE handler.
    //      It will assume that the proposed font is 1x1, regardless of DPI.
    if (_rgpEngines.size() < 1)
    {
        return E_FAIL;
    }

    // There will only every really be two engines - the real head and the VT
    //      renderer. We won't know which is which, so iterate over them.
    //      Only return the result of the successful one if it's not S_FALSE (which is the VT renderer)
    // TODO: 14560740 - The Window might be able to get at this info in a more sane manner
    FAIL_FAST_IF(!(_rgpEngines.size() <= 2));
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        const HRESULT hr = LOG_IF_FAILED(pEngine->GetProposedFont(FontInfoDesired, FontInfo, iDpi));
        // We're looking for specifically S_OK, S_FALSE is not good enough.
        if (hr == S_OK)
        {
            return hr;
        }
    };

    return E_FAIL;
}

// Routine Description:
// - Tests against the current rendering engine to see if this particular character would be considered
// full-width (inscribed in a square, twice as wide as a standard Western character, typically used for CJK
// languages) or half-width.
// - Typically used to determine how many positions in the backing buffer a particular character should fill.
// NOTE: This only handles 1 or 2 wide (in monospace terms) characters.
// Arguments:
// - glyph - the utf16 encoded codepoint to test
// Return Value:
// - True if the codepoint is full-width (two wide), false if it is half-width (one wide).
bool Renderer::IsGlyphWideByFont(const std::wstring_view glyph)
{
    bool fIsFullWidth = false;

    // There will only every really be two engines - the real head and the VT
    //      renderer. We won't know which is which, so iterate over them.
    //      Only return the result of the successful one if it's not S_FALSE (which is the VT renderer)
    // TODO: 14560740 - The Window might be able to get at this info in a more sane manner
    FAIL_FAST_IF(!(_rgpEngines.size() <= 2));
    for (IRenderEngine* const pEngine : _rgpEngines)
    {
        const HRESULT hr = LOG_IF_FAILED(pEngine->IsGlyphWideByFont(glyph, &fIsFullWidth));
        // We're looking for specifically S_OK, S_FALSE is not good enough.
        if (hr == S_OK)
        {
            return fIsFullWidth;
        }
    }

    return fIsFullWidth;
}

// Routine Description:
// - Sets an event in the render thread that allows it to proceed, thus enabling painting.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::EnablePainting()
{
    _pThread->EnablePainting();
}

// Routine Description:
// - Waits for the current paint operation to complete, if any, up to the specified timeout.
// - Resets an event in the render thread that precludes it from advancing, thus disabling rendering.
// - If no paint operation is currently underway, returns immediately.
// Arguments:
// - dwTimeoutMs - Milliseconds to wait for the current paint operation to complete, if any (can be INFINITE).
// Return Value:
// - <none>
void Renderer::WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs)
{
    _pThread->WaitForPaintCompletionAndDisable(dwTimeoutMs);
}

// Method Description:
// - Adds another Render engine to this renderer. Future rendering calls will
//      also be sent to the new renderer.
// Arguments:
// - pEngine: The new render engine to be added
// Return Value:
// - <none>
// Throws if we ran out of memory or there was some other error appending the
//      engine to our collection.
void Renderer::AddRenderEngine(_In_ IRenderEngine* const pEngine)
{
    THROW_HR_IF_NULL(E_INVALIDARG, pEngine);
    pEngine->UpdateViewport(_pData);
    _rgpEngines.push_back(pEngine);
}

// Method Description:
// - Registers a callback that will be called when this renderer gives up.
//   An application consuming a renderer can use this to display auxiliary Retry UI
// Arguments:
// - pfn: the callback
// Return Value:
// - <none>
void Renderer::SetRendererEnteredErrorStateCallback(std::function<void()> pfn)
{
    _pfnRendererEnteredErrorState = std::move(pfn);
}

// Method Description:
// - Attempts to restart the renderer.
void Renderer::ResetErrorStateAndResume()
{
    // because we're not stateful (we could be in the future), all we want to do is reenable painting.
    EnablePainting();
}

// Method Description:
// - Blocks until the engines are able to render without blocking.
void Renderer::WaitUntilCanRender()
{
    for (const auto pEngine : _rgpEngines)
    {
        pEngine->WaitUntilCanRender();
    }
}
