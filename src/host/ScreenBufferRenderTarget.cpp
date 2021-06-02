// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"
#include "ScreenBufferRenderTarget.hpp"
#include "../interactivity/inc/ServiceLocator.hpp"

using Microsoft::Console::Interactivity::ServiceLocator;
ScreenBufferRenderTarget::ScreenBufferRenderTarget(SCREEN_INFORMATION& owner) :
    _owner{ owner }
{
}

void ScreenBufferRenderTarget::TriggerRedraw(const Microsoft::Console::Types::Viewport& region)
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerRedraw(region);
    }
}

void ScreenBufferRenderTarget::TriggerRedraw(const COORD* const pcoord)
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerRedraw(pcoord);
    }
}

void ScreenBufferRenderTarget::TriggerRedrawCursor(const COORD* const pcoord)
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerRedrawCursor(pcoord);
    }
}

void ScreenBufferRenderTarget::TriggerRedrawAll()
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerRedrawAll();
    }
}

void ScreenBufferRenderTarget::TriggerTeardown() noexcept
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerTeardown();
    }
}

void ScreenBufferRenderTarget::TriggerSelection()
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerSelection();
    }
}

void ScreenBufferRenderTarget::TriggerScroll()
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerScroll();
    }
}

void ScreenBufferRenderTarget::TriggerScroll(const COORD* const pcoordDelta)
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerScroll(pcoordDelta);
    }
}

void ScreenBufferRenderTarget::TriggerCircling()
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerCircling();
    }
}

void ScreenBufferRenderTarget::TriggerTitleChange()
{
    auto* pRenderer = ServiceLocator::LocateGlobals().pRender;
    const auto* pActive = &ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    if (pRenderer != nullptr && pActive == &_owner)
    {
        pRenderer->TriggerTitleChange();
    }
}
