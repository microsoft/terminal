/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Thread.hpp

Abstract:
- This is the definition of our rendering thread designed to throttle and compartmentalize drawing operations.

Author(s):
- Michael Niksa (MiNiksa) Feb 2016
--*/

#pragma once

namespace Microsoft::Console::Render
{
    class Renderer;

    class RenderThread
    {
    public:
        RenderThread(Renderer* renderer);
        ~RenderThread();

        void NotifyPaint() noexcept;
        void EnablePainting() noexcept;
        void DisablePainting() noexcept;
        void TriggerTeardown() noexcept;

    private:
        static DWORD WINAPI s_ThreadProc(_In_ LPVOID lpParameter);
        DWORD WINAPI _ThreadProc();

        Renderer* renderer;
        wil::slim_event_manual_reset _enable;
        wil::slim_event_auto_reset _redraw;
        wil::srwlock _threadMutex;
        wil::unique_handle _thread;
        std::atomic<bool> _keepRunning{ false };
    };
}
