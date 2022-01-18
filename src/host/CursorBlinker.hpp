/*
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- CursorBlinker.hpp
Abstract:
- Encapsulates all of the behavior needed to blink the cursor, and update the
    blink rate to account for different system settings.

Author(s):
- Mike Griese (migrie) Nov 2018
*/

namespace Microsoft::Console
{
    class CursorBlinker final
    {
    public:
        CursorBlinker();
        ~CursorBlinker();

        void FocusStart() const noexcept;
        void FocusEnd() const noexcept;

        void UpdateSystemMetrics() noexcept;
        void SettingsChanged() noexcept;
        void TimerRoutine(SCREEN_INFORMATION& ScreenInfo) const noexcept;

    private:
        void SetCaretTimer() const noexcept;
        void KillCaretTimer() const noexcept;

        wil::unique_threadpool_timer_nowait _timer;
        UINT _uCaretBlinkTime;
    };
}
