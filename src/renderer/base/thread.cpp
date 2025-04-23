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
    TriggerTeardown();
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
        _enable.wait();

        // Between waiting on _hEvent and calling PaintFrame() there should be a minimal delay,
        // so that a key press progresses to a drawing operation as quickly as possible.
        // As such, we wait for the renderer to complete _before_ waiting on `_redraw`.
        renderer->WaitUntilCanRender();

        _redraw.wait();
        if (!_keepRunning.load(std::memory_order_relaxed))
        {
            break;
        }

        LOG_IF_FAILED(renderer->PaintFrame());
    }

    return S_OK;
}

void RenderThread::NotifyPaint() noexcept
{
    _redraw.SetEvent();
}

// Spawns a new rendering thread if none exists yet.
void RenderThread::EnablePainting() noexcept
{
    const auto guard = _threadMutex.lock_exclusive();

    _enable.SetEvent();

    if (!_thread)
    {
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
}

// This function is meant to only be called by `Renderer`. You should use `TriggerTeardown()` instead,
// even if you plan to call `EnablePainting()` later, because that ensures proper synchronization.
void RenderThread::DisablePainting() noexcept
{
    _enable.ResetEvent();
}

// Stops the rendering thread, and waits for it to finish.
void RenderThread::TriggerTeardown() noexcept
{
    const auto guard = _threadMutex.lock_exclusive();

    if (_thread)
    {
        // The render thread first waits for the event and then checks _keepRunning. By doing it
        // in reverse order here, we ensure that it's impossible for the render thread to miss this.
        _keepRunning.store(false, std::memory_order_relaxed);
        _redraw.SetEvent();
        _enable.SetEvent();

        WaitForSingleObject(_thread.get(), INFINITE);
        _thread.reset();
    }

    DisablePainting();
}
