// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "WindowMetrics.hpp"
#include "ConIoSrvComm.hpp"

#include "..\..\renderer\wddmcon\wddmconrenderer.hpp"

#include "..\inc\ServiceLocator.hpp"

#pragma hdrstop

// Default metrics for when in headless mode.
#define HEADLESS_FONT_SIZE_WIDTH (8)
#define HEADLESS_FONT_SIZE_HEIGHT (12)
#define HEADLESS_DISPLAY_SIZE_WIDTH (80)
#define HEADLESS_DISPLAY_SIZE_HEIGHT (25)

using namespace Microsoft::Console::Interactivity::OneCore;

RECT WindowMetrics::GetMinClientRectInPixels()
{
    ConIoSrvComm* Server;

    NTSTATUS Status;

    // We need to always return something viable for this call,
    // so by default, set the font and display size to our headless
    // constants.
    // If we get information from the Server, great. We'll calculate
    // the values for that at the end.
    // If we don't... then at least we have a non-zero rectangle.
    COORD FontSize = { 0 };
    FontSize.X = HEADLESS_FONT_SIZE_WIDTH;
    FontSize.Y = HEADLESS_FONT_SIZE_HEIGHT;

    RECT DisplaySize = { 0 };
    DisplaySize.right = HEADLESS_DISPLAY_SIZE_WIDTH;
    DisplaySize.bottom = HEADLESS_DISPLAY_SIZE_HEIGHT;

    CD_IO_FONT_SIZE FontSizeIoctl = { 0 };
    CD_IO_DISPLAY_SIZE DisplaySizeIoctl = { 0 };

    USHORT DisplayMode;

    // Fetch a reference to the Console IO Server.
    Server = ServiceLocator::LocateInputServices<ConIoSrvComm>();

    // Figure out what kind of display we are using.
    DisplayMode = Server->GetDisplayMode();

    // Note on status propagation:
    //
    // The IWindowMetrics contract was extracted from the original methods in
    // the Win32 Window class, which have no failure modes. However, in the case
    // of their OneCore implementations, because getting this information
    // requires reaching out to the Console IO Server if display output occurs
    // via BGFX, there is a possibility of failure where the server may be
    // unreachable. As a result, Get[Max|Min]ClientRectInPixels call
    // SetLastError in their OneCore implementations to reflect whether their
    // return value is accurate.

    switch (DisplayMode)
    {
    case CIS_DISPLAY_MODE_BGFX:
    {
        // TODO: MSFT: 10916072 This requires switching to kernel mode and calling
        //       BgkGetConsoleState. The call's result can be cached, though that
        //       might be a problem for plugging/unplugging monitors or perhaps
        //       across KVM sessions.

        Status = Server->RequestGetDisplaySize(&DisplaySizeIoctl);

        if (NT_SUCCESS(Status))
        {
            Status = Server->RequestGetFontSize(&FontSizeIoctl);

            if (NT_SUCCESS(Status))
            {
                DisplaySize.top = 0;
                DisplaySize.left = 0;
                DisplaySize.bottom = DisplaySizeIoctl.Height;
                DisplaySize.right = DisplaySizeIoctl.Width;

                FontSize.X = (SHORT)FontSizeIoctl.Width;
                FontSize.Y = (SHORT)FontSizeIoctl.Height;
            }
        }
        else
        {
            SetLastError(Status);
        }
    }
    break;

    case CIS_DISPLAY_MODE_DIRECTX:
    {
        LOG_IF_FAILED(Server->pWddmConEngine->GetFontSize(&FontSize));
        DisplaySize = Server->pWddmConEngine->GetDisplaySize();
    }
    break;

    case CIS_DISPLAY_MODE_NONE:
    {
        // When in headless mode and using EMS (Emergency Management
        // Services), ensure that the buffer isn't zero-sized by
        // using the default values.
    }
    break;
    }

    // The result is expected to be in pixels, not rows/columns.
    DisplaySize.right *= FontSize.X;
    DisplaySize.bottom *= FontSize.Y;

    return DisplaySize;
}

RECT WindowMetrics::GetMaxClientRectInPixels()
{
    // OneCore consoles only have one size and cannot be resized.
    return GetMinClientRectInPixels();
}
