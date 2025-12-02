// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "renderer.hpp"

#include <til/atomic.h>

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

using PointTree = interval_tree::IntervalTree<til::point, size_t>;

static constexpr TimerRepr TimerReprMax = std::numeric_limits<TimerRepr>::max();
static constexpr DWORD maxRetriesForRenderEngine = 3;
// The renderer will wait this number of milliseconds * how many tries have elapsed before trying again.
static constexpr DWORD renderBackoffBaseTimeMilliseconds = 150;

// Routine Description:
// - Creates a new renderer controller for a console.
// Arguments:
// - pData - The interface to console data structures required for rendering
// Return Value:
// - An instance of a Renderer.
Renderer::Renderer(RenderSettings& renderSettings, IRenderData* pData) :
    _renderSettings(renderSettings),
    _pData(pData)
{
    _cursorBlinker = RegisterTimer("cursor blink", [](Renderer& renderer, TimerHandle) {
        renderer._cursorBlinkerOn = !renderer._cursorBlinkerOn;
    });
    _renditionBlinker = RegisterTimer("blink rendition", [](Renderer& renderer, TimerHandle) {
        renderer._renderSettings.ToggleBlinkRendition();
        renderer.TriggerRedrawAll();
    });
}

Renderer::~Renderer()
{
    TriggerTeardown();
}

IRenderData* Renderer::GetRenderData() const noexcept
{
    return _pData;
}

// Routine Description:
// - Sets an event in the render thread that allows it to proceed, thus enabling painting.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::EnablePainting()
{
    // When the renderer is constructed, the initial viewport won't be available yet,
    // but once EnablePainting is called it should be safe to retrieve.
    _viewport = _pData->GetViewport();

    _enable.SetEvent();

    if (const auto guard = _threadMutex.lock_exclusive(); !_thread)
    {
        _threadKeepRunning.store(true, std::memory_order_relaxed);

        _thread.reset(CreateThread(nullptr, 0, s_renderThread, this, 0, nullptr));
        THROW_LAST_ERROR_IF(!_thread);

        // SetThreadDescription only works on 1607 and higher. If we cannot find it,
        // then it's no big deal. Just skip setting the description.
        const auto func = GetProcAddressByFunctionDeclaration(GetModuleHandleW(L"kernel32.dll"), SetThreadDescription);
        if (func)
        {
            LOG_IF_FAILED(func(_thread.get(), L"Rendering Output Thread"));
        }
    }
}

void Renderer::_disablePainting() noexcept
{
    _enable.ResetEvent();
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
    if (const auto guard = _threadMutex.lock_exclusive(); _thread)
    {
        // The render thread first waits for the event and then checks _threadKeepRunning. By doing it
        // in reverse order here, we ensure that it's impossible for the render thread to miss this.
        _threadKeepRunning.store(false, std::memory_order_relaxed);
        NotifyPaintFrame();
        _enable.SetEvent();

        WaitForSingleObject(_thread.get(), INFINITE);
        _thread.reset();
    }

    _disablePainting();
}

void Renderer::NotifyPaintFrame() noexcept
{
    _redraw.store(true, std::memory_order_relaxed);
    til::atomic_notify_one(_redraw);
}

DWORD WINAPI Renderer::s_renderThread(void* param) noexcept
{
    return static_cast<Renderer*>(param)->_renderThread();
}

DWORD Renderer::_renderThread() noexcept
{
    while (true)
    {
        _enable.wait();
        _waitUntilCanRender();
        _waitUntilTimerOrRedraw();

        if (!_threadKeepRunning.load(std::memory_order_relaxed))
        {
            break;
        }

        LOG_IF_FAILED(PaintFrame());
    }

    return S_OK;
}

void Renderer::_waitUntilCanRender() noexcept
{
    for (const auto pEngine : _engines)
    {
        pEngine->WaitUntilCanRender();
    }
}

TimerHandle Renderer::RegisterTimer(const char* description, TimerCallback routine)
{
    // If it doesn't crash now, it would crash later.
    WI_ASSERT(routine != nullptr);

    const auto id = _nextTimerId++;

    _timers.push_back(TimerRoutine{
        .description = description,
        .interval = TimerReprMax,
        .next = TimerReprMax,
        .routine = std::move(routine),
    });

    return TimerHandle{ id };
}

bool Renderer::IsTimerRunning(TimerHandle handle) const
{
    const auto& timer = _timers.at(handle.id);
    return timer.next != TimerReprMax;
}

TimerDuration Renderer::GetTimerInterval(TimerHandle handle) const
{
    const auto& timer = _timers.at(handle.id);
    return TimerDuration{ timer.interval };
}

void Renderer::StarTimer(TimerHandle handle, TimerDuration delay)
{
    _starTimer(handle, delay.count(), TimerReprMax);
}

void Renderer::StartRepeatingTimer(TimerHandle handle, TimerDuration interval)
{
    _starTimer(handle, interval.count(), interval.count());
}

void Renderer::_starTimer(TimerHandle handle, TimerRepr delay, TimerRepr interval)
{
    // Nothing breaks if these assertions are violated, but you should still violate them.
    // A timer with a 1-hour delay is weird and indicative of a bug. It should have been
    // a max-wait (TimerReprMax) instead, which turns into an INFINITE timeout
    // for WaitOnAddress(), which in turn is less costly than one with timeout.
#ifndef NDEBUG
    constexpr TimerRepr one_min_in_100ns = 60 * 1000 * 10000;
    assert(delay > 0 && (delay < one_min_in_100ns || delay == TimerReprMax));
    assert(interval > 0 && (interval < one_min_in_100ns || interval == TimerReprMax));
#endif

    auto& timer = _timers.at(handle.id);
    timer.interval = interval;
    timer.next = _timerSaturatingAdd(_timerInstant(), delay);

    // Tickle _waitUntilCanRender() into calling _calculateTimerMaxWait() again.
    // WaitOnAddress() will return with TRUE, even if the atomic didn't change.
    til::atomic_notify_one(_redraw);
}

void Renderer::StopTimer(TimerHandle handle)
{
    auto& timer = _timers.at(handle.id);
    timer.interval = TimerReprMax;
    timer.next = TimerReprMax;
}

DWORD Renderer::_calculateTimerMaxWait() noexcept
{
    if (_timers.empty())
    {
        return INFINITE;
    }

    const auto now = _timerInstant();
    auto wait = TimerReprMax;

    for (const auto& timer : _timers)
    {
        wait = std::min(wait, _timerSaturatingSub(timer.next, now));
    }

    return _timerToMillis(wait);
}

void Renderer::_waitUntilTimerOrRedraw() noexcept
{
    // Did we get an explicit rendering request? Yes? Exit.
    //
    // We don't reset _redraw just yet because we can delay that until we
    // actually acquired the console lock. That's the main synchronization
    // point and the instant we know everyone else is blocked. See PaintFrame().
    while (!_redraw.load(std::memory_order_relaxed))
    {
        // Otherwise calculate when the next timer expires.
        const auto wait = _calculateTimerMaxWait();
        if (wait == 0)
        {
            break;
        }

        // and wait until the timer expires, or we potentially got a rendering request.
        constexpr auto bad = false;
        if (!til::atomic_wait(_redraw, bad, wait))
        {
            // The timer expired.
            assert(GetLastError() == ERROR_TIMEOUT); // What else could it be?
            break;
        }

        // If WaitOnAddress returned TRUE, we got signaled and retry.
    }
}

