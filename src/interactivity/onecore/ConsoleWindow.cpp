// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "ConsoleWindow.hpp"

#include "..\inc\ServiceLocator.hpp"
#include "..\..\types\inc\Viewport.hpp"

using namespace Microsoft::Console::Interactivity::OneCore;
using namespace Microsoft::Console::Types;

BOOL ConsoleWindow::EnableBothScrollBars()
{
    return FALSE;
}

int ConsoleWindow::UpdateScrollBar(bool /*isVertical*/,
                                   bool /*isAltBuffer*/,
                                   UINT /*pageSize*/,
                                   int /*maxSize*/,
                                   int /*viewportPosition*/)
{
    return 0;
}

bool ConsoleWindow::IsInFullscreen() const
{
    return true;
}

void ConsoleWindow::SetIsFullscreen(bool const /*fFullscreenEnabled*/)
{
}

void ConsoleWindow::ChangeViewport(const SMALL_RECT NewWindow)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    SCREEN_INFORMATION& ScreenInfo = gci.GetActiveOutputBuffer();
    COORD const FontSize = ScreenInfo.GetScreenFontSize();

    Selection* pSelection = &Selection::Instance();
    pSelection->HideSelection();

    ScreenInfo.SetViewport(Viewport::FromInclusive(NewWindow), true);

    if (ServiceLocator::LocateGlobals().pRender != nullptr)
    {
        ServiceLocator::LocateGlobals().pRender->TriggerScroll();
    }

    pSelection->ShowSelection();

    ScreenInfo.UpdateScrollBars();
}

void ConsoleWindow::CaptureMouse()
{
}

BOOL ConsoleWindow::ReleaseMouse()
{
    return TRUE;
}

HWND ConsoleWindow::GetWindowHandle() const
{
    return nullptr;
}

void ConsoleWindow::SetOwner()
{
}

BOOL ConsoleWindow::GetCursorPosition(LPPOINT /*lpPoint*/)
{
    return FALSE;
}

BOOL ConsoleWindow::GetClientRectangle(LPRECT /*lpRect*/)
{
    return FALSE;
}

int ConsoleWindow::MapPoints(LPPOINT /*lpPoints*/, UINT /*cPoints*/)
{
    return 0;
}

BOOL ConsoleWindow::ConvertScreenToClient(LPPOINT /*lpPoint*/)
{
    return 0;
}

BOOL ConsoleWindow::SendNotifyBeep() const
{
    return Beep(800, 200);
}

BOOL ConsoleWindow::PostUpdateScrollBars() const
{
    return FALSE;
}

BOOL ConsoleWindow::PostUpdateWindowSize() const
{
    return FALSE;
}

void ConsoleWindow::UpdateWindowSize(COORD const /*coordSizeInChars*/)
{
}

void ConsoleWindow::UpdateWindowText()
{
}

void ConsoleWindow::HorizontalScroll(const WORD /*wScrollCommand*/, const WORD /*wAbsoluteChange*/)
{
}

void ConsoleWindow::VerticalScroll(const WORD /*wScrollCommand*/, const WORD /*wAbsoluteChange*/)
{
}

[[nodiscard]] HRESULT ConsoleWindow::SignalUia(_In_ EVENTID /*id*/)
{
    return E_NOTIMPL;
}

[[nodiscard]] HRESULT ConsoleWindow::UiaSetTextAreaFocus()
{
    return E_NOTIMPL;
}

RECT ConsoleWindow::GetWindowRect() const
{
    RECT rc = { 0 };
    return rc;
}
