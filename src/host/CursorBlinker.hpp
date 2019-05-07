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

        void FocusStart();
        void FocusEnd();

        void UpdateSystemMetrics();
        void SettingsChanged();
        void TimerRoutine(SCREEN_INFORMATION& ScreenInfo);

    private:
        // These use Timer Queues:
        // https://msdn.microsoft.com/en-us/library/windows/desktop/ms687003(v=vs.85).aspx
        HANDLE _hCaretBlinkTimer; // timer used to periodically blink the cursor
        HANDLE _hCaretBlinkTimerQueue; // timer queue where the blink timer lives
        UINT _uCaretBlinkTime;
        void SetCaretTimer();
        void KillCaretTimer();
    };
}