void Renderer::_tickTimers() noexcept
{
    const auto now = _timerInstant();
    size_t id = 0;

    for (auto& timer : _timers)
    {
        if (now >= timer.next)
        {
            // Prevent clock drift by incrementing the originally scheduled time.
            timer.next = _timerSaturatingAdd(timer.next, timer.interval);
            // ...but still take care to not schedule in the past.
            if (timer.next <= now)
            {
                timer.next = now + timer.interval;
            }

            try
            {
                timer.routine(*this, TimerHandle{ id });
            }
            CATCH_LOG();
        }

        id++;
    }
}

ULONGLONG Renderer::_timerInstant() noexcept
{
    // QueryUnbiasedInterruptTime is what WaitOnAddress uses internally.
    ULONGLONG now;
    QueryUnbiasedInterruptTime(&now);
    return now;
}

TimerRepr Renderer::_timerSaturatingAdd(TimerRepr a, TimerRepr b) noexcept
{
    auto c = a + b;
    if (c < a)
    {
        c = TimerReprMax;
    }
    return c;
}

TimerRepr Renderer::_timerSaturatingSub(TimerRepr a, TimerRepr b) noexcept
{
    auto c = a - b;
    if (c > a)
    {
        c = 0;
    }
    return c;
}

DWORD Renderer::_timerToMillis(TimerRepr t) noexcept
{
    return gsl::narrow_cast<DWORD>(std::min<TimerRepr>(t / 10000, DWORD_MAX));
}

// Routine Description:
// - Walks through the console data structures to compose a new frame based on the data that has changed since last call and outputs it to the connected rendering engine.
// Arguments:
// - <none>
// Return Value:
// - HRESULT S_OK, GDI error, Safe Math error, or state/argument errors.
[[nodiscard]] HRESULT Renderer::PaintFrame()
{
    auto tries = maxRetriesForRenderEngine;
    while (tries > 0)
    {
        // BODGY: Optimally we would want to retry per engine, but that causes different
        // problems (intermittent inconsistent states between text renderer and UIA output,
        // not being able to lock the cursor location, etc.).
        const auto hr = _PaintFrame();
        if (SUCCEEDED(hr))
        {
            break;
        }

        LOG_HR_IF(hr, hr != E_PENDING);

        if (--tries == 0)
        {
            // Stop trying.
            _disablePainting();
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

    return S_OK;
}

[[nodiscard]] HRESULT Renderer::_PaintFrame() noexcept
{
    {
        _pData->LockConsole();
        auto unlock = wil::scope_exit([&]() {
            _pData->UnlockConsole();
        });

        if (_isSynchronizingOutput)
        {
            _synchronizeWithOutput();
        }

        _tickTimers();

        // We reset _redraw after _tickTimers() so that NotifyPaintFrame() calls
        // are picked up and ignored. We're about to render a frame after all.
        // We do it before the remaining code below so that if we do have an
        // intentional call to NotifyPaintFrame(), it triggers a redraw.
        _redraw.store(false, std::memory_order_relaxed);

        // NOTE: _CheckViewportAndScroll() updates _viewport which is used by all other functions.
        _CheckViewportAndScroll();

        _scheduleRenditionBlink();

        // Add the previous cursor / composition to the dirty rect.
        _invalidateCurrentCursor();
        _invalidateOldComposition();

        // Add the new cursor position to the dirt rect.
        // Prepare the composition for insertion into the output screen.
        _updateCursorInfo();
        _invalidateCurrentCursor(); // NOTE: This now refers to the updated cursor position.
        _prepareNewComposition();

        for (const auto pEngine : _engines)
        {
            RETURN_IF_FAILED(_PaintFrameForEngine(pEngine));
        }
    }

    for (const auto pEngine : _engines)
    {
        RETURN_IF_FAILED(pEngine->Present());
    }

    return S_OK;
}

[[nodiscard]] HRESULT Renderer::_PaintFrameForEngine(_In_ IRenderEngine* const pEngine) noexcept
try
{
    FAIL_FAST_IF_NULL(pEngine); // This is a programming error. Fail fast.

    // Try to start painting a frame
    const auto hr = pEngine->StartPaint();
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
            NotifyPaintFrame();
        }
    });

    // A. Prep Colors
    RETURN_IF_FAILED(_UpdateDrawingBrushes(pEngine, {}, false, true));

    // B. Perform Scroll Operations
    RETURN_IF_FAILED(_PerformScrolling(pEngine));

    // C. Prepare the engine with additional information before we start drawing.
    RETURN_IF_FAILED(_PrepareRenderInfo(pEngine));

    // 1. Paint Background
    RETURN_IF_FAILED(_PaintBackground(pEngine));

    // 2. Paint Rows of Text
    _PaintBufferOutput(pEngine);

    // 4. Paint Selection
    _PaintSelection(pEngine);

    // 5. Paint Cursor
    _PaintCursor(pEngine);

    // 6. Paint window title
    RETURN_IF_FAILED(_PaintTitle(pEngine));

    // Force scope exit end paint to finish up collecting information and possibly painting
    endPaint.reset();

    // As we leave the scope, EndPaint will be called (declared above)
    return S_OK;
}
CATCH_RETURN()

// NOTE: You must be holding the console lock when calling this function.
void Renderer::SynchronizedOutputChanged() noexcept
{
    const auto so = _renderSettings.GetRenderMode(RenderSettings::Mode::SynchronizedOutput);
    if (_isSynchronizingOutput == so)
    {
        return;
    }

    // If `_isSynchronizingOutput` is true, it'll kick the
    // render thread into calling `_synchronizeWithOutput()`...
    _isSynchronizingOutput = so;

    if (!_isSynchronizingOutput)
    {
        // ...otherwise, unblock `_synchronizeWithOutput()` from the `WaitOnAddress` call.
        WakeByAddressSingle(&_isSynchronizingOutput);

        // It's crucial to give the render thread at least a chance to gain the lock.
        // Otherwise, a VT application could continuously spam DECSET 2026 (Synchronized Output) and
        // essentially drop our renderer to 10 FPS, because `_isSynchronizingOutput` is always true.
        //
        // Obviously calling LockConsole/UnlockConsole here is an awful, ugly hack,
        // since there's no guarantee that this is the same lock as the one the VT parser uses.
        // But the alternative is Denial-Of-Service of the render thread.
        //
        // Note that this causes raw throughput of DECSET 2026 to be comparatively low, but that's fine.
        // Apps that use DECSET 2026 don't produce that sequence continuously, but rather at a fixed rate.
        _pData->UnlockConsole();
        _pData->LockConsole();
    }
}

void Renderer::_synchronizeWithOutput() noexcept
{
    constexpr DWORD timeout = 100;

    UINT64 start = 0;
    DWORD elapsed = 0;
    bool wrong = false;

    QueryUnbiasedInterruptTime(&start);

    // Wait for `_isSynchronizingOutput` to be set to false or for a timeout to occur.
    while (true)
    {
        // We can't call a blocking function while holding the console lock, so release it temporarily.
        _pData->UnlockConsole();
        const auto ok = WaitOnAddress(&_isSynchronizingOutput, &wrong, sizeof(_isSynchronizingOutput), timeout - elapsed);
        _pData->LockConsole();

        if (!ok || !_isSynchronizingOutput)
        {
            break;
        }

        UINT64 now;
        QueryUnbiasedInterruptTime(&now);
        elapsed = static_cast<DWORD>((now - start) / 10000);
        if (elapsed >= timeout)
        {
            break;
        }
    }

    // If a timeout occurred, `_isSynchronizingOutput` may still be true.
    // Set it to false now to skip calling `_synchronizeWithOutput()` on the next frame.
    _isSynchronizingOutput = false;
    _renderSettings.SetRenderMode(RenderSettings::Mode::SynchronizedOutput, false);
}

