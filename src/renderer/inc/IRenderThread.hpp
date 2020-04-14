/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- IRenderThread.hpp

Abstract:
- an abstraction for all the actions a render thread needs to perform.

Author(s):
- Mike Griese (migrie) 16 Jan 2019
--*/

#pragma once
namespace Microsoft::Console::Render
{
    class IRenderThread
    {
    public:
        virtual ~IRenderThread() = 0;
        IRenderThread(const IRenderThread&) = default;
        IRenderThread(IRenderThread&&) = default;
        IRenderThread& operator=(const IRenderThread&) = default;
        IRenderThread& operator=(IRenderThread&&) = default;

        virtual void NotifyPaint() = 0;
        virtual void EnablePainting() = 0;
        virtual void WaitForPaintCompletionAndDisable(const DWORD dwTimeoutMs) = 0;

    protected:
        IRenderThread() = default;
    };

    inline Microsoft::Console::Render::IRenderThread::~IRenderThread(){};
}
