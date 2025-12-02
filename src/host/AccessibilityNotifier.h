// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

struct IRawElementProviderSimple;
typedef int EVENTID;

namespace Microsoft::Console
{
    // UIA and MSAA calls are both extraordinarily slow, so we got this
    // handy class to batch then up and emit them on a background thread.
    struct AccessibilityNotifier
    {
        AccessibilityNotifier();
        ~AccessibilityNotifier();

        AccessibilityNotifier(const AccessibilityNotifier&) = delete;
        AccessibilityNotifier& operator=(const AccessibilityNotifier&) = delete;

        // NOTE: It is assumed that you're holding the console lock when calling any of the member functions.
        // This class uses mutexes, but those are only for synchronizing with the worker threads.

        void Initialize(HWND hwnd, DWORD msaaDelay, DWORD uiaDelay) noexcept;
        void SetUIAProvider(IRawElementProviderSimple* provider) noexcept;

        void CursorChanged(til::point position, bool activeSelection) noexcept;
        void SelectionChanged() noexcept;
        bool WantsRegionChangedEvents() const noexcept;
        void RegionChanged(til::point begin, til::point end) noexcept;
        void ScrollBuffer(til::CoordType delta) noexcept;
        void ScrollViewport(til::point delta) noexcept;
        void Layout() noexcept;
        void ApplicationStart(DWORD pid) const noexcept;
        void ApplicationEnd(DWORD pid) const noexcept;

    private:
        // !!! NOTE !!!
        // * _emitEventsCallback assumes that this struct can be quickly initialized with memset(0).
        // * Members are intentionally left undefined so that we can create instances on the stack without initialization.
        struct State
        {
            // EVENT_CONSOLE_CARET / ConsoleControl(ConsoleSetCaretInfo)
            til::HugeCoordType eventConsoleCaretPositionX;
            til::HugeCoordType eventConsoleCaretPositionY;
            bool eventConsoleCaretSelecting;
            bool eventConsoleCaretPrimed;

            // EVENT_CONSOLE_UPDATE_REGION
            til::HugeCoordType eventConsoleUpdateRegionBeginX;
            til::HugeCoordType eventConsoleUpdateRegionBeginY;
            til::HugeCoordType eventConsoleUpdateRegionEndX;
            til::HugeCoordType eventConsoleUpdateRegionEndY;
            bool eventConsoleUpdateRegionPrimed;

            // EVENT_CONSOLE_UPDATE_SCROLL
            til::HugeCoordType eventConsoleUpdateScrollDeltaX;
            til::HugeCoordType eventConsoleUpdateScrollDeltaY;
            bool eventConsoleUpdateScrollPrimed;

            // EVENT_CONSOLE_LAYOUT
            bool eventConsoleLayoutPrimed;

            // UIA
            bool textSelectionChanged; // UIA_Text_TextSelectionChangedEventId
            bool textChanged; // UIA_Text_TextChangedEventId

            bool timerScheduled;
        };

        PTP_TIMER _createTimer(PTP_TIMER_CALLBACK callback) noexcept;
        void _timerSet() noexcept;
        static void NTAPI _timerEmitMSAA(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_TIMER timer) noexcept;
        void _emitEvents(State& msaa) const noexcept;
        static void _emitUIAEvent(IRawElementProviderSimple* provider, EVENTID id) noexcept;

        // The main window, used for NotifyWinEvent / ConsoleControl(ConsoleSetCaretInfo) calls.
        HWND _hwnd = nullptr;
        // The current UIA provider, if any.
        std::atomic<IRawElementProviderSimple*> _uiaProvider{ nullptr };
        // The timer object used to schedule debounced a11y events.
        // It's null if the delay is set to 0.
        wil::unique_threadpool_timer _timer;
        // The delay to use for MSAA/UIA events, in filetime units (100ns units).
        // The value will be negative because that's what SetThreadpoolTimerEx needs.
        int64_t _msaaDelay = 0;
        int64_t _uiaDelay = 0;
        // Depending on whether we have a UIA provider or not, this points to either _msaaDelay or _uiaDelay.
        FILETIME* _delay = nullptr;
        // The delay window to use for SetThreadpoolTimerEx, in milliseconds.
        DWORD _delayWindow = 0;
        // Whether MSAA and UIA are enabled.
        bool _msaaEnabled = false;
        bool _uiaEnabled = false;

        // _lock protects access to _state.
        wil::srwlock _lock;
        State _state;
    };
}