void Renderer::AllowCursorVisibility(InhibitionSource source, bool enable) noexcept
{
    const auto before = _cursorVisibilityInhibitors.any();
    _cursorVisibilityInhibitors.set(source, !enable);
    const auto after = _cursorVisibilityInhibitors.any();
    if (before != after)
    {
        NotifyPaintFrame();
    }
}

void Renderer::AllowCursorBlinking(InhibitionSource source, bool enable) noexcept
{
    const auto before = _cursorBlinkingInhibitors.any();
    _cursorBlinkingInhibitors.set(source, !enable);
    const auto after = _cursorBlinkingInhibitors.any();
    if (before != after)
    {
        NotifyPaintFrame();
    }
}

// Routine Description:
// - Called when the system has requested we redraw a portion of the console.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerSystemRedraw(const til::rect* const prcDirtyClient)
{
    for (const auto pEngine : _engines)
    {
        LOG_IF_FAILED(pEngine->InvalidateSystem(prcDirtyClient));
    }

    NotifyPaintFrame();
}

// Routine Description:
// - Called when a particular region within the console buffer has changed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerRedraw(const Viewport& region)
{
    auto view = _pData->GetViewport();
    auto srUpdateRegion = region.ToExclusive();

    // If the dirty region has double width lines, we need to double the size of
    // the right margin to make sure all the affected cells are invalidated.
    const auto& buffer = _pData->GetTextBuffer();
    for (auto row = srUpdateRegion.top; row < srUpdateRegion.bottom; row++)
    {
        if (buffer.IsDoubleWidthLine(row))
        {
            srUpdateRegion.right *= 2;
            break;
        }
    }

    if (view.TrimToViewport(&srUpdateRegion))
    {
        view.ConvertToOrigin(&srUpdateRegion);
        for (const auto pEngine : _engines)
        {
            LOG_IF_FAILED(pEngine->Invalidate(&srUpdateRegion));
        }

        NotifyPaintFrame();
    }
}

// Routine Description:
// - Called when a particular coordinate within the console buffer has changed.
// Arguments:
// - pcoord: The buffer-space coordinate that has changed.
// Return Value:
// - <none>
void Renderer::TriggerRedraw(const til::point* const pcoord)
{
    TriggerRedraw(Viewport::FromDimensions(*pcoord, { 1, 1 })); // this will notify to paint if we need it.
}

// Routine Description:
// - Called when something that changes the output state has occurred and the entire frame is now potentially invalid.
// - NOTE: Use sparingly. Try to reduce the refresh region where possible. Only use when a global state change has occurred.
// Arguments:
// - backgroundChanged - Set to true if the background color has changed.
// - frameChanged - Set to true if the frame colors have changed.
// Return Value:
// - <none>
void Renderer::TriggerRedrawAll(const bool backgroundChanged, const bool frameChanged)
{
    for (const auto pEngine : _engines)
    {
        LOG_IF_FAILED(pEngine->InvalidateAll());
    }

    NotifyPaintFrame();

    if (backgroundChanged && _pfnBackgroundColorChanged)
    {
        _pfnBackgroundColorChanged();
    }

    if (frameChanged && _pfnFrameColorChanged)
    {
        _pfnFrameColorChanged();
    }
}

// Routine Description:
// - Called when the selected area in the console has changed.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::TriggerSelection()
try
{
    const auto spans = _pData->GetSelectionSpans();
    if (spans.size() != _lastSelectionPaintSize || (!spans.empty() && _lastSelectionPaintSpan != til::point_span{ spans.front().start, spans.back().end }))
    {
        std::vector<til::rect> newSelectionViewportRects;

        _lastSelectionPaintSize = spans.size();
        if (_lastSelectionPaintSize)
        {
            _lastSelectionPaintSpan = til::point_span{ spans.front().start, spans.back().end };

            const auto& buffer = _pData->GetTextBuffer();
            const auto bufferWidth = buffer.GetSize().Width();
            const til::rect vp{ _viewport.ToExclusive() };
            for (auto&& sp : spans)
            {
                sp.iterate_rows_exclusive(bufferWidth, [&](til::CoordType row, til::CoordType min, til::CoordType max) {
                    const auto shift = buffer.GetLineRendition(row) != LineRendition::SingleWidth ? 1 : 0;
                    min <<= shift;
                    max <<= shift;
                    til::rect r{ min, row, max, row + 1 };
                    newSelectionViewportRects.emplace_back(r.to_origin(vp));
                });
            }
        }

        for (const auto pEngine : _engines)
        {
            LOG_IF_FAILED(pEngine->InvalidateSelection(_lastSelectionRectsByViewport));
            LOG_IF_FAILED(pEngine->InvalidateSelection(newSelectionViewportRects));
        }

        std::exchange(_lastSelectionRectsByViewport, newSelectionViewportRects);

        NotifyPaintFrame();
    }
}
CATCH_LOG()

// Routine Description:
// - Called when the search highlight areas in the console have changed.
void Renderer::TriggerSearchHighlight(const std::vector<til::point_span>& oldHighlights)
try
{
    // no need to invalidate focused search highlight separately as they are
    // included in (all) search highlights.
    const auto newHighlights = _pData->GetSearchHighlights();

    if (oldHighlights.empty() && newHighlights.empty())
    {
        return;
    }

    const auto& buffer = _pData->GetTextBuffer();

    for (const auto pEngine : _engines)
    {
        LOG_IF_FAILED(pEngine->InvalidateHighlight(oldHighlights, buffer));
        LOG_IF_FAILED(pEngine->InvalidateHighlight(newHighlights, buffer));
    }

    NotifyPaintFrame();
}
CATCH_LOG()

// Routine Description:
// - Called when we want to check if the viewport has moved and scroll accordingly if so.
// Arguments:
// - <none>
// Return Value:
// - True if something changed and we scrolled. False otherwise.
bool Renderer::_CheckViewportAndScroll()
{
    const auto srOldViewport = _viewport.ToInclusive();
    const auto srNewViewport = _pData->GetViewport().ToInclusive();

    if (!_forceUpdateViewport && srOldViewport == srNewViewport)
    {
        return false;
    }

    _viewport = Viewport::FromInclusive(srNewViewport);
    _forceUpdateViewport = false;

    til::point coordDelta;
    coordDelta.x = srOldViewport.left - srNewViewport.left;
    coordDelta.y = srOldViewport.top - srNewViewport.top;

    for (const auto engine : _engines)
    {
        LOG_IF_FAILED(engine->UpdateViewport(srNewViewport));
        LOG_IF_FAILED(engine->InvalidateScroll(&coordDelta));
    }

    _ScrollPreviousSelection(coordDelta);

    // The cursor may have moved out of or into the viewport. Update the .inViewport property.
    {
        const auto view = ScreenToBufferLine(srNewViewport, _currentCursorOptions.lineRendition);
        auto coordCursor = _currentCursorOptions.coordCursor;

        // `coordCursor` was stored in viewport-relative while `view` is in absolute coordinates.
        // --> Turn it back into the absolute coordinates with the help of the viewport.
        // We have to use the new viewport, because _ScrollPreviousSelection adjusts the cursor position to match the new one.
        coordCursor.y += srNewViewport.top;

        // Note that we allow the X coordinate to be outside the left border by 1 position,
        // because the cursor could still be visible if the focused character is double width.
        const auto xInRange = coordCursor.x >= view.left - 1 && coordCursor.x <= view.right;
        const auto yInRange = coordCursor.y >= view.top && coordCursor.y <= view.bottom;

        _currentCursorOptions.inViewport = xInRange && yInRange;
    }

    return true;
}

