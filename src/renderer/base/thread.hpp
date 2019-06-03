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

#include "..\inc\IRenderer.hpp"
#include "..\inc\IRenderThread.hpp"

namespace Microsoft::Console::Render
{
    class RenderThread final : public IRenderThread
    {
    public:
        RenderThread();
        virtual ~RenderThread() override;

        [[nodiscard]] HRESULT Initialize(_In_ IRenderer* const pRendererParent) noexcept;

        void NotifyPaint() override;

        void EnablePainting() override;
        void WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs) override;

    private:
        static DWORD WINAPI s_ThreadProc(_In_ LPVOID lpParameter);
        DWORD WINAPI _ThreadProc();

        static DWORD const s_FrameLimitMilliseconds = 8;

        HANDLE _hThread;
        HANDLE _hEvent;

        HANDLE _hPaintEnabledEvent;
        HANDLE _hPaintCompletedEvent;

        IRenderer* _pRenderer; // Non-ownership pointer

        bool _fKeepRunning;
    };
}
