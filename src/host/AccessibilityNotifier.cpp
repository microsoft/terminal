// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "AccessibilityNotifier.h"

#include "../types/inc/convert.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"

using namespace Microsoft::Console;
using namespace Microsoft::Console::Interactivity;

template<typename U, typename T>
constexpr U castSaturated(T v) noexcept
{
    static_assert(sizeof(U) <= sizeof(T), "use this for narrowing");
    constexpr T min = std::numeric_limits<U>::min();
    constexpr T max = std::numeric_limits<U>::max();
    return gsl::narrow_cast<U>(v < min ? min : (v > max ? max : v));
}

AccessibilityNotifier::AccessibilityNotifier()
{
    // Mirrors _timerEmitMSAA / _timerEmitUIA
    memset(&_state, 0, sizeof(_state));
}

AccessibilityNotifier::~AccessibilityNotifier()
{
    SetUIAProvider(nullptr);
}

void AccessibilityNotifier::Initialize(HWND hwnd, DWORD msaaDelay, DWORD uiaDelay) noexcept
{
    _hwnd = hwnd;

    // delay=INFINITE is intended to disable events completely, but realistically,
    // even a delay of 1s makes little sense. So, the cut-off was set to 10s.
    if (msaaDelay < 10000 && hwnd)
    {
        _msaaEnabled = true;

        // msaaDelay=0 makes all events synchronous. That's how
        // it used to work and has a huge performance impact.
        if (msaaDelay != 0)
        {
            // Convert from milliseconds to 100-nanosecond intervals.
            // Negative values indicate relative time.
            _msaaDelay = static_cast<int64_t>(msaaDelay) * -10000;
        }
    }

    if (uiaDelay < 10000)
    {
        _uiaEnabled = true;

        if (uiaDelay != 0)
        {
            _uiaDelay = static_cast<int64_t>(uiaDelay) * -10000;
        }
    }

    if (_msaaDelay || _uiaDelay)
    {
        _timer.reset(_createTimer(&_timerEmitMSAA));
    }

    // Triggers the computation of _delay and _delayWindow.
    SetUIAProvider(nullptr);
}

void AccessibilityNotifier::SetUIAProvider(IRawElementProviderSimple* provider) noexcept
{
    // If UIA events are disabled, don't set _uiaProvider either.
    // It would trigger unnecessary work.
    //
    // NOTE: We check this before the assert() below so that unit tests don't trigger the assert.
    if (!_uiaEnabled)
    {
        return;
    }

    // NOTE: The assumption is that you're holding the console lock when calling any of the member functions.
    // This is why we can safely update these members (no worker thread is running nor can be scheduled).
    assert(ServiceLocator::LocateGlobals().getConsoleInformation().IsConsoleLocked());

    // Of course we must ensure our precious provider object doesn't go away.
    if (provider)
    {
        provider->AddRef();
    }

    const auto old = _uiaProvider.exchange(provider, std::memory_order_relaxed);

    // Before we can release the old object, we must ensure it's not in use by a worker thread.
    if (_timer)
    {
        WaitForThreadpoolTimerCallbacks(_timer.get(), TRUE);
    }

    if (old)
    {
        old->Release();
    }

    // Update the delay. If UIA is enabled now, use the UIA delay.
    // Note that a delay of 0 means "no delay" and we signal that as _delay=nullptr.
    //
    // NOTE: We don't set a second timer just for UIA, because some applications like NVDA
    // listen to both MSAA and UIA events. If they don't arrive approximately together,
    // they'll be announced as separate events, which breaks announcements.
    if (const auto delay = provider ? &_uiaDelay : &_msaaDelay; *delay == 0)
    {
        _delay = nullptr;
        _delayWindow = 0;
    }
    else
    {
        static_assert(sizeof(FILETIME) == sizeof(_uiaDelay));
        _delay = reinterpret_cast<FILETIME*>(delay);
        // Set the delay window to 1/5th of the delay, but in milliseconds.
        _delayWindow = gsl::narrow_cast<DWORD>(std::max<int64_t>(0, *delay / (5 * -10000)));
    }

    // If we canceled the timer, reschedule it.
    if (_state.timerScheduled)
    {
        _state.timerScheduled = false;

        // Of course there's no point to schedule it if there isn't a provider.
        if (provider)
        {
            _timerSet();
        }
    }
}