void Renderer::_scheduleRenditionBlink()
{
    const auto& buffer = _pData->GetTextBuffer();
    bool blinkUsed = false;

    for (auto row = _viewport.Top(); row < _viewport.BottomExclusive(); ++row)
    {
        const auto& r = buffer.GetRowByOffset(row);
        for (const auto& attr : r.Attributes())
        {
            if (attr.IsBlinking())
            {
                blinkUsed = true;
                goto why_does_cpp_not_have_labeled_loops;
            }
        }
    }

why_does_cpp_not_have_labeled_loops:
    if (blinkUsed != IsTimerRunning(_renditionBlinker))
    {
        if (blinkUsed)
        {
            StartRepeatingTimer(_renditionBlinker, std::chrono::seconds(1));
        }
        else
        {
            StopTimer(_renditionBlinker);
        }
    }
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
        NotifyPaintFrame();
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
void Renderer::TriggerScroll(const til::point* const pcoordDelta)
{
    for (const auto pEngine : _engines)
    {
        LOG_IF_FAILED(pEngine->InvalidateScroll(pcoordDelta));
    }

    _ScrollPreviousSelection(*pcoordDelta);

    NotifyPaintFrame();
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
    for (const auto pEngine : _engines)
    {
        LOG_IF_FAILED(pEngine->InvalidateTitle(newTitle));
    }
    NotifyPaintFrame();
}

void Renderer::TriggerNewTextNotification(const std::wstring_view newText)
{
    for (const auto pEngine : _engines)
    {
        LOG_IF_FAILED(pEngine->NotifyNewText(newText));
    }
}

// Routine Description:
// - Update the title for a particular engine.
// Arguments:
// - pEngine: the engine to update the title for.
// Return Value:
// - the HRESULT of the underlying engine's UpdateTitle call.
HRESULT Renderer::_PaintTitle(IRenderEngine* const pEngine)
{
    const auto newTitle = _pData->GetConsoleTitle();
    return pEngine->UpdateTitle(newTitle);
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
    for (const auto pEngine : _engines)
    {
        LOG_IF_FAILED(pEngine->UpdateDpi(iDpi));
        LOG_IF_FAILED(pEngine->UpdateFont(FontInfoDesired, FontInfo));
    }

    NotifyPaintFrame();
}

// Routine Description:
// - Called when the active soft font has been updated.
// Arguments:
// - bitPattern - An array of scanlines representing all the glyphs in the font.
// - cellSize - The cell size for an individual glyph.
// - centeringHint - The horizontal extent that glyphs are offset from center.
// Return Value:
// - <none>
void Renderer::UpdateSoftFont(const std::span<const uint16_t> bitPattern, const til::size cellSize, const size_t centeringHint)
{
    // We reserve PUA code points U+EF20 to U+EF7F for soft fonts, but the range
    // that we test for in _IsSoftFontChar will depend on the size of the active
    // bitPattern. If it's empty (i.e. no soft font is set), then nothing will
    // match, and those code points will be treated the same as everything else.
    const auto softFontCharCount = cellSize.height ? bitPattern.size() / cellSize.height : 0;
    _lastSoftFontChar = _firstSoftFontChar + softFontCharCount - 1;

    for (const auto pEngine : _engines)
    {
        LOG_IF_FAILED(pEngine->UpdateSoftFont(bitPattern, cellSize, centeringHint));
    }
    TriggerRedrawAll();
}

// We initially tried to have a "_isSoftFontChar" member function, but MSVC
// failed to inline it at _all_ call sites (check invocations inside loops).
// This issue strangely doesn't occur with static functions.
bool Renderer::s_IsSoftFontChar(const std::wstring_view& v, const size_t firstSoftFontChar, const size_t lastSoftFontChar)
{
    return v.size() == 1 && v[0] >= firstSoftFontChar && v[0] <= lastSoftFontChar;
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
    // There will only every really be two engines - the real head and the VT
    //      renderer. We won't know which is which, so iterate over them.
    //      Only return the result of the successful one if it's not S_FALSE (which is the VT renderer)
    // TODO: 14560740 - The Window might be able to get at this info in a more sane manner
    for (const auto pEngine : _engines)
    {
        const auto hr = LOG_IF_FAILED(pEngine->GetProposedFont(FontInfoDesired, FontInfo, iDpi));
        // We're looking for specifically S_OK, S_FALSE is not good enough.
        if (hr == S_OK)
        {
            return hr;
        }
    }

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
    auto fIsFullWidth = false;

    // There will only every really be two engines - the real head and the VT
    //      renderer. We won't know which is which, so iterate over them.
    //      Only return the result of the successful one if it's not S_FALSE (which is the VT renderer)
    // TODO: 14560740 - The Window might be able to get at this info in a more sane manner
    for (const auto pEngine : _engines)
    {
        const auto hr = LOG_IF_FAILED(pEngine->IsGlyphWideByFont(glyph, &fIsFullWidth));
        // We're looking for specifically S_OK, S_FALSE is not good enough.
        if (hr == S_OK)
        {
            break;
        }
    }

    return fIsFullWidth;
}

// Routine Description:
// - Paint helper to fill in the background color of the invalid area within the frame.
// Arguments:
// - <none>
// Return Value:
// - <none>
[[nodiscard]] HRESULT Renderer::_PaintBackground(_In_ IRenderEngine* const pEngine)
{
    return pEngine->PaintBackground();
}

// Routine Description:
// - Paint helper to copy the primary console buffer text onto the screen.
// - This portion primarily handles figuring the current viewport, comparing it/trimming it versus the invalid portion of the frame, and queuing up, row by row, which pieces of text need to be further processed.
// - See also: Helper functions that separate out each complexity of text rendering.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::_PaintBufferOutput(_In_ IRenderEngine* const pEngine)
{
    // This is the subsection of the entire screen buffer that is currently being presented.
    // It can move left/right or top/bottom depending on how the viewport is scrolled
    // relative to the entire buffer.
    const auto compositionRow = _compositionCache ? _compositionCache->absoluteOrigin.y : -1;
    const auto& activeComposition = _pData->GetActiveComposition();

    // This is effectively the number of cells on the visible screen that need to be redrawn.
    // The origin is always 0, 0 because it represents the screen itself, not the underlying buffer.
    std::span<const til::rect> dirtyAreas;
    LOG_IF_FAILED(pEngine->GetDirtyArea(dirtyAreas));

    // This is to make sure any transforms are reset when this paint is finished.
    auto resetLineTransform = wil::scope_exit([&]() {
        LOG_IF_FAILED(pEngine->ResetLineTransform());
    });

    for (const auto& dirtyRect : dirtyAreas)
    {
        if (!dirtyRect)
        {
            continue;
        }

        auto dirty = Viewport::FromExclusive(dirtyRect);

        // Shift the origin of the dirty region to match the underlying buffer so we can
        // compare the two regions directly for intersection.
        dirty = Viewport::Offset(dirty, _viewport.Origin());

        // The intersection between what is dirty on the screen (in need of repaint)
        // and what is supposed to be visible on the screen (the viewport) is what
        // we need to walk through line-by-line and repaint onto the screen.
        const auto redraw = Viewport::Intersect(dirty, _viewport);

        // Retrieve the text buffer so we can read information out of it.
        auto& buffer = _pData->GetTextBuffer();
        // Now walk through each row of text that we need to redraw.
        for (auto row = redraw.Top(); row < redraw.BottomExclusive(); row++)
        {
            // Calculate the boundaries of a single line. This is from the left to right edge of the dirty
            // area in width and exactly 1 tall.
            const auto screenLine = til::inclusive_rect{ redraw.Left(), row, redraw.RightInclusive(), row };
            const auto& r = buffer.GetRowByOffset(row);

            // Draw the active composition.
            // We have to use some tricks here with const_cast, because the code after it relies on TextBufferCellIterator,
            // which isn't compatible with the scratchpad row. This forces us to back up and modify the actual row `r`.
            ROW* rowBackup = nullptr;
            if (row == compositionRow)
            {
                auto& scratch = buffer.GetScratchpadRow();
                scratch.CopyFrom(r);
                rowBackup = &scratch;

                std::wstring_view text{ activeComposition.text };
                RowWriteState state{
                    .columnLimit = r.GetReadableColumnCount(),
                    .columnEnd = _compositionCache->absoluteOrigin.x,
                };

                size_t off = 0;
                for (const auto& range : activeComposition.attributes)
                {
                    const auto len = range.len;
                    auto attr = range.attr;

                    // Use the color at the cursor if TSF didn't specify any explicit color.
                    if (attr.GetBackground().IsDefault())
                    {
                        attr.SetBackground(_compositionCache->baseAttribute.GetBackground());
                    }
                    if (attr.GetForeground().IsDefault())
                    {
                        attr.SetForeground(_compositionCache->baseAttribute.GetForeground());
                    }

                    state.text = text.substr(off, len);
                    state.columnBegin = state.columnEnd;
                    const_cast<ROW&>(r).ReplaceText(state);
                    const_cast<ROW&>(r).ReplaceAttributes(state.columnBegin, state.columnEnd, attr);
                    off += len;
                }
            }
            const auto restore = wil::scope_exit([&] {
                if (rowBackup)
                {
                    const_cast<ROW&>(r).CopyFrom(*rowBackup);
                }
            });

            // Convert the screen coordinates of the line to an equivalent
            // range of buffer cells, taking line rendition into account.
            const auto lineRendition = buffer.GetLineRendition(row);
            const auto bufferLine = Viewport::FromInclusive(ScreenToBufferLine(screenLine, lineRendition));

            // Find where on the screen we should place this line information. This requires us to re-map
            // the buffer-based origin of the line back onto the screen-based origin of the line.
            // For example, the screen might say we need to paint line 1 because it is dirty but the viewport
            // is actually looking at line 26 relative to the buffer. This means that we need line 27 out
            // of the backing buffer to fill in line 1 of the screen.
            const auto screenPosition = bufferLine.Origin() - til::point{ 0, _viewport.Top() };

            // Retrieve the cell information iterator limited to just this line we want to redraw.
            auto it = buffer.GetCellDataAt(bufferLine.Origin(), bufferLine);

            // Calculate if two things are true:
            // 1. this row wrapped
            // 2. We're painting the last col of the row.
            // In that case, set lineWrapped=true for the _PaintBufferOutputHelper call.
            const auto lineWrapped = (buffer.GetRowByOffset(bufferLine.Origin().y).WasWrapForced()) &&
                                     (bufferLine.RightExclusive() == buffer.GetSize().Width());

            // Prepare the appropriate line transform for the current row and viewport offset.
            LOG_IF_FAILED(pEngine->PrepareLineTransform(lineRendition, screenPosition.y, _viewport.Left()));

            // Ask the helper to paint through this specific line.
            _PaintBufferOutputHelper(pEngine, it, screenPosition, lineWrapped);

            // Paint any image content on top of the text.
            const auto imageSlice = buffer.GetRowByOffset(row).GetImageSlice();
            if (imageSlice) [[unlikely]]
            {
                LOG_IF_FAILED(pEngine->PaintImageSlice(*imageSlice, screenPosition.y, _viewport.Left()));
            }
        }
    }
}

static bool _IsAllSpaces(const std::wstring_view v)
{
    // first non-space char is not found (is npos)
    return v.find_first_not_of(L' ') == decltype(v)::npos;
}

void Renderer::_PaintBufferOutputHelper(_In_ IRenderEngine* const pEngine,
                                        TextBufferCellIterator it,
                                        const til::point target,
                                        const bool lineWrapped)
{
    auto globalInvert{ _renderSettings.GetRenderMode(RenderSettings::Mode::ScreenReversed) };

    // If we have valid data, let's figure out how to draw it.
    if (it)
    {
        // TODO: MSFT: 20961091 -  This is a perf issue. Instead of rebuilding this and allocing memory to hold the reinterpretation,
        // we should have an iterator/view adapter for the rendering.
        // That would probably also eliminate the RenderData needing to give us the entire TextBuffer as well...
        // Retrieve the iterator for one line of information.
        til::CoordType cols = 0;

        // Retrieve the first color.
        auto color = it->TextAttr();
        // Retrieve the first pattern id
        auto patternIds = _pData->GetPatternId(target);
        // Determine whether we're using a soft font.
        auto usingSoftFont = s_IsSoftFontChar(it->Chars(), _firstSoftFontChar, _lastSoftFontChar);

        // And hold the point where we should start drawing.
        auto screenPoint = target;

        // This outer loop will continue until we reach the end of the text we are trying to draw.
        while (it)
        {
            // Hold onto the current run color right here for the length of the outer loop.
            // We'll be changing the persistent one as we run through the inner loops to detect
            // when a run changes, but we will still need to know this color at the bottom
            // when we go to draw gridlines for the length of the run.
            const auto currentRunColor = color;

            // Hold onto the current pattern id as well
            const auto currentPatternId = patternIds;

            // Update the drawing brushes with our color and font usage.
            THROW_IF_FAILED(_UpdateDrawingBrushes(pEngine, currentRunColor, usingSoftFont, false));

            // Advance the point by however many columns we've just outputted and reset the accumulator.
            screenPoint.x += cols;
            cols = 0;

            // Hold onto the start of this run iterator and the target location where we started
            // in case we need to do some special work to paint the line drawing characters.
            const auto currentRunItStart = it;
            const auto currentRunTargetStart = screenPoint;

            // Ensure that our cluster vector is clear.
            _clusterBuffer.clear();

            // Reset our flag to know when we're in the special circumstance
            // of attempting to draw only the right-half of a two-column character
            // as the first item in our run.
            auto trimLeft = false;

            // Run contains wide character (>1 columns)
            auto containsWideCharacter = false;

            // This inner loop will accumulate clusters until the color changes.
            // When the color changes, it will save the new color off and break.
            // We also accumulate clusters according to regex patterns
            do
            {
                til::point thisPoint{ screenPoint.x + cols, screenPoint.y };
                const auto thisPointPatterns = _pData->GetPatternId(thisPoint);
                const auto thisUsingSoftFont = s_IsSoftFontChar(it->Chars(), _firstSoftFontChar, _lastSoftFontChar);
                const auto changedPatternOrFont = patternIds != thisPointPatterns || usingSoftFont != thisUsingSoftFont;
                if (color != it->TextAttr() || changedPatternOrFont)
                {
                    auto newAttr{ it->TextAttr() };
                    // foreground doesn't matter for runs of spaces (!)
                    // if we trick it . . . we call Paint far fewer times for cmatrix
                    if (!_IsAllSpaces(it->Chars()) || !newAttr.HasIdenticalVisualRepresentationForBlankSpace(color, globalInvert) || changedPatternOrFont)
                    {
                        color = newAttr;
                        patternIds = thisPointPatterns;
                        usingSoftFont = thisUsingSoftFont;
                        break; // vend this run
                    }
                }

                // Walk through the text data and turn it into rendering clusters.
                // Keep the columnCount as we go to improve performance over digging it out of the vector at the end.
                auto columnCount = it->Columns();

                // If we're on the first cluster to be added and it's marked as "trailing"
                // (a.k.a. the right half of a two column character), then we need some special handling.
                if (_clusterBuffer.empty() && it->DbcsAttr() == DbcsAttribute::Trailing)
                {
                    // Move left to the one so the whole character can be struck correctly.
                    --screenPoint.x;
                    // And tell the next function to trim off the left half of it.
                    trimLeft = true;
                    // And add one to the number of columns we expect it to take as we insert it.
                    ++columnCount;
                }

                if (columnCount > 1)
                {
                    containsWideCharacter = true;
                }

                // Advance the cluster and column counts.
                _clusterBuffer.emplace_back(it->Chars(), columnCount);
                it += std::max(it->Columns(), 1); // prevent infinite loop for no visible columns
                cols += columnCount;

            } while (it);

            // Do the painting.
            THROW_IF_FAILED(pEngine->PaintBufferLine({ _clusterBuffer.data(), _clusterBuffer.size() }, screenPoint, trimLeft, lineWrapped));

            // If we're allowed to do grid drawing, draw that now too (since it will be coupled with the color data)
            // We're only allowed to draw the grid lines under certain circumstances.
            if (_pData->IsGridLineDrawingAllowed())
            {
                // See GH: 803
                // If we found a wide character while we looped above, it's possible we skipped over the right half
                // attribute that could have contained different line information than the left half.
                if (containsWideCharacter)
                {
                    // Start from the original position in this run.
                    auto lineIt = currentRunItStart;
                    // Start from the original target in this run.
                    auto lineTarget = currentRunTargetStart;

                    // We need to go through the iterators again to ensure we get the lines associated with each
                    // exact column. The code above will condense two-column characters into one, but it is possible
                    // (like with the IME) that the line drawing characters will vary from the left to right half
                    // of a wider character.
                    // We could theoretically pre-pass for this in the loop above to be more efficient about walking
                    // the iterator, but I fear it would make the code even more confusing than it already is.
                    // Do that in the future if some WPR trace points you to this spot as super bad.
                    for (til::CoordType colsPainted = 0; colsPainted < cols; ++colsPainted, ++lineIt, ++lineTarget.x)
                    {
                        auto lines = lineIt->TextAttr();
                        _PaintBufferOutputGridLineHelper(pEngine, lines, 1, lineTarget);
                    }
                }
                else
                {
                    // If nothing exciting is going on, draw the lines in bulk.
                    _PaintBufferOutputGridLineHelper(pEngine, currentRunColor, cols, screenPoint);
                }
            }
        }
    }
}

// Method Description:
// - Generates a GridLines structure from the values in the
//      provided textAttribute
// Arguments:
// - textAttribute: the TextAttribute to generate GridLines from.
// Return Value:
// - a GridLineSet containing all the gridline info from the TextAttribute
GridLineSet Renderer::s_GetGridlines(const TextAttribute& textAttribute) noexcept
{
    // Convert console grid line representations into rendering engine enum representations.
    GridLineSet lines;

    if (textAttribute.IsTopHorizontalDisplayed())
    {
        lines.set(GridLines::Top);
    }

    if (textAttribute.IsBottomHorizontalDisplayed())
    {
        lines.set(GridLines::Bottom);
    }

    if (textAttribute.IsLeftVerticalDisplayed())
    {
        lines.set(GridLines::Left);
    }

    if (textAttribute.IsRightVerticalDisplayed())
    {
        lines.set(GridLines::Right);
    }

    if (textAttribute.IsCrossedOut())
    {
        lines.set(GridLines::Strikethrough);
    }

    const auto underlineStyle = textAttribute.GetUnderlineStyle();
    switch (underlineStyle)
    {
    case UnderlineStyle::NoUnderline:
        break;
    case UnderlineStyle::SinglyUnderlined:
        lines.set(GridLines::Underline);
        break;
    case UnderlineStyle::DoublyUnderlined:
        lines.set(GridLines::DoubleUnderline);
        break;
    case UnderlineStyle::CurlyUnderlined:
        lines.set(GridLines::CurlyUnderline);
        break;
    case UnderlineStyle::DottedUnderlined:
        lines.set(GridLines::DottedUnderline);
        break;
    case UnderlineStyle::DashedUnderlined:
        lines.set(GridLines::DashedUnderline);
        break;
    default:
        lines.set(GridLines::Underline);
        break;
    }

    if (textAttribute.IsHyperlink())
    {
        lines.set(GridLines::HyperlinkUnderline);
    }
    return lines;
}

// Routine Description:
// - Paint helper for primary buffer output function.
// - This particular helper sets up the various box drawing lines that can be inscribed around any character in the buffer (left, right, top, underline).
// - See also: All related helpers and buffer output functions.
// Arguments:
// - textAttribute - The line/box drawing attributes to use for this particular run.
// - cchLine - The length of both pwsLine and pbKAttrsLine.
// - coordTarget - The X/Y coordinate position in the buffer which we're attempting to start rendering from.
// Return Value:
// - <none>
void Renderer::_PaintBufferOutputGridLineHelper(_In_ IRenderEngine* const pEngine,
                                                const TextAttribute textAttribute,
                                                const size_t cchLine,
                                                const til::point coordTarget)
{
    // Convert console grid line representations into rendering engine enum representations.
    auto lines = Renderer::s_GetGridlines(textAttribute);

    // For now, we dash underline patterns and switch to regular underline on hover
    if (_isHoveredHyperlink(textAttribute) || _isInHoveredInterval(coordTarget))
    {
        lines.reset(GridLines::HyperlinkUnderline);
        lines.set(GridLines::Underline);
    }

    // Return early if there are no lines to paint.
    if (lines.any())
    {
        // Get the current foreground and underline colors to render the lines.
        const auto fg = _renderSettings.GetAttributeColors(textAttribute).first;
        const auto underlineColor = _renderSettings.GetAttributeUnderlineColor(textAttribute);
        // Draw the lines
        LOG_IF_FAILED(pEngine->PaintBufferGridLines(lines, fg, underlineColor, cchLine, coordTarget));
    }
}

bool Renderer::_isHoveredHyperlink(const TextAttribute& textAttribute) const noexcept
{
    return _hyperlinkHoveredId && _hyperlinkHoveredId == textAttribute.GetHyperlinkId();
}

bool Renderer::_isInHoveredInterval(const til::point coordTarget) const noexcept
{
    return _hoveredInterval &&
           _hoveredInterval->start <= coordTarget && coordTarget <= _hoveredInterval->stop &&
           _pData->GetPatternId(coordTarget).size() > 0;
}

// Routine Description:
// - Retrieve information about the cursor, and pack it into a CursorOptions
//   which the render engine can use for painting the cursor.
// - If the cursor is "off", or the cursor is out of bounds of the viewport,
//   this will return nullopt (indicating the cursor shouldn't be painted this
//   frame)
// Arguments:
// - <none>
// Return Value:
// - nullopt if the cursor is off or out-of-frame; otherwise, a CursorOptions
void Renderer::_updateCursorInfo()
{
    const auto& buffer = _pData->GetTextBuffer();
    const auto& cursor = buffer.GetCursor();
    const auto cursorPosition = cursor.GetPosition();
    auto coordCursor = cursorPosition; // Later this will be viewport-relative

    // GH#3166: Only draw the cursor if it's actually in the viewport. It
    // might be on the line that's in that partially visible row at the
    // bottom of the viewport, the space that's not quite a full line in
    // height. Since we don't draw that text, we shouldn't draw the cursor
    // there either.

    // The cursor is never rendered as double height, so we don't care about
    // the exact line rendition - only whether it's double width or not.
    const auto doubleWidth = buffer.IsDoubleWidthLine(coordCursor.y);
    const auto lineRendition = doubleWidth ? LineRendition::DoubleWidth : LineRendition::SingleWidth;

    // We need to convert the screen coordinates of the viewport to an
    // equivalent range of buffer cells, taking line rendition into account.
    const auto viewport = _viewport.ToInclusive();
    const auto view = ScreenToBufferLine(viewport, lineRendition);

    // Note that we allow the X coordinate to be outside the left border by 1 position,
    // because the cursor could still be visible if the focused character is double width.
    const auto xInRange = coordCursor.x >= view.left - 1 && coordCursor.x <= view.right;
    const auto yInRange = coordCursor.y >= view.top && coordCursor.y <= view.bottom;

    // Adjust cursor Y offset to viewport.
    // The viewport X offset is saved in the options and handled with a transform.
    coordCursor.y -= view.top;

    const auto cursorColor = _renderSettings.GetColorTableEntry(TextColor::CURSOR_COLOR);
    const auto useColor = cursorColor != INVALID_COLOR;

    // Update inhibitors based on whatever the VT parser (= client app) wants.
    AllowCursorVisibility(InhibitionSource::Client, cursor.IsVisible());
    AllowCursorBlinking(InhibitionSource::Client, cursor.IsBlinking());

    // If the buffer or cursor changed, turn the cursor on for the next cycle. This makes it
    // so that rapidly typing/printing keeps the cursor on continuously, which looks nicer.
    {
        const auto cursorBufferMutationId = buffer.GetLastMutationId();
        const auto cursorCursorMutationId = cursor.GetLastMutationId();

        if (_cursorBufferMutationId != cursorBufferMutationId || _cursorCursorMutationId != cursorCursorMutationId)
        {
            _cursorBufferMutationId = cursorBufferMutationId;
            _cursorCursorMutationId = cursorCursorMutationId;
            _cursorBlinkerOn = true;

            // We'll restart the timer below if there are no inhibitors.
            StopTimer(_cursorBlinker);
        }
    }

    if (_cursorVisibilityInhibitors.any() || _cursorBlinkingInhibitors.any())
    {
        StopTimer(_cursorBlinker);
    }
    else if (!IsTimerRunning(_cursorBlinker))
    {
        const auto actual = GetTimerInterval(_cursorBlinker);
        const auto expected = _pData->GetBlinkInterval();

        if (expected > TimerDuration::zero() && expected < TimerDuration::max())
        {
            if (expected != actual)
            {
                StartRepeatingTimer(_cursorBlinker, expected);
            }
        }
        else
        {
            // If blinking is disabled due to the OS settings, then we force-enable it.
            _cursorBlinkerOn = true;
        }
    }

    // If blinking is disabled, the cursor is always on.
    _cursorBlinkerOn |= _cursorBlinkingInhibitors.any();

    auto cursorHeight = cursor.GetSize();
    // Now adjust the height for the overwrite/insert mode. If we're in overwrite mode, IsDouble will be set.
    // When IsDouble is set, we either need to double the height of the cursor, or if it's already too big,
    // then we need to shrink it by half.
    if (cursor.IsDouble())
    {
        if (cursorHeight > 50) // 50 because 50 percent is half of 100 percent which is the max size.
        {
            cursorHeight >>= 1;
        }
        else
        {
            cursorHeight <<= 1;
        }
    }

    _currentCursorOptions.coordCursor = coordCursor;
    _currentCursorOptions.viewportLeft = viewport.left;
    _currentCursorOptions.lineRendition = lineRendition;
    _currentCursorOptions.ulCursorHeightPercent = cursorHeight;
    _currentCursorOptions.cursorPixelWidth = _pData->GetCursorPixelWidth();
    _currentCursorOptions.fIsDoubleWidth = buffer.GetRowByOffset(cursorPosition.y).DbcsAttrAt(cursorPosition.x) != DbcsAttribute::Single;
    _currentCursorOptions.cursorType = cursor.GetType();
    _currentCursorOptions.fUseColor = useColor;
    _currentCursorOptions.cursorColor = cursorColor;
    _currentCursorOptions.isVisible = !_cursorVisibilityInhibitors.any();
    _currentCursorOptions.isOn = _currentCursorOptions.isVisible && _cursorBlinkerOn;
    _currentCursorOptions.inViewport = xInRange && yInRange;
}

void Renderer::_invalidateCurrentCursor() const
{
    if (!_currentCursorOptions.inViewport || !_currentCursorOptions.isOn)
    {
        return;
    }

    const auto& buffer = _pData->GetTextBuffer();
    const auto view = buffer.GetSize();
    const auto coord = _currentCursorOptions.coordCursor;

    const auto lineRendition = _currentCursorOptions.lineRendition;
    const auto cursorWidth = _currentCursorOptions.fIsDoubleWidth ? 2 : 1;
    const auto x = coord.x - _viewport.Left();

    til::rect rect{ x, coord.y, x + cursorWidth, coord.y + 1 };
    rect = BufferToScreenLine(rect, lineRendition);

    if (view.TrimToViewport(&rect))
    {
        for (const auto pEngine : _engines)
        {
            LOG_IF_FAILED(pEngine->InvalidateCursor(&rect));
        }
    }
}

// If we had previously drawn a composition at the previous cursor position
// we need to invalidate the entire line because who knows what changed.
// (It's possible to figure that out, but not worth the effort right now.)
void Renderer::_invalidateOldComposition()
{
    if (!_compositionCache)
    {
        return;
    }

    _compositionCache.reset();

    if (!_currentCursorOptions.inViewport)
    {
        return;
    }

    const auto& buffer = _pData->GetTextBuffer();
    const auto view = buffer.GetSize();
    const auto coord = _currentCursorOptions.coordCursor;

    til::rect rect{ 0, coord.y, til::CoordTypeMax, coord.y + 1 };
    if (view.TrimToViewport(&rect))
    {
        for (const auto pEngine : _engines)
        {
            LOG_IF_FAILED(pEngine->Invalidate(&rect));
        }
    }
}

// Invalidate the line that the active TSF composition is on,
// so that _PaintBufferOutput() actually gets a chance to draw it.
void Renderer::_prepareNewComposition()
{
    if (_pData->GetActiveComposition().text.empty())
    {
        return;
    }

    auto& buffer = _pData->GetTextBuffer();
    const auto coordCursor = buffer.GetCursor().GetPosition();

    til::rect line{ 0, coordCursor.y, til::CoordTypeMax, coordCursor.y + 1 };
    if (_viewport.TrimToViewport(&line))
    {
        _viewport.ConvertToOrigin(&line);

        for (const auto pEngine : _engines)
        {
            LOG_IF_FAILED(pEngine->Invalidate(&line));
        }

        auto& scratch = buffer.GetScratchpadRow();
        const auto& activeComposition = _pData->GetActiveComposition();

        std::wstring_view text{ activeComposition.text };
        RowWriteState state{
            .columnLimit = buffer.GetRowByOffset(line.top).GetReadableColumnCount(),
        };

        state.text = text.substr(0, activeComposition.cursorPos);
        scratch.ReplaceText(state);
        const auto cursorOffset = state.columnEnd;

        state.text = text.substr(activeComposition.cursorPos);
        state.columnBegin = state.columnEnd;
        scratch.ReplaceText(state);

        // Ideally the text is inserted at the position of the cursor (`coordCursor`),
        // but if we got more text than fits into the remaining space until the end of the line,
        // then we'll insert the text aligned to the end of the line.
        const auto remaining = state.columnLimit - state.columnEnd;
        const auto beg = std::clamp(coordCursor.x, 0, remaining);

        const auto baseAttribute = buffer.GetRowByOffset(coordCursor.y).GetAttrByColumn(coordCursor.x);
        _compositionCache.emplace(til::point{ beg, coordCursor.y }, baseAttribute);

        // Fake-move the cursor to where it needs to be in the active composition.
        _currentCursorOptions.coordCursor.x = std::min(beg + cursorOffset, line.right - 1);
    }
}

// Routine Description:
// - Paint helper to draw the cursor within the buffer.
// Arguments:
// - engine - The render engine that we're targeting.
// Return Value:
// - <none>
void Renderer::_PaintCursor(_In_ IRenderEngine* const pEngine)
{
    if (_currentCursorOptions.inViewport && _currentCursorOptions.isVisible)
    {
        LOG_IF_FAILED(pEngine->PaintCursor(_currentCursorOptions));
    }
}

// Routine Description:
// - Retrieves info from the render data to prepare the engine with, before the
//   frame is drawn. Some renderers might want to use this information to affect
//   later drawing decisions.
//   * Namely, the DX renderer uses this to know the cursor position and state
//     before PaintCursor is called, so it can draw the cursor underneath the
//     text.
// Arguments:
// - engine - The render engine that we're targeting.
// Return Value:
// - S_OK if the engine prepared successfully, or a relevant error via HRESULT.
[[nodiscard]] HRESULT Renderer::_PrepareRenderInfo(_In_ IRenderEngine* const pEngine)
{
    RenderFrameInfo info;
    info.searchHighlights = _pData->GetSearchHighlights();
    info.searchHighlightFocused = _pData->GetSearchHighlightFocused();
    info.selectionSpans = _pData->GetSelectionSpans();
    info.selectionBackground = _renderSettings.GetColorTableEntry(TextColor::SELECTION_BACKGROUND);
    return pEngine->PrepareRenderInfo(std::move(info));
}

// Routine Description:
// - Paint helper to draw the selected area of the window.
// Arguments:
// - <none>
// Return Value:
// - <none>
void Renderer::_PaintSelection(_In_ IRenderEngine* const pEngine)
{
    try
    {
        std::span<const til::rect> dirtyAreas;
        LOG_IF_FAILED(pEngine->GetDirtyArea(dirtyAreas));

        for (auto&& dirtyRect : dirtyAreas)
        {
            for (const auto& rect : _lastSelectionRectsByViewport)
            {
                if (const auto rectCopy{ rect & dirtyRect })
                {
                    LOG_IF_FAILED(pEngine->PaintSelection(rectCopy));
                }
            }
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - Helper to convert the text attributes to actual RGB colors and update the rendering pen/brush within the rendering engine before the next draw operation.
// Arguments:
// - pEngine - Which engine is being updated
// - textAttributes - The 16 color foreground/background combination to set
// - usingSoftFont - Whether we're rendering characters from a soft font
// - isSettingDefaultBrushes - Alerts that the default brushes are being set which will
//                             impact whether or not to include the hung window/erase window brushes in this operation
//                             and can affect other draw state that wants to know the default color scheme.
//                             (Usually only happens when the default is changed, not when each individual color is swapped in a multi-color run.)
// Return Value:
// - <none>
[[nodiscard]] HRESULT Renderer::_UpdateDrawingBrushes(_In_ IRenderEngine* const pEngine,
                                                      const TextAttribute textAttributes,
                                                      const bool usingSoftFont,
                                                      const bool isSettingDefaultBrushes)
{
    // The last color needs to be each engine's responsibility. If it's local to this function,
    //      then on the next engine we might not update the color.
    return pEngine->UpdateDrawingBrushes(textAttributes, _renderSettings, _pData, usingSoftFont, isSettingDefaultBrushes);
}

// Routine Description:
// - Helper called before a majority of paint operations to scroll most of the previous frame into the appropriate
//   position before we paint the remaining invalid area.
// - Used to save drawing time/improve performance
// Arguments:
// - <none>
// Return Value:
// - <none>
[[nodiscard]] HRESULT Renderer::_PerformScrolling(_In_ IRenderEngine* const pEngine)
{
    return pEngine->ScrollFrame();
}

// Method Description:
// - Offsets all of the selection rectangles we might be holding onto
//   as the previously selected area. If the whole viewport scrolls,
//   we need to scroll these areas also to ensure they're invalidated
//   properly when the selection further changes.
// Arguments:
// - delta - The scroll delta
// Return Value:
// - <none> - Updates internal state instead.
void Renderer::_ScrollPreviousSelection(const til::point delta)
{
    if (delta != til::point{ 0, 0 })
    {
        for (auto& rc : _lastSelectionRectsByViewport)
        {
            rc += delta;
        }

        _currentCursorOptions.coordCursor += delta;
    }
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
    _engines.push_back(pEngine);
    _forceUpdateViewport = true;
}

void Renderer::RemoveRenderEngine(_In_ IRenderEngine* const pEngine)
{
    THROW_HR_IF_NULL(E_INVALIDARG, pEngine);

    std::erase_if(_engines, [=](IRenderEngine* e) {
        return pEngine == e;
    });
}

// Method Description:
// - Registers a callback for when the background color is changed
// Arguments:
// - pfn: the callback
// Return Value:
// - <none>
void Renderer::SetBackgroundColorChangedCallback(std::function<void()> pfn)
{
    _pfnBackgroundColorChanged = std::move(pfn);
}

// Method Description:
// - Registers a callback for when the frame colors have changed
// Arguments:
// - pfn: the callback
// Return Value:
// - <none>
void Renderer::SetFrameColorChangedCallback(std::function<void()> pfn)
{
    _pfnFrameColorChanged = std::move(pfn);
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

void Renderer::UpdateHyperlinkHoveredId(uint16_t id) noexcept
{
    _hyperlinkHoveredId = id;
    for (const auto pEngine : _engines)
    {
        pEngine->UpdateHyperlinkHoveredId(id);
    }
}

void Renderer::UpdateLastHoveredInterval(const std::optional<PointTree::interval>& newInterval)
{
    _hoveredInterval = newInterval;
}
