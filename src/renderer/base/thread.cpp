// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "thread.hpp"

#include "renderer.hpp"

#pragma hdrstop

using namespace Microsoft::Console::Render;

RenderThread::RenderThread(Renderer* renderer) :
    renderer(renderer)
{
}

RenderThread::~RenderThread()
{
    WaitForPaintCompletionAndDisable();
}

DWORD WINAPI RenderThread::s_ThreadProc(_In_ LPVOID lpParameter)
{
    const auto pContext = static_cast<RenderThread*>(lpParameter);
    return pContext->_ThreadProc();
}

DWORD WINAPI RenderThread::_ThreadProc()
{
    while (true)
    {
        // Between waiting on _hEvent and calling PaintFrame() there should be a minimal delay,
        // so that a key press progresses to a drawing operation as quickly as possible.
        // As such, we wait for the renderer to complete _before_ waiting on _hEvent.
        renderer->WaitUntilCanRender();

        _event.wait();
        if (!_keepRunning.load(std::memory_order_relaxed))
        {
            break;
        }

        const auto hr = renderer->PaintFrame();
        if (hr == S_FALSE)
        {
            break;
        }

        LOG_IF_FAILED(hr);
    }

    return S_OK;
}

void RenderThread::NotifyPaint() noexcept
{
    _event.SetEvent();
}

// Spawns a new rendering thread if none exists yet.
void RenderThread::EnablePainting() noexcept
{
    const auto guard = _threadMutex.lock_exclusive();

    // Check if we already got a thread.
    if (_thread)
    {
        return;
    }

    _keepRunning.store(true, std::memory_order_relaxed);

    _thread.reset(CreateThread(nullptr, 0, s_ThreadProc, this, 0, nullptr));
    THROW_LAST_ERROR_IF(!_thread);

    // SetThreadDescription only works on 1607 and higher. If we cannot find it,
    // then it's no big deal. Just skip setting the description.
    const auto func = GetProcAddressByFunctionDeclaration(GetModuleHandleW(L"kernel32.dll"), SetThreadDescription);
    if (func)
    {
        LOG_IF_FAILED(func(_thread.get(), L"Rendering Output Thread"));
    }
}

// Stops the rendering thread, and waits for it to finish.
void RenderThread::WaitForPaintCompletionAndDisable() noexcept
{
    const auto guard = _threadMutex.lock_exclusive();

    _keepRunning.store(false, std::memory_order_relaxed);
    _event.SetEvent(); // To allow the thread to reach the exit point.

    if (_thread)
    {
        WaitForSingleObject(_thread.get(), INFINITE);
        _thread.reset();
    }
}