// Emits EVENT_CONSOLE_CARET, indicating the new cursor position.
// `rectangle` is the cursor rectangle in buffer coordinates (rows/columns)
// `flags` can be either CONSOLE_CARET_SELECTION _or_ CONSOLE_CARET_VISIBLE (not a bitfield)
//
//
// It then also Calls ConsoleControl() with ConsoleSetCaretInfo, which goes through the kernel sets
// cciConsole on the HWND and then raises EVENT_OBJECT_LOCATIONCHANGE with OBJID_CARET, INDEXID_CONTAINER.
// The cciConsole information is then used by GetGUIThreadInfo() to populate hwndCaret and rcCaret.
// Unfortunately there's no way to know whether anyone even needs this information so we always raise this.
void AccessibilityNotifier::CursorChanged(til::point position, bool activeSelection) noexcept
{
    const auto uiaEnabled = _uiaProvider.load(std::memory_order_relaxed) != nullptr;

    // Can't check for IsWinEventHookInstalled(EVENT_CONSOLE_CARET),
    // because we need to emit a ConsoleControl() call regardless.
    if (_msaaEnabled || uiaEnabled)
    {
        const auto guard = _lock.lock_exclusive();

        if (_msaaEnabled)
        {
            _state.eventConsoleCaretPositionX = position.x;
            _state.eventConsoleCaretPositionY = position.y;
            _state.eventConsoleCaretSelecting = activeSelection;
            _state.eventConsoleCaretPrimed = true;
        }

        if (uiaEnabled)
        {
            _state.textSelectionChanged = true;
        }

        _timerSet();
    }
}

void AccessibilityNotifier::SelectionChanged() noexcept
{
    if (_uiaProvider.load(std::memory_order_relaxed))
    {
        const auto guard = _lock.lock_exclusive();

        _state.textSelectionChanged = true;

        _timerSet();
    }
}

bool AccessibilityNotifier::WantsRegionChangedEvents() const noexcept
{
    // See RegionChanged().
    return (_msaaEnabled && IsWinEventHookInstalled(EVENT_CONSOLE_UPDATE_REGION)) ||
           (_uiaProvider.load(std::memory_order_relaxed) != nullptr);
}

// Emits EVENT_CONSOLE_UPDATE_REGION, the region of the console that changed.
// `end` is expected to be an inclusive coordinate.
void AccessibilityNotifier::RegionChanged(til::point begin, til::point end) noexcept
{
    if (begin > end)
    {
        return;
    }

    const auto msaa = _msaaEnabled && IsWinEventHookInstalled(EVENT_CONSOLE_UPDATE_REGION);
    const auto uia = _uiaProvider.load(std::memory_order_relaxed) != nullptr;

    if (!msaa && !uia)
    {
        return;
    }

    const auto guard = _lock.lock_exclusive();

    if (msaa)
    {
        const til::HugeCoordType begX = begin.x;
        const til::HugeCoordType begY = begin.y;
        const til::HugeCoordType endX = end.x;
        const til::HugeCoordType endY = end.y;

        const auto primed = _state.eventConsoleUpdateRegionPrimed;

        // Initialize the region (if !primed) or extend the region to the union of old and new.
        if (!primed || begY < _state.eventConsoleUpdateRegionBeginY || (begY == _state.eventConsoleUpdateRegionBeginY && begX < _state.eventConsoleUpdateRegionBeginX))
        {
            _state.eventConsoleUpdateRegionBeginX = begX;
            _state.eventConsoleUpdateRegionBeginY = begY;
            _state.eventConsoleUpdateRegionPrimed = true;
        }
        if (!primed || endY > _state.eventConsoleUpdateRegionEndY || (endY == _state.eventConsoleUpdateRegionEndY && endX > _state.eventConsoleUpdateRegionEndX))
        {
            _state.eventConsoleUpdateRegionEndX = endX;
            _state.eventConsoleUpdateRegionEndY = endY;
            _state.eventConsoleUpdateRegionPrimed = true;
        }
    }

    if (uia)
    {
        _state.textChanged = true;
    }

    _timerSet();
}

