// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleWindow.hpp"

#include "../inc/ServiceLocator.hpp"
#include "../../types/inc/Viewport.hpp"

using namespace Microsoft::Console::Interactivity::OneCore;
using namespace Microsoft::Console::Types;

BOOL ConsoleWindow::EnableBothScrollBars() noexcept
{
    return FALSE;
}

int ConsoleWindow::UpdateScrollBar(bool /*isVertical*/,
                                   bool /*isAltBuffer*/,
                                   UINT /*pageSize*/,
                                   int /*maxSize*/,
                                   int /*viewportPosition*/) noexcept
{
    return 0;
}

bool ConsoleWindow::IsInFullscreen() const noexcept
{
    return true;
}

void ConsoleWindow::SetIsFullscreen(const bool /*fFullscreenEnabled*/) noexcept
{
}

void ConsoleWindow::ChangeViewport(const til::inclusive_rect& NewWindow)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    auto& ScreenInfo = gci.GetActiveOutputBuffer();
    const auto FontSize = ScreenInfo.GetScreenFontSize();

    auto pSelection = &Selection::Instance();
    pSelection->HideSelection();

    ScreenInfo.SetViewport(Viewport::FromInclusive(NewWindow), true);

    if (ServiceLocator::LocateGlobals().pRender != nullptr)
    {
        ServiceLocator::LocateGlobals().pRender->TriggerScroll();
    }

    pSelection->ShowSelection();

    ScreenInfo.UpdateScrollBars();
}

void ConsoleWindow::CaptureMouse() noexcept
{
}

BOOL ConsoleWindow::ReleaseMouse() noexcept
{
    return TRUE;
}

HWND ConsoleWindow::GetWindowHandle() const noexcept
{
    return nullptr;
}

void ConsoleWindow::SetOwner() noexcept
{
}

BOOL ConsoleWindow::GetCursorPosition(til::point* /*lpPoint*/) noexcept
{
    return FALSE;
}

BOOL ConsoleWindow::GetClientRectangle(til::rect* /*lpRect*/) noexcept
{
    return FALSE;
}

BOOL ConsoleWindow::MapRect(til::rect* /*lpRect*/) noexcept
{
    return 0;
}

BOOL ConsoleWindow::ConvertScreenToClient(til::point* /*lpPoint*/) noexcept
{
    return 0;
}

BOOL ConsoleWindow::SendNotifyBeep() const noexcept
{
    return Beep(800, 200);
}

BOOL ConsoleWindow::PostUpdateScrollBars() const noexcept
{
    return FALSE;
}

BOOL ConsoleWindow::PostUpdateWindowSize() const noexcept
{
    return FALSE;
}

void ConsoleWindow::UpdateWindowSize(const til::size /*coordSizeInChars*/) noexcept
{
}

void ConsoleWindow::UpdateWindowText() noexcept
{
}

void ConsoleWindow::HorizontalScroll(const WORD /*wScrollCommand*/, const WORD /*wAbsoluteChange*/) noexcept
{
}

void ConsoleWindow::VerticalScroll(const WORD /*wScrollCommand*/, const WORD /*wAbsoluteChange*/) noexcept
{
}

[[nodiscard]] HRESULT ConsoleWindow::SignalUia(_In_ EVENTID /*id*/) noexcept
{
    return E_NOTIMPL;
}

[[nodiscard]] HRESULT ConsoleWindow::UiaSetTextAreaFocus() noexcept
{
    return E_NOTIMPL;
}

til::rect ConsoleWindow::GetWindowRect() const noexcept
{
    return {};
}
