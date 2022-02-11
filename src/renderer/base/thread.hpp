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
        RenderThread();
        ~RenderThread();

        [[nodiscard]] HRESULT Initialize(Renderer* const pRendererParent) noexcept;

        void NotifyPaint() noexcept;
        void EnablePainting() noexcept;
        void DisablePainting() noexcept;
        void WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs) noexcept;

    private:
        static DWORD WINAPI s_ThreadProc(_In_ LPVOID lpParameter);
        DWORD WINAPI _ThreadProc();

        HANDLE _hThread;
        HANDLE _hEvent;

        HANDLE _hPaintEnabledEvent;
        HANDLE _hPaintCompletedEvent;

        Renderer* _pRenderer; // Non-ownership pointer

        bool _fKeepRunning;
        std::atomic<bool> _fNextFrameRequested;
        std::atomic<bool> _fWaiting;
    };
}