// Emits EVENT_CONSOLE_UPDATE_SCROLL. Specific to buffer scrolls and
// allows us to adjust previously cached buffer coordinates accordingly.
void AccessibilityNotifier::ScrollBuffer(til::CoordType delta) noexcept
{
    if (_msaaEnabled && IsWinEventHookInstalled(EVENT_CONSOLE_UPDATE_SCROLL))
    {
        const auto guard = _lock.lock_exclusive();

        // They say accessibility is hard, but then they design EVENT_CONSOLE_UPDATE_SCROLL
        // to count _both_ viewport scrolls _and_ buffer scrolls as the same thing,
        // making the information carried by the event completely useless. Don't ask me.
        //
        // Fun fact: conhost "v2" (Windows 10+) would raise EVENT_CONSOLE_UPDATE_SCROLL events every
        // time ScrollConsoleScreenBuffer is called. People ask me why I'm balding. They don't know.
        _state.eventConsoleUpdateScrollDeltaY += delta;
        _state.eventConsoleUpdateScrollPrimed = true;

        if (_state.eventConsoleCaretPrimed)
        {
            _state.eventConsoleCaretPositionY += delta;
        }

        if (_state.eventConsoleUpdateRegionPrimed)
        {
            _state.eventConsoleUpdateRegionBeginY += delta;
            _state.eventConsoleUpdateRegionEndY += delta;
        }

        _timerSet();
    }
}

// Emits EVENT_CONSOLE_UPDATE_SCROLL. Specific to viewport scrolls.
void AccessibilityNotifier::ScrollViewport(til::point delta) noexcept
{
    if (_msaaEnabled && IsWinEventHookInstalled(EVENT_CONSOLE_UPDATE_SCROLL))
    {
        const auto guard = _lock.lock_exclusive();

        _state.eventConsoleUpdateScrollDeltaX += delta.x;
        _state.eventConsoleUpdateScrollDeltaY += delta.y;
        _state.eventConsoleUpdateScrollPrimed = true;

        _timerSet();
    }
}

// Emits EVENT_CONSOLE_LAYOUT. Documentation just states "The console layout has changed."
// but it's absolutely unclear what that even means. Try to emit it when the scrollbar
// position or window size has changed... I guess.
void AccessibilityNotifier::Layout() noexcept
{
    if (_msaaEnabled && IsWinEventHookInstalled(EVENT_CONSOLE_LAYOUT))
    {
        const auto guard = _lock.lock_exclusive();

        _state.eventConsoleLayoutPrimed = true;

        _timerSet();
    }
}

void AccessibilityNotifier::ApplicationStart(DWORD pid) const noexcept
{
    if (_msaaEnabled)
    {
        const auto cc = ServiceLocator::LocateConsoleControl<IConsoleControl>();
        cc->NotifyWinEvent(EVENT_CONSOLE_START_APPLICATION, _hwnd, pid, 0);
    }
}

void AccessibilityNotifier::ApplicationEnd(DWORD pid) const noexcept
{
    if (_msaaEnabled)
    {
        const auto cc = ServiceLocator::LocateConsoleControl<IConsoleControl>();
        cc->NotifyWinEvent(EVENT_CONSOLE_END_APPLICATION, _hwnd, pid, 0);
    }
}

PTP_TIMER AccessibilityNotifier::_createTimer(PTP_TIMER_CALLBACK callback) noexcept
{
    return THROW_LAST_ERROR_IF_NULL(CreateThreadpoolTimer(callback, this, nullptr));
}

void AccessibilityNotifier::_timerSet() noexcept
{
    if (!_delay)
    {
        _emitEvents(_state);
    }
    else if (!_state.timerScheduled)
    {
        _state.timerScheduled = true;
        SetThreadpoolTimerEx(_timer.get(), _delay, 0, _delayWindow);
    }
}

void NTAPI AccessibilityNotifier::_timerEmitMSAA(PTP_CALLBACK_INSTANCE, PVOID context, PTP_TIMER) noexcept
{
    const auto self = static_cast<AccessibilityNotifier*>(context);
    State state;

    // Make a copy of _state, because UIA and MSAA are very slow (up to 1ms per call).
    // Holding a lock while _emitEventsCallback would mean that the IO thread can't proceed.
    //
    // The only concern I have is whether calling SetThreadpoolTimerEx() again on
    // _timer while we're still executing will properly schedule another run.
    // The docs say to read the "Remarks" and the remarks just don't clarify it. Great.
    // FWIW we can't just create two timer objects since that may (theoretically)
    // just end up with two callbacks running at the same time = same problem.
    {
        const auto guard = self->_lock.lock_exclusive();

        // What we want to do is
        //   state = self->_state;
        //   self->_state = {};
        // MSVC optimizes the first line with SIMD, but fails to do so for the second line.
        // This forces us to use memset. memcpy is used for consistency.
        static_assert(std::is_trivially_copyable_v<State>);
        memcpy(&state, &self->_state, sizeof(State));
        memset(&self->_state, 0, sizeof(State));
    }

    self->_emitEvents(state);
}

void AccessibilityNotifier::_emitEvents(State& state) const noexcept
{
    const auto cc = ServiceLocator::LocateConsoleControl<IConsoleControl>();
    const auto provider = _uiaProvider.load(std::memory_order_relaxed);
    LONG updateRegionBeg = 0;
    LONG updateRegionEnd = 0;
    LONG updateSimpleCharAndAttr = 0;
    LONG caretPosition = 0;
    std::optional<CONSOLE_CARET_INFO> caretInfo;

    // vvv   Prepare any information we need   vvv
    //
    // Because NotifyWinEvent and UiaRaiseAutomationEvent are _very_ slow,
    // and the following needs the console lock, we do it separately first.

    if (state.eventConsoleUpdateRegionPrimed || state.eventConsoleCaretPrimed)
    {
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        gci.LockConsole();

        if (state.eventConsoleUpdateRegionPrimed)
        {
            const auto regionBegX = castSaturated<SHORT>(state.eventConsoleUpdateRegionBeginX);
            const auto regionBegY = castSaturated<SHORT>(state.eventConsoleUpdateRegionBeginY);
            const auto regionEndX = castSaturated<SHORT>(state.eventConsoleUpdateRegionEndX);
            const auto regionEndY = castSaturated<SHORT>(state.eventConsoleUpdateRegionEndY);
            updateRegionBeg = MAKELONG(regionBegX, regionBegY);
            updateRegionEnd = MAKELONG(regionEndX, regionEndY);

            // Historically we'd emit an EVENT_CONSOLE_UPDATE_SIMPLE event for single-char updates,
            // but in the 30 years since, the way fast software is written has changed:
            // We now have plenty CPU power but the speed of light is still the same.
            // It's much more important to batch events to avoid NotifyWinEvent's latency problems.
            // EVENT_CONSOLE_UPDATE_SIMPLE is not trivially batch-able, so we should avoid it.
            //
            // That said, NVDA is currently a very popular screen reader for Windows.
            // IF you set its "Windows Console support" to "Legacy" AND disable
            // "Use enhanced typed character support in legacy Windows Console when available"
            // then it will purely rely on these WinEvents for accessibility.
            //
            // In this case it assumes that EVENT_CONSOLE_UPDATE_REGION is regular output
            // and that EVENT_CONSOLE_UPDATE_SIMPLE is keyboard input (FYI: don't do this).
            // The problem now is that it doesn't announce any EVENT_CONSOLE_UPDATE_REGION
            // events where beg == end (i.e. a single character change).
            //
            // Unfortunately, the same is partially true for Microsoft's own Narrator.
            if (gci.HasActiveOutputBuffer() && updateRegionBeg == updateRegionEnd)
            {
                auto& screenInfo = gci.GetActiveOutputBuffer();
                auto& buffer = screenInfo.GetTextBuffer();
                const auto& row = buffer.GetRowByOffset(regionBegY);
                const auto glyph = row.GlyphAt(regionBegX);
                const auto attr = row.GetAttrByColumn(regionBegX);
                updateSimpleCharAndAttr = MAKELONG(Utf16ToUcs2(glyph), attr.GetLegacyAttributes());
            }
        }

        if (state.eventConsoleCaretPrimed)
        {
            const auto caretX = castSaturated<SHORT>(state.eventConsoleCaretPositionX);
            const auto caretY = castSaturated<SHORT>(state.eventConsoleCaretPositionY);
            caretPosition = MAKELONG(caretX, caretY);

            // Convert the buffer position to the equivalent screen coordinates
            // required by CONSOLE_CARET_INFO, taking line rendition into account.
            if (gci.HasActiveOutputBuffer())
            {
                auto& screenInfo = gci.GetActiveOutputBuffer();
                auto& buffer = screenInfo.GetTextBuffer();
                const auto position = buffer.BufferToScreenPosition({ caretX, caretY });
                const auto viewport = screenInfo.GetViewport();
                const auto fontSize = screenInfo.GetScreenFontSize();
                const auto left = (position.x - viewport.Left()) * fontSize.width;
                const auto top = (position.y - viewport.Top()) * fontSize.height;
                caretInfo.emplace(CONSOLE_CARET_INFO{
                    .hwnd = _hwnd,
                    .rc = RECT{
                        left,
                        top,
                        left + fontSize.width,
                        top + fontSize.height,
                    },
                });
            }
        }

        gci.UnlockConsole();
    }

    // vvv   Raise events now   vvv
    //
    // NOTE: When typing in a cooked read prompt (e.g. cmd.exe), the following events
    // are historically raised synchronously/immediately in the listed order:
    // * NotifyWinEvent(EVENT_CONSOLE_UPDATE_SIMPLE)
    // * UiaRaiseAutomationEvent(UIA_Text_TextChangedEventId)
    //
    // Then, between 0-530ms later, via the now removed blink timer routine,
    // the following was raised asynchronously:
    // * ConsoleControl(ConsoleSetCaretInfo)
    // * NotifyWinEvent(EVENT_CONSOLE_CARET)
    // * UiaRaiseAutomationEvent(UIA_Text_TextSelectionChangedEventId)

    if (state.eventConsoleUpdateRegionPrimed)
    {
        if (updateSimpleCharAndAttr)
        {
            cc->NotifyWinEvent(EVENT_CONSOLE_UPDATE_SIMPLE, _hwnd, updateRegionBeg, updateSimpleCharAndAttr);
        }
        else
        {
            cc->NotifyWinEvent(EVENT_CONSOLE_UPDATE_REGION, _hwnd, updateRegionBeg, updateRegionEnd);
        }

        state.eventConsoleUpdateRegionBeginX = 0;
        state.eventConsoleUpdateRegionBeginY = 0;
        state.eventConsoleUpdateRegionEndX = 0;
        state.eventConsoleUpdateRegionEndY = 0;
        state.eventConsoleUpdateRegionPrimed = false;
    }

    if (state.textChanged)
    {
        _emitUIAEvent(provider, UIA_Text_TextChangedEventId);
        state.textChanged = false;
    }

    if (state.eventConsoleCaretPrimed)
    {
        if (caretInfo)
        {
            cc->Control(ControlType::ConsoleSetCaretInfo, &*caretInfo, sizeof(*caretInfo));
        }

        // There's no need to check for IsWinEventHookInstalled,
        // because NotifyWinEvent is very fast if no event is installed.
        //
        // Technically, CONSOLE_CARET_SELECTION and CONSOLE_CARET_VISIBLE are bitflags,
        // however Microsoft's _own_ example code for these assumes that they're an
        // enumeration and also assumes that a value of 0 (= invisible cursor) is invalid.
        // So, we just pretend as if the cursor is always visible.
        const auto flags = state.eventConsoleCaretSelecting ? CONSOLE_CARET_SELECTION : CONSOLE_CARET_VISIBLE;
        cc->NotifyWinEvent(EVENT_CONSOLE_CARET, _hwnd, flags, caretPosition);

        state.eventConsoleCaretPositionX = 0;
        state.eventConsoleCaretPositionY = 0;
        state.eventConsoleCaretSelecting = false;
        state.eventConsoleCaretPrimed = false;
    }

    if (state.textSelectionChanged)
    {
        _emitUIAEvent(provider, UIA_Text_TextSelectionChangedEventId);
        state.textSelectionChanged = false;
    }

    if (state.eventConsoleUpdateScrollPrimed)
    {
        const auto dx = castSaturated<LONG>(state.eventConsoleUpdateScrollDeltaX);
        const auto dy = castSaturated<LONG>(state.eventConsoleUpdateScrollDeltaY);

        cc->NotifyWinEvent(EVENT_CONSOLE_UPDATE_SCROLL, _hwnd, dx, dy);

        state.eventConsoleUpdateScrollDeltaX = 0;
        state.eventConsoleUpdateScrollDeltaY = 0;
        state.eventConsoleUpdateScrollPrimed = false;
    }

    if (state.eventConsoleLayoutPrimed)
    {
        cc->NotifyWinEvent(EVENT_CONSOLE_LAYOUT, _hwnd, 0, 0);
        state.eventConsoleLayoutPrimed = false;
    }
}

void AccessibilityNotifier::_emitUIAEvent(IRawElementProviderSimple* provider, EVENTID id) noexcept
{
    if (provider)
    {
        LOG_IF_FAILED(UiaRaiseAutomationEvent(provider, id));
    }
}
