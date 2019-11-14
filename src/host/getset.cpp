// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "getset.h"

#include "_output.h"
#include "_stream.h"
#include "output.h"
#include "dbcs.h"
#include "handle.h"
#include "misc.h"
#include "cmdline.h"

#include "../types/inc/convert.hpp"
#include "../types/inc/viewport.hpp"

#include "ApiRoutines.h"

#include "..\interactivity\inc\ServiceLocator.hpp"

#pragma hdrstop

// The following mask is used to test for valid text attributes.
#define VALID_TEXT_ATTRIBUTES (FG_ATTRS | BG_ATTRS | META_ATTRS)

#define INPUT_MODES (ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT | ENABLE_ECHO_INPUT | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT | ENABLE_VIRTUAL_TERMINAL_INPUT)
#define OUTPUT_MODES (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN | ENABLE_LVB_GRID_WORLDWIDE)
#define PRIVATE_MODES (ENABLE_INSERT_MODE | ENABLE_QUICK_EDIT_MODE | ENABLE_AUTO_POSITION | ENABLE_EXTENDED_FLAGS)

using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::Interactivity;

// Routine Description:
// - Retrieves the console input mode (settings that apply when manipulating the input buffer)
// Arguments:
// - context - The input buffer concerned
// - mode - Receives the mode flags set
void ApiRoutines::GetConsoleInputModeImpl(InputBuffer& context, ULONG& mode) noexcept
{
    try
    {
        Telemetry::Instance().LogApiCall(Telemetry::ApiCall::GetConsoleMode);
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        mode = context.InputMode;

        if (WI_IsFlagSet(gci.Flags, CONSOLE_USE_PRIVATE_FLAGS))
        {
            WI_SetFlag(mode, ENABLE_EXTENDED_FLAGS);
            WI_SetFlagIf(mode, ENABLE_INSERT_MODE, gci.GetInsertMode());
            WI_SetFlagIf(mode, ENABLE_QUICK_EDIT_MODE, WI_IsFlagSet(gci.Flags, CONSOLE_QUICK_EDIT_MODE));
            WI_SetFlagIf(mode, ENABLE_AUTO_POSITION, WI_IsFlagSet(gci.Flags, CONSOLE_AUTO_POSITION));
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - Retrieves the console output mode (settings that apply when manipulating the output buffer)
// Arguments:
// - context - The output buffer concerned
// - mode - Receives the mode flags set
void ApiRoutines::GetConsoleOutputModeImpl(SCREEN_INFORMATION& context, ULONG& mode) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        mode = context.GetActiveBuffer().OutputMode;
    }
    CATCH_LOG();
}

// Routine Description:
// - Retrieves the number of console event items in the input queue right now
// Arguments:
// - context - The input buffer concerned
// - event - The count of events in the queue
// Return Value:
//  - S_OK or math failure.
[[nodiscard]] HRESULT ApiRoutines::GetNumberOfConsoleInputEventsImpl(const InputBuffer& context, ULONG& events) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        const auto readyEventCount = context.GetNumberOfReadyEvents();
        RETURN_IF_FAILED(SizeTToULong(readyEventCount, &events));

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves metadata associated with the output buffer (size, default colors, etc.)
// Arguments:
// - context - The output buffer concerned
// - data - Receives structure filled with metadata about the output buffer
void ApiRoutines::GetConsoleScreenBufferInfoExImpl(const SCREEN_INFORMATION& context,
                                                   CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        data.bFullscreenSupported = FALSE; // traditional full screen with the driver support is no longer supported.
        // see MSFT: 19918103
        // Make sure to use the active buffer here. There are clients that will
        //      use WINDOW_SIZE_EVENTs as a signal to then query the console
        //      with GetConsoleScreenBufferInfoEx to get the actual viewport
        //      size.
        // If they're in the alt buffer, then when they query in that way, the
        //      value they'll get is the main buffer's size, which isn't updated
        //      until we switch back to it.
        context.GetActiveBuffer().GetScreenBufferInformation(&data.dwSize,
                                                             &data.dwCursorPosition,
                                                             &data.srWindow,
                                                             &data.wAttributes,
                                                             &data.dwMaximumWindowSize,
                                                             &data.wPopupAttributes,
                                                             data.ColorTable);
        // Callers of this function expect to receive an exclusive rect, not an inclusive one.
        data.srWindow.Right += 1;
        data.srWindow.Bottom += 1;
    }
    CATCH_LOG();
}

// Routine Description:
// - Retrieves information about the console cursor's display state
// Arguments:
// - context - The output buffer concerned
// - size - The size as a percentage of the total possible height (0-100 for percentages).
// - isVisible - Whether the cursor is displayed or hidden
void ApiRoutines::GetConsoleCursorInfoImpl(const SCREEN_INFORMATION& context,
                                           ULONG& size,
                                           bool& isVisible) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        size = context.GetActiveBuffer().GetTextBuffer().GetCursor().GetSize();
        isVisible = context.GetTextBuffer().GetCursor().IsVisible();
    }
    CATCH_LOG();
}

// Routine Description:
// - Retrieves information about the selected area in the console
// Arguments:
// - consoleSelectionInfo - contains flags, anchors, and area to describe selection area
void ApiRoutines::GetConsoleSelectionInfoImpl(CONSOLE_SELECTION_INFO& consoleSelectionInfo) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        const auto& selection = Selection::Instance();
        if (selection.IsInSelectingState())
        {
            consoleSelectionInfo.dwFlags = selection.GetPublicSelectionFlags();

            WI_SetFlag(consoleSelectionInfo.dwFlags, CONSOLE_SELECTION_IN_PROGRESS);

            consoleSelectionInfo.dwSelectionAnchor = selection.GetSelectionAnchor();
            consoleSelectionInfo.srSelection = selection.GetSelectionRectangle();
        }
        else
        {
            ZeroMemory(&consoleSelectionInfo, sizeof(consoleSelectionInfo));
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - Retrieves the number of buttons on the mouse as reported by the system
// Arguments:
// - buttons - Count of buttons
void ApiRoutines::GetNumberOfConsoleMouseButtonsImpl(ULONG& buttons) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        buttons = ServiceLocator::LocateSystemConfigurationProvider()->GetNumberOfMouseButtons();
    }
    CATCH_LOG();
}

// Routine Description:
// - Retrieves information about a known font based on index
// Arguments:
// - context - The output buffer concerned
// - index - We only accept 0 now as we don't keep a list of fonts in memory.
// - size - The X by Y pixel size of the font
// Return Value:
// - S_OK, E_INVALIDARG or code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::GetConsoleFontSizeImpl(const SCREEN_INFORMATION& context,
                                                          const DWORD index,
                                                          COORD& size) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        if (index == 0)
        {
            // As of the November 2015 renderer system, we only have a single font at index 0.
            size = context.GetActiveBuffer().GetCurrentFont().GetUnscaledSize();
            return S_OK;
        }
        else
        {
            // Invalid font is 0,0 with STATUS_INVALID_PARAMETER
            size = { 0 };
            return E_INVALIDARG;
        }
    }
    CATCH_RETURN();
}

// Routine Description:
// - Retrieves information about the console cursor's display state
// Arguments:
// - context - The output buffer concerned
// - isForMaximumWindowSize - Returns the maximum number of characters in the largest window size if true. Otherwise, it's the size of the font.
// - consoleFontInfoEx - structure containing font information like size, family, weight, etc.
// Return Value:
// - S_OK, string copy failure code or code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::GetCurrentConsoleFontExImpl(const SCREEN_INFORMATION& context,
                                                               const bool isForMaximumWindowSize,
                                                               CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        const SCREEN_INFORMATION& activeScreenInfo = context.GetActiveBuffer();

        COORD WindowSize;
        if (isForMaximumWindowSize)
        {
            WindowSize = activeScreenInfo.GetMaxWindowSizeInCharacters();
        }
        else
        {
            WindowSize = activeScreenInfo.GetCurrentFont().GetUnscaledSize();
        }
        consoleFontInfoEx.dwFontSize = WindowSize;

        consoleFontInfoEx.nFont = 0;

        const FontInfo& fontInfo = activeScreenInfo.GetCurrentFont();
        consoleFontInfoEx.FontFamily = fontInfo.GetFamily();
        consoleFontInfoEx.FontWeight = fontInfo.GetWeight();

        RETURN_IF_FAILED(fontInfo.FillLegacyNameBuffer(gsl::make_span(consoleFontInfoEx.FaceName)));

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the current font to be used for drawing
// Arguments:
// - context - The output buffer concerned
// - isForMaximumWindowSize - Obsolete.
// - consoleFontInfoEx - structure containing font information like size, family, weight, etc.
// Return Value:
// - S_OK, string copy failure code or code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetCurrentConsoleFontExImpl(IConsoleOutputObject& context,
                                                               const bool /*isForMaximumWindowSize*/,
                                                               const CONSOLE_FONT_INFOEX& consoleFontInfoEx) noexcept
{
    try
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        SCREEN_INFORMATION& activeScreenInfo = context.GetActiveBuffer();

        WCHAR FaceName[ARRAYSIZE(consoleFontInfoEx.FaceName)];
        RETURN_IF_FAILED(StringCchCopyW(FaceName, ARRAYSIZE(FaceName), consoleFontInfoEx.FaceName));

        FontInfo fi(FaceName,
                    gsl::narrow_cast<unsigned char>(consoleFontInfoEx.FontFamily),
                    consoleFontInfoEx.FontWeight,
                    consoleFontInfoEx.dwFontSize,
                    gci.OutputCP);

        // TODO: MSFT: 9574827 - should this have a failure case?
        activeScreenInfo.UpdateFont(&fi);

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the input mode for the console
// Arguments:
// - context - The input buffer concerned
// - mode - flags that change behavior of the buffer
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleInputModeImpl(InputBuffer& context, const ULONG mode) noexcept
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        if (WI_IsAnyFlagSet(mode, PRIVATE_MODES))
        {
            WI_SetFlag(gci.Flags, CONSOLE_USE_PRIVATE_FLAGS);

            WI_UpdateFlag(gci.Flags, CONSOLE_QUICK_EDIT_MODE, WI_IsFlagSet(mode, ENABLE_QUICK_EDIT_MODE));
            WI_UpdateFlag(gci.Flags, CONSOLE_AUTO_POSITION, WI_IsFlagSet(mode, ENABLE_AUTO_POSITION));

            const bool PreviousInsertMode = gci.GetInsertMode();
            gci.SetInsertMode(WI_IsFlagSet(mode, ENABLE_INSERT_MODE));
            if (gci.GetInsertMode() != PreviousInsertMode)
            {
                gci.GetActiveOutputBuffer().SetCursorDBMode(false);
                if (gci.HasPendingCookedRead())
                {
                    gci.CookedReadData().SetInsertMode(gci.GetInsertMode());
                }
            }
        }
        else
        {
            WI_ClearFlag(gci.Flags, CONSOLE_USE_PRIVATE_FLAGS);
        }

        context.InputMode = mode;
        WI_ClearAllFlags(context.InputMode, PRIVATE_MODES);

        // NOTE: For compatibility reasons, we need to set the modes and then return the error codes, not the other way around
        //       as might be expected.
        //       This is a bug from a long time ago and some applications depend on this functionality to operate properly.
        //       ---
        //       A prime example of this is that PSReadline module in Powershell will set the invalid mode 0x1e4
        //       which includes 0x4 for ECHO_INPUT but turns off 0x2 for LINE_INPUT. This is invalid, but PSReadline
        //       relies on it to properly receive the ^C printout and make a new line when the user presses Ctrl+C.
        {
            // Flags we don't understand are invalid.
            RETURN_HR_IF(E_INVALIDARG, WI_IsAnyFlagSet(mode, ~(INPUT_MODES | PRIVATE_MODES)));

            // ECHO on with LINE off is invalid.
            RETURN_HR_IF(E_INVALIDARG, WI_IsFlagSet(mode, ENABLE_ECHO_INPUT) && WI_IsFlagClear(mode, ENABLE_LINE_INPUT));
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the output mode for the console
// Arguments:
// - context - The output buffer concerned
// - mode - flags that change behavior of the buffer
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleOutputModeImpl(SCREEN_INFORMATION& context, const ULONG mode) noexcept
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Flags we don't understand are invalid.
        RETURN_HR_IF(E_INVALIDARG, WI_IsAnyFlagSet(mode, ~OUTPUT_MODES));

        SCREEN_INFORMATION& screenInfo = context.GetActiveBuffer();
        const DWORD dwOldMode = screenInfo.OutputMode;
        const DWORD dwNewMode = mode;

        screenInfo.OutputMode = dwNewMode;

        // if we're moving from VT on->off
        if (WI_IsFlagClear(dwNewMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING) &&
            WI_IsFlagSet(dwOldMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            // jiggle the handle
            screenInfo.GetStateMachine().ResetState();
            screenInfo.ClearTabStops();
        }
        // if we're moving from VT off->on
        else if (WI_IsFlagSet(dwNewMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING) &&
                 WI_IsFlagClear(dwOldMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            screenInfo.SetDefaultVtTabStops();
        }

        gci.SetVirtTermLevel(WI_IsFlagSet(dwNewMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING) ? 1 : 0);
        gci.SetAutomaticReturnOnNewline(WI_IsFlagSet(screenInfo.OutputMode, DISABLE_NEWLINE_AUTO_RETURN) ? false : true);
        gci.SetGridRenderingAllowedWorldwide(WI_IsFlagSet(screenInfo.OutputMode, ENABLE_LVB_GRID_WORLDWIDE));

        // if we changed rendering modes then redraw the output buffer,
        // but only do this if we're not in conpty mode.
        if (!gci.IsInVtIoMode() &&
            (WI_IsFlagSet(dwNewMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING) != WI_IsFlagSet(dwOldMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING) ||
             WI_IsFlagSet(dwNewMode, ENABLE_LVB_GRID_WORLDWIDE) != WI_IsFlagSet(dwOldMode, ENABLE_LVB_GRID_WORLDWIDE)))
        {
            auto* pRender = ServiceLocator::LocateGlobals().pRender;
            if (pRender)
            {
                pRender->TriggerRedrawAll();
            }
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the given output buffer as the active one
// Arguments:
// - context - The output buffer concerned
void ApiRoutines::SetConsoleActiveScreenBufferImpl(SCREEN_INFORMATION& newContext) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        SetActiveScreenBuffer(newContext.GetActiveBuffer());
    }
    CATCH_LOG();
}

// Routine Description:
// - Clears all items out of the input buffer queue
// Arguments:
// - context - The input buffer concerned
void ApiRoutines::FlushConsoleInputBuffer(InputBuffer& context) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        context.Flush();
    }
    CATCH_LOG();
}

// Routine Description:
// - Gets the largest possible window size in characters.
// Arguments:
// - context - The output buffer concerned
// - size - receives the size in character count (rows/columns)
void ApiRoutines::GetLargestConsoleWindowSizeImpl(const SCREEN_INFORMATION& context,
                                                  COORD& size) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        const SCREEN_INFORMATION& screenInfo = context.GetActiveBuffer();

        size = screenInfo.GetLargestWindowSizeInCharacters();
    }
    CATCH_LOG();
}

// Routine Description:
// - Sets the size of the output buffer (screen buffer) in rows/columns
// Arguments:
// - context - The output buffer concerned
// - size - size in character rows and columns
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleScreenBufferSizeImpl(SCREEN_INFORMATION& context,
                                                                  const COORD size) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        SCREEN_INFORMATION& screenInfo = context.GetActiveBuffer();

        // see MSFT:17415266
        // We only really care about the minimum window size if we have a head.
        if (!ServiceLocator::LocateGlobals().IsHeadless())
        {
            COORD const coordMin = screenInfo.GetMinWindowSizeInCharacters();
            // clang-format off
            // Make sure requested screen buffer size isn't smaller than the window.
            RETURN_HR_IF(E_INVALIDARG, (size.X < screenInfo.GetViewport().Width() ||
                                        size.Y < screenInfo.GetViewport().Height() ||
                                        size.Y < coordMin.Y ||
                                        size.X < coordMin.X));
            // clang-format on
        }

        // Ensure the requested size isn't larger than we can handle in our data type.
        RETURN_HR_IF(E_INVALIDARG, (size.X == SHORT_MAX || size.Y == SHORT_MAX));

        // Only do the resize if we're actually changing one of the dimensions
        COORD const coordScreenBufferSize = screenInfo.GetBufferSize().Dimensions();
        if (size.X != coordScreenBufferSize.X || size.Y != coordScreenBufferSize.Y)
        {
            RETURN_NTSTATUS(screenInfo.ResizeScreenBuffer(size, TRUE));
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets metadata information on the output buffer
// Arguments:
// - context - The output buffer concerned
// - data - metadata information structure like buffer size, viewport size, colors, and more.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleScreenBufferInfoExImpl(SCREEN_INFORMATION& context,
                                                                    const CONSOLE_SCREEN_BUFFER_INFOEX& data) noexcept
{
    try
    {
        // clang-format off
        RETURN_HR_IF(E_INVALIDARG, (data.dwSize.X == 0 ||
                                    data.dwSize.Y == 0 ||
                                    data.dwSize.X == SHRT_MAX ||
                                    data.dwSize.Y == SHRT_MAX));
        // clang-format on

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        Globals& g = ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();

        const COORD coordScreenBufferSize = context.GetBufferSize().Dimensions();
        const COORD requestedBufferSize = data.dwSize;
        if (requestedBufferSize.X != coordScreenBufferSize.X ||
            requestedBufferSize.Y != coordScreenBufferSize.Y)
        {
            CommandLine& commandLine = CommandLine::Instance();

            commandLine.Hide(FALSE);

            LOG_IF_FAILED(context.ResizeScreenBuffer(data.dwSize, TRUE));

            commandLine.Show();
        }
        const COORD newBufferSize = context.GetBufferSize().Dimensions();

        gci.SetColorTable(data.ColorTable, ARRAYSIZE(data.ColorTable));

        context.SetDefaultAttributes({ data.wAttributes }, { data.wPopupAttributes });

        const Viewport requestedViewport = Viewport::FromExclusive(data.srWindow);

        COORD NewSize = requestedViewport.Dimensions();
        // If we have a window, clamp the requested viewport to the max window size
        if (!ServiceLocator::LocateGlobals().IsHeadless())
        {
            NewSize.X = std::min(NewSize.X, data.dwMaximumWindowSize.X);
            NewSize.Y = std::min(NewSize.Y, data.dwMaximumWindowSize.Y);
        }

        // If wrap text is on, then the window width must be the same size as the buffer width
        if (gci.GetWrapText())
        {
            NewSize.X = newBufferSize.X;
        }

        if (NewSize.X != context.GetViewport().Width() ||
            NewSize.Y != context.GetViewport().Height())
        {
            context.SetViewportSize(&NewSize);

            IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
            if (pWindow != nullptr)
            {
                pWindow->UpdateWindowSize(NewSize);
            }
        }

        // Despite the fact that this API takes in a srWindow for the viewport, it traditionally actually doesn't set
        //  anything using that member - for moving the viewport, you need SetConsoleWindowInfo
        //  (see https://msdn.microsoft.com/en-us/library/windows/desktop/ms686125(v=vs.85).aspx and DoSrvSetConsoleWindowInfo)
        // Note that it also doesn't set cursor position.

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the cursor position in the given output buffer
// Arguments:
// - context - The output buffer concerned
// - position - The X/Y (row/column) position in the buffer to place the cursor
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleCursorPositionImpl(SCREEN_INFORMATION& context,
                                                                const COORD position) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

        auto& buffer = context.GetActiveBuffer();

        const COORD coordScreenBufferSize = buffer.GetBufferSize().Dimensions();
        // clang-format off
        RETURN_HR_IF(E_INVALIDARG, (position.X >= coordScreenBufferSize.X ||
                                    position.Y >= coordScreenBufferSize.Y ||
                                    position.X < 0 ||
                                    position.Y < 0));
        // clang-format on

        // MSFT: 15813316 - Try to use this SetCursorPosition call to inherit the cursor position.
        RETURN_IF_FAILED(gci.GetVtIo()->SetCursorPosition(position));

        RETURN_IF_NTSTATUS_FAILED(buffer.SetCursorPosition(position, true));

        LOG_IF_FAILED(ConsoleImeResizeCompStrView());

        // Attempt to "snap" the viewport to the cursor position. If the cursor
        // is not in the current viewport, we'll try and move the viewport so
        // that the cursor is visible.
        // microsoft/terminal#1222 - Use the "virtual" viewport here, so that
        // when the console is in terminal-scrolling mode, the viewport snaps
        // back to the virtual viewport's location.
        const SMALL_RECT currentViewport = gci.IsTerminalScrolling() ?
                                               buffer.GetVirtualViewport().ToInclusive() :
                                               buffer.GetViewport().ToInclusive();
        COORD delta{ 0 };
        {
            if (currentViewport.Left > position.X)
            {
                delta.X = position.X - currentViewport.Left;
            }
            else if (currentViewport.Right < position.X)
            {
                delta.X = position.X - currentViewport.Right;
            }

            if (currentViewport.Top > position.Y)
            {
                delta.Y = position.Y - currentViewport.Top;
            }
            else if (currentViewport.Bottom < position.Y)
            {
                delta.Y = position.Y - currentViewport.Bottom;
            }
        }

        COORD newWindowOrigin{ 0 };
        newWindowOrigin.X = currentViewport.Left + delta.X;
        newWindowOrigin.Y = currentViewport.Top + delta.Y;
        // SetViewportOrigin will worry about clamping these values to the
        // buffer for us.
        RETURN_IF_NTSTATUS_FAILED(buffer.SetViewportOrigin(true, newWindowOrigin, true));

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets metadata on the cursor
// Arguments:
// - context - The output buffer concerned
// - size - Height percentage of the displayed cursor (when visible)
// - isVisible - Whether or not the cursor should be displayed
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleCursorInfoImpl(SCREEN_INFORMATION& context,
                                                            const ULONG size,
                                                            const bool isVisible) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // If more than 100% or less than 0% cursor height, reject it.
        RETURN_HR_IF(E_INVALIDARG, (size > 100 || size == 0));

        context.SetCursorInformation(size, isVisible);

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the viewport/window information for displaying a portion of the output buffer visually
// Arguments:
// - context - The output buffer concerned
// - isAbsolute - Coordinates are based on the entire screen buffer (origin 0,0) if true.
//              - If false, coordinates are a delta from the existing viewport position
// - windowRect - Updated viewport rectangle information
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleWindowInfoImpl(SCREEN_INFORMATION& context,
                                                            const bool isAbsolute,
                                                            const SMALL_RECT& windowRect) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        Globals& g = ServiceLocator::LocateGlobals();
        SMALL_RECT Window = windowRect;

        if (!isAbsolute)
        {
            SMALL_RECT currentViewport = context.GetViewport().ToInclusive();
            Window.Left += currentViewport.Left;
            Window.Right += currentViewport.Right;
            Window.Top += currentViewport.Top;
            Window.Bottom += currentViewport.Bottom;
        }

        RETURN_HR_IF(E_INVALIDARG, (Window.Right < Window.Left || Window.Bottom < Window.Top));

        COORD NewWindowSize;
        NewWindowSize.X = (SHORT)(CalcWindowSizeX(Window));
        NewWindowSize.Y = (SHORT)(CalcWindowSizeY(Window));

        // see MSFT:17415266
        // If we have a actual head, we care about the maximum size the window can be.
        // if we're headless, not so much. However, GetMaxWindowSizeInCharacters
        //      will only return the buffer size, so we can't use that to clip the arg here.
        // So only clip the requested size if we're not headless
        if (!g.IsHeadless())
        {
            COORD const coordMax = context.GetMaxWindowSizeInCharacters();
            RETURN_HR_IF(E_INVALIDARG, (NewWindowSize.X > coordMax.X || NewWindowSize.Y > coordMax.Y));
        }
        else if (g.getConsoleInformation().IsInVtIoMode())
        {
            // SetViewportRect doesn't cause the buffer to resize. Manually resize the buffer.
            RETURN_IF_NTSTATUS_FAILED(context.ResizeScreenBuffer(Viewport::FromInclusive(Window).Dimensions(), false));
        }

        // Even if it's the same size, we need to post an update in case the scroll bars need to go away.
        context.SetViewport(Viewport::FromInclusive(Window), true);
        if (context.IsActiveScreenBuffer())
        {
            // TODO: MSFT: 9574827 - shouldn't we be looking at or at least logging the failure codes here? (Or making them non-void?)
            context.PostUpdateWindowSize();
            WriteToScreen(context, context.GetViewport());
        }
        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Moves a portion of text from one part of the output buffer to another
// Arguments:
// - context - The output buffer concerned
// - source - The rectangular region to copy from
// - target - The top left corner of the destination to paste the copy (source)
// - clip - The rectangle inside which all operations should be bounded (or no bounds if not given)
// - fillCharacter - Fills in the region left behind when the source is "lifted" out of its original location. The symbol to display.
// - fillAttribute - Fills in the region left behind when the source is "lifted" out of its original location. The color to use.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::ScrollConsoleScreenBufferAImpl(SCREEN_INFORMATION& context,
                                                                  const SMALL_RECT& source,
                                                                  const COORD target,
                                                                  std::optional<SMALL_RECT> clip,
                                                                  const char fillCharacter,
                                                                  const WORD fillAttribute) noexcept
{
    try
    {
        wchar_t const unicodeFillCharacter = CharToWchar(&fillCharacter, 1);

        return ScrollConsoleScreenBufferWImpl(context, source, target, clip, unicodeFillCharacter, fillAttribute);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Moves a portion of text from one part of the output buffer to another
// Arguments:
// - context - The output buffer concerned
// - source - The rectangular region to copy from
// - target - The top left corner of the destination to paste the copy (source)
// - clip - The rectangle inside which all operations should be bounded (or no bounds if not given)
// - fillCharacter - Fills in the region left behind when the source is "lifted" out of its original location. The symbol to display.
// - fillAttribute - Fills in the region left behind when the source is "lifted" out of its original location. The color to use.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::ScrollConsoleScreenBufferWImpl(SCREEN_INFORMATION& context,
                                                                  const SMALL_RECT& source,
                                                                  const COORD target,
                                                                  std::optional<SMALL_RECT> clip,
                                                                  const wchar_t fillCharacter,
                                                                  const WORD fillAttribute) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        auto& buffer = context.GetActiveBuffer();

        TextAttribute useThisAttr(fillAttribute);

        // Here we're being a little clever - similar to FillConsoleOutputAttributeImpl
        // Because RGB/default color can't roundtrip the API, certain VT
        //      sequences will forget the RGB color because their first call to
        //      GetScreenBufferInfo returned a legacy attr.
        // If they're calling this with the legacy attrs version of our current
        //      attributes, they likely wanted to use the full version of
        //      our current attributes, whether that be RGB or _default_ colored.
        // This could create a scenario where someone emitted RGB with VT,
        //      THEN used the API to ScrollConsoleOutput with the legacy attrs,
        //      and DIDN'T want the RGB color. As in FillConsoleOutputAttribute,
        //      this scenario is highly unlikely, and we can reasonably do this
        //      on their behalf.
        // see MSFT:19853701

        if (buffer.InVTMode())
        {
            const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
            const auto currentAttributes = buffer.GetAttributes();
            const auto bufferLegacy = gci.GenerateLegacyAttributes(currentAttributes);
            if (bufferLegacy == fillAttribute)
            {
                useThisAttr = currentAttributes;
            }
        }

        ScrollRegion(buffer, source, clip, target, fillCharacter, useThisAttr);

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Adjusts the default color used for future text written to this output buffer
// Arguments:
// - context - The output buffer concerned
// - attribute - Color information
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleTextAttributeImpl(SCREEN_INFORMATION& context,
                                                               const WORD attribute) noexcept
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        RETURN_HR_IF(E_INVALIDARG, WI_IsAnyFlagSet(attribute, ~VALID_TEXT_ATTRIBUTES));

        const TextAttribute attr{ attribute };
        context.SetAttributes(attr);

        gci.ConsoleIme.RefreshAreaAttributes();

        return S_OK;
    }
    CATCH_RETURN();
}

void DoSrvPrivateSetLegacyAttributes(SCREEN_INFORMATION& screenInfo,
                                     const WORD Attribute,
                                     const bool fForeground,
                                     const bool fBackground,
                                     const bool fMeta)
{
    auto& buffer = screenInfo.GetActiveBuffer();
    const TextAttribute OldAttributes = buffer.GetAttributes();
    TextAttribute NewAttributes = OldAttributes;

    NewAttributes.SetLegacyAttributes(Attribute, fForeground, fBackground, fMeta);

    buffer.SetAttributes(NewAttributes);
}

void DoSrvPrivateSetDefaultAttributes(SCREEN_INFORMATION& screenInfo,
                                      const bool fForeground,
                                      const bool fBackground)
{
    auto& buffer = screenInfo.GetActiveBuffer();
    TextAttribute NewAttributes = buffer.GetAttributes();
    if (fForeground)
    {
        NewAttributes.SetDefaultForeground();
    }
    if (fBackground)
    {
        NewAttributes.SetDefaultBackground();
    }
    buffer.SetAttributes(NewAttributes);
}

void DoSrvPrivateSetConsoleXtermTextAttribute(SCREEN_INFORMATION& screenInfo,
                                              const int iXtermTableEntry,
                                              const bool fIsForeground)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    auto& buffer = screenInfo.GetActiveBuffer();
    TextAttribute NewAttributes = buffer.GetAttributes();

    COLORREF rgbColor;
    if (iXtermTableEntry < COLOR_TABLE_SIZE)
    {
        //Convert the xterm index to the win index
        WORD iWinEntry = ::XtermToWindowsIndex(iXtermTableEntry);

        rgbColor = gci.GetColorTableEntry(iWinEntry);
    }
    else
    {
        rgbColor = gci.GetColorTableEntry(iXtermTableEntry);
    }

    NewAttributes.SetColor(rgbColor, fIsForeground);

    buffer.SetAttributes(NewAttributes);
}

void DoSrvPrivateSetConsoleRGBTextAttribute(SCREEN_INFORMATION& screenInfo,
                                            const COLORREF rgbColor,
                                            const bool fIsForeground)
{
    auto& buffer = screenInfo.GetActiveBuffer();

    TextAttribute NewAttributes = buffer.GetAttributes();
    NewAttributes.SetColor(rgbColor, fIsForeground);
    buffer.SetAttributes(NewAttributes);
}

void DoSrvPrivateBoldText(SCREEN_INFORMATION& screenInfo, const bool bolded)
{
    auto& buffer = screenInfo.GetActiveBuffer();
    auto attrs = buffer.GetAttributes();
    if (bolded)
    {
        attrs.Embolden();
    }
    else
    {
        attrs.Debolden();
    }
    buffer.SetAttributes(attrs);
}

// Method Description:
// - Retrieves the active ExtendedAttributes (italic, underline, etc.) of the
//   given screen buffer. Text written to this buffer will be written with these
//   attributes.
// Arguments:
// - screenInfo: The buffer to get the extended attrs from.
// Return Value:
// - the currently active ExtendedAttributes.
ExtendedAttributes DoSrvPrivateGetExtendedTextAttributes(SCREEN_INFORMATION& screenInfo)
{
    auto& buffer = screenInfo.GetActiveBuffer();
    auto attrs = buffer.GetAttributes();
    return attrs.GetExtendedAttributes();
}

// Method Description:
// - Sets the active ExtendedAttributes (italic, underline, etc.) of the given
//   screen buffer. Text written to this buffer will be written with these
//   attributes.
// Arguments:
// - screenInfo: The buffer to set the extended attrs for.
// - extendedAttrs: The new ExtendedAttributes to use
// Return Value:
// - <none>
void DoSrvPrivateSetExtendedTextAttributes(SCREEN_INFORMATION& screenInfo,
                                           const ExtendedAttributes extendedAttrs)
{
    auto& buffer = screenInfo.GetActiveBuffer();
    auto attrs = buffer.GetAttributes();
    attrs.SetExtendedAttributes(extendedAttrs);
    buffer.SetAttributes(attrs);
}

// Routine Description:
// - Sets the codepage used for translating text when calling A versions of functions affecting the output buffer.
// Arguments:
// - codepage - The codepage
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleOutputCodePageImpl(const ULONG codepage) noexcept
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Return if it's not known as a valid codepage ID.
        RETURN_HR_IF(E_INVALIDARG, !(IsValidCodePage(codepage)));

        // Do nothing if no change.
        if (gci.OutputCP != codepage)
        {
            // Set new code page
            gci.OutputCP = codepage;

            SetConsoleCPInfo(TRUE);
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets the codepage used for translating text when calling A versions of functions affecting the input buffer.
// Arguments:
// - codepage - The codepage
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleInputCodePageImpl(const ULONG codepage) noexcept
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Return if it's not known as a valid codepage ID.
        RETURN_HR_IF(E_INVALIDARG, !(IsValidCodePage(codepage)));

        // Do nothing if no change.
        if (gci.CP != codepage)
        {
            // Set new code page
            gci.CP = codepage;

            SetConsoleCPInfo(FALSE);
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Gets the codepage used for translating text when calling A versions of functions affecting the input buffer.
// Arguments:
// - codepage - The codepage
void ApiRoutines::GetConsoleInputCodePageImpl(ULONG& codepage) noexcept
{
    try
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        codepage = gci.CP;
    }
    CATCH_LOG();
}

void DoSrvGetConsoleOutputCodePage(_Out_ unsigned int* const pCodePage)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    *pCodePage = gci.OutputCP;
}

// Routine Description:
// - Gets the codepage used for translating text when calling A versions of functions affecting the output buffer.
// Arguments:
// - codepage - The codepage
void ApiRoutines::GetConsoleOutputCodePageImpl(ULONG& codepage) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });
        unsigned int uiCodepage;
        DoSrvGetConsoleOutputCodePage(&uiCodepage);
        codepage = uiCodepage;
    }
    CATCH_LOG();
}

// Routine Description:
// - Gets the window handle ID for the console
// Arguments:
// - hwnd - The window handle ID
void ApiRoutines::GetConsoleWindowImpl(HWND& hwnd) noexcept
{
    try
    {
        // Set return to null before we do anything in case of failures/errors.
        hwnd = nullptr;

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });
        const IConsoleWindow* pWindow = ServiceLocator::LocateConsoleWindow();
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        if (pWindow != nullptr)
        {
            hwnd = pWindow->GetWindowHandle();
        }
        else
        {
            // Some applications will fail silently if this API returns 0 (cygwin)
            // If we're in pty mode, we need to return a fake window handle that
            //      doesn't actually do anything, but is a unique HWND to this
            //      console, so that they know that this console is in fact a real
            //      console window.
            if (gci.IsInVtIoMode())
            {
                hwnd = ServiceLocator::LocatePseudoWindow();
            }
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - Gets metadata about the storage of command history for cooked read modes
// Arguments:
// - consoleHistoryInformation - metadata pertaining to the number of history buffers and their size and modes.
void ApiRoutines::GetConsoleHistoryInfoImpl(CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept
{
    try
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        consoleHistoryInfo.HistoryBufferSize = gci.GetHistoryBufferSize();
        consoleHistoryInfo.NumberOfHistoryBuffers = gci.GetNumberOfHistoryBuffers();
        WI_SetFlagIf(consoleHistoryInfo.dwFlags, HISTORY_NO_DUP_FLAG, WI_IsFlagSet(gci.Flags, CONSOLE_HISTORY_NODUP));
    }
    CATCH_LOG();
}

// Routine Description:
// - Sets metadata about the storage of command history for cooked read modes
// Arguments:
// - consoleHistoryInformation - metadata pertaining to the number of history buffers and their size and modes.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
HRESULT ApiRoutines::SetConsoleHistoryInfoImpl(const CONSOLE_HISTORY_INFO& consoleHistoryInfo) noexcept
{
    try
    {
        CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        RETURN_HR_IF(E_INVALIDARG, consoleHistoryInfo.HistoryBufferSize > SHORT_MAX);
        RETURN_HR_IF(E_INVALIDARG, consoleHistoryInfo.NumberOfHistoryBuffers > SHORT_MAX);
        RETURN_HR_IF(E_INVALIDARG, WI_IsAnyFlagSet(consoleHistoryInfo.dwFlags, ~CHI_VALID_FLAGS));

        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        CommandHistory::s_ResizeAll(consoleHistoryInfo.HistoryBufferSize);
        gci.SetNumberOfHistoryBuffers(consoleHistoryInfo.NumberOfHistoryBuffers);

        WI_UpdateFlag(gci.Flags, CONSOLE_HISTORY_NODUP, WI_IsFlagSet(consoleHistoryInfo.dwFlags, HISTORY_NO_DUP_FLAG));

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Gets whether or not the console is full screen
// Arguments:
// - flags - Field contains full screen flag or doesn't.
// NOTE: This was in private.c, but turns out to be a public API: http://msdn.microsoft.com/en-us/library/windows/desktop/ms683164(v=vs.85).aspx
void ApiRoutines::GetConsoleDisplayModeImpl(ULONG& flags) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Initialize flags portion of structure
        flags = 0;

        IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
        if (pWindow != nullptr && pWindow->IsInFullscreen())
        {
            WI_SetFlag(flags, CONSOLE_FULLSCREEN_MODE);
        }
    }
    CATCH_LOG();
}

// Routine Description:
// - This routine sets the console display mode for an output buffer.
// - This API is only supported on x86 machines.
// Parameters:
// - context - Supplies a console output handle.
// - flags - Specifies the display mode. Options are:
//      CONSOLE_FULLSCREEN_MODE - data is displayed fullscreen
//      CONSOLE_WINDOWED_MODE - data is displayed in a window
// - newSize - On output, contains the new dimensions of the screen buffer.  The dimensions are in rows and columns for textmode screen buffers.
// Return value:
// - TRUE - The operation was successful.
// - FALSE/nullptr - The operation failed. Extended error status is available using GetLastError.
// NOTE:
// - This was in private.c, but turns out to be a public API:
// - See: http://msdn.microsoft.com/en-us/library/windows/desktop/ms686028(v=vs.85).aspx
[[nodiscard]] HRESULT ApiRoutines::SetConsoleDisplayModeImpl(SCREEN_INFORMATION& context,
                                                             const ULONG flags,
                                                             COORD& newSize) noexcept
{
    try
    {
        // SetIsFullscreen() below ultimately calls SetwindowLong, which ultimately calls SendMessage(). If we retain
        // the console lock, we'll deadlock since ConsoleWindowProc takes the lock before processing messages. Instead,
        // we'll release early.
        LockConsole();
        {
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

            SCREEN_INFORMATION& screenInfo = context.GetActiveBuffer();

            newSize = screenInfo.GetBufferSize().Dimensions();
            RETURN_HR_IF(E_INVALIDARG, !(screenInfo.IsActiveScreenBuffer()));
        }

        IConsoleWindow* const pWindow = ServiceLocator::LocateConsoleWindow();
        if (WI_IsFlagSet(flags, CONSOLE_FULLSCREEN_MODE))
        {
            if (pWindow != nullptr)
            {
                pWindow->SetIsFullscreen(true);
            }
        }
        else if (WI_IsFlagSet(flags, CONSOLE_WINDOWED_MODE))
        {
            if (pWindow != nullptr)
            {
                pWindow->SetIsFullscreen(false);
            }
        }
        else
        {
            RETURN_HR(E_INVALIDARG);
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - A private API call for changing the cursor keys input mode between normal and application mode.
//     The cursor keys are the arrows, plus Home and End.
// Parameters:
// - fApplicationMode - set to true to enable Application Mode Input, false for Numeric Mode Input.
// Return value:
// - True if handled successfully. False otherwise.
[[nodiscard]] NTSTATUS DoSrvPrivateSetCursorKeysMode(_In_ bool fApplicationMode)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.pInputBuffer == nullptr)
    {
        return STATUS_UNSUCCESSFUL;
    }
    gci.pInputBuffer->GetTerminalInput().ChangeCursorKeysMode(fApplicationMode);
    return STATUS_SUCCESS;
}

// Routine Description:
// - A private API call for changing the keypad input mode between numeric and application mode.
//     This controls what the keys on the numpad translate to.
// Parameters:
// - fApplicationMode - set to true to enable Application Mode Input, false for Numeric Mode Input.
// Return value:
// - True if handled successfully. False otherwise.
[[nodiscard]] NTSTATUS DoSrvPrivateSetKeypadMode(_In_ bool fApplicationMode)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    if (gci.pInputBuffer == nullptr)
    {
        return STATUS_UNSUCCESSFUL;
    }
    gci.pInputBuffer->GetTerminalInput().ChangeKeypadMode(fApplicationMode);
    return STATUS_SUCCESS;
}

// Routine Description:
// - A private API call for making the cursor visible or not. Does not modify
//      blinking state.
// Parameters:
// - show - set to true to make the cursor visible, false to hide.
// Return value:
// - <none>
void DoSrvPrivateShowCursor(SCREEN_INFORMATION& screenInfo, const bool show) noexcept
{
    screenInfo.GetActiveBuffer().GetTextBuffer().GetCursor().SetIsVisible(show);
}

// Routine Description:
// - A private API call for enabling or disabling the cursor blinking.
// Parameters:
// - fEnable - set to true to enable blinking, false to disable
// Return value:
// - True if handled successfully. False otherwise.
void DoSrvPrivateAllowCursorBlinking(SCREEN_INFORMATION& screenInfo, const bool fEnable)
{
    screenInfo.GetActiveBuffer().GetTextBuffer().GetCursor().SetBlinkingAllowed(fEnable);
    screenInfo.GetActiveBuffer().GetTextBuffer().GetCursor().SetIsOn(!fEnable);
}

// Routine Description:
// - A private API call for setting the top and bottom scrolling margins for
//     the current page. This creates a subsection of the screen that scrolls
//     when input reaches the end of the region, leaving the rest of the screen
//     untouched.
//  Currently only accessible through the use of ANSI sequence DECSTBM
// Parameters:
// - psrScrollMargins - A rect who's Top and Bottom members will be used to set
//     the new values of the top and bottom margins. If (0,0), then the margins
//     will be disabled. NOTE: This is a rect in the case that we'll need the
//     left and right margins in the future.
// Return value:
// - True if handled successfully. False otherwise.
[[nodiscard]] NTSTATUS DoSrvPrivateSetScrollingRegion(SCREEN_INFORMATION& screenInfo, const SMALL_RECT* const psrScrollMargins)
{
    NTSTATUS Status = STATUS_SUCCESS;

    if (psrScrollMargins->Top > psrScrollMargins->Bottom)
    {
        Status = STATUS_INVALID_PARAMETER;
    }
    if (NT_SUCCESS(Status))
    {
        SMALL_RECT srScrollMargins = screenInfo.GetRelativeScrollMargins().ToInclusive();
        srScrollMargins.Top = psrScrollMargins->Top;
        srScrollMargins.Bottom = psrScrollMargins->Bottom;
        screenInfo.GetActiveBuffer().SetScrollMargins(Viewport::FromInclusive(srScrollMargins));
    }

    return Status;
}

// Routine Description:
// - A private API call for performing a "Reverse line feed", essentially, the opposite of '\n'.
//    Moves the cursor up one line, and tries to keep its position in the line
// Parameters:
// - screenInfo - a pointer to the screen buffer that should perform the reverse line feed
// Return value:
// - True if handled successfully. False otherwise.
[[nodiscard]] NTSTATUS DoSrvPrivateReverseLineFeed(SCREEN_INFORMATION& screenInfo)
{
    NTSTATUS Status = STATUS_SUCCESS;

    const SMALL_RECT viewport = screenInfo.GetActiveBuffer().GetViewport().ToInclusive();
    const COORD oldCursorPosition = screenInfo.GetTextBuffer().GetCursor().GetPosition();
    const COORD newCursorPosition = { oldCursorPosition.X, oldCursorPosition.Y - 1 };

    // If the cursor is at the top of the viewport, we don't want to shift the viewport up.
    // We want it to stay exactly where it is.
    // In that case, shift the buffer contents down, to emulate inserting a line
    //      at the top of the buffer.
    if (oldCursorPosition.Y > viewport.Top)
    {
        // Cursor is below the top line of the viewport
        Status = AdjustCursorPosition(screenInfo, newCursorPosition, TRUE, nullptr);
    }
    else
    {
        // If we don't have margins, or the cursor is within the boundaries of the margins
        // It's important to check if the cursor is in the margins,
        //      If it's not, but the margins are set, then we don't want to scroll anything
        if (screenInfo.IsCursorInMargins(oldCursorPosition))
        {
            // Cursor is at the top of the viewport
            // Rectangle to cut out of the existing buffer. This is inclusive.
            // It will be clipped to the buffer boundaries so SHORT_MAX gives us the full buffer width.
            SMALL_RECT srScroll;
            srScroll.Left = 0;
            srScroll.Right = SHORT_MAX;
            srScroll.Top = viewport.Top;
            srScroll.Bottom = viewport.Bottom;
            // Clip to the DECSTBM margin boundary
            if (screenInfo.AreMarginsSet())
            {
                srScroll.Bottom = screenInfo.GetAbsoluteScrollMargins().BottomInclusive();
            }
            // Paste coordinate for cut text above
            COORD coordDestination;
            coordDestination.X = 0;
            coordDestination.Y = viewport.Top + 1;

            // Here we previously called to ScrollConsoleScreenBufferWImpl to
            // perform the scrolling operation. However, that function only
            // accepts a WORD for the fill attributes. That means we'd lose
            // 256/RGB fidelity for fill attributes. So instead, we'll just call
            // ScrollRegion ourselves, with the same params that
            // ScrollConsoleScreenBufferWImpl would have.
            // See microsoft/terminal#832, #2702 for more context.
            try
            {
                LockConsole();
                auto Unlock = wil::scope_exit([&] { UnlockConsole(); });
                ScrollRegion(screenInfo,
                             srScroll,
                             srScroll,
                             coordDestination,
                             UNICODE_SPACE,
                             screenInfo.GetAttributes());
            }
            CATCH_LOG();
        }
    }
    return Status;
}

// Routine Description:
// - A private API call for moving the cursor vertically in the buffer. This is
//      because the vertical cursor movements in VT are constrained by the
//      scroll margins, while the absolute positioning is not.
// Parameters:
// - screenInfo - a reference to the screen buffer we should move the cursor for
// - lines - The number of lines to move the cursor. Up is negative, down positive.
// Return value:
// - S_OK if handled successfully. Otherwise an appropriate HRESULT for failing to clamp.
[[nodiscard]] HRESULT DoSrvMoveCursorVertically(SCREEN_INFORMATION& screenInfo, const short lines)
{
    auto& cursor = screenInfo.GetActiveBuffer().GetTextBuffer().GetCursor();
    COORD clampedPos = { cursor.GetPosition().X, cursor.GetPosition().Y + lines };

    // Make sure the cursor doesn't move outside the viewport.
    screenInfo.GetViewport().Clamp(clampedPos);

    // Make sure the cursor stays inside the margins
    if (screenInfo.AreMarginsSet())
    {
        const auto margins = screenInfo.GetAbsoluteScrollMargins().ToInclusive();

        const auto cursorY = cursor.GetPosition().Y;

        const auto lo = margins.Top;
        const auto hi = margins.Bottom;

        // See microsoft/terminal#2929 - If the cursor is _below_ the top
        // margin, it should stay below the top margin. If it's _above_ the
        // bottom, it should stay above the bottom. Cursor movements that stay
        // outside the margins shouldn't necessarily be affected. For example,
        // moving up while below the bottom margin shouldn't just jump straight
        // to the bottom margin. See
        // ScreenBufferTests::CursorUpDownOutsideMargins for a test of that
        // behavior.
        const bool cursorBelowTop = cursorY >= lo;
        const bool cursorAboveBottom = cursorY <= hi;

        if (cursorBelowTop)
        {
            try
            {
                clampedPos.Y = std::max(clampedPos.Y, lo);
            }
            CATCH_RETURN();
        }

        if (cursorAboveBottom)
        {
            try
            {
                clampedPos.Y = std::min(clampedPos.Y, hi);
            }
            CATCH_RETURN();
        }
    }
    cursor.SetPosition(clampedPos);

    return S_OK;
}

// Routine Description:
// - A private API call for swapping to the alternate screen buffer. In virtual terminals, there exists both a "main"
//     screen buffer and an alternate. ASBSET creates a new alternate, and switches to it. If there is an already
//     existing alternate, it is discarded.
// Parameters:
// - screenInfo - a reference to the screen buffer that should use an alternate buffer
// Return value:
// - True if handled successfully. False otherwise.
[[nodiscard]] NTSTATUS DoSrvPrivateUseAlternateScreenBuffer(SCREEN_INFORMATION& screenInfo)
{
    return screenInfo.GetActiveBuffer().UseAlternateScreenBuffer();
}

// Routine Description:
// - A private API call for swaping to the main screen buffer. From the
//     alternate buffer, returns to the main screen buffer. From the main
//     screen buffer, does nothing. The alternate is discarded.
// Parameters:
// - screenInfo - a reference to the screen buffer that should use the main buffer
// Return value:
// - True if handled successfully. False otherwise.
void DoSrvPrivateUseMainScreenBuffer(SCREEN_INFORMATION& screenInfo)
{
    screenInfo.GetActiveBuffer().UseMainScreenBuffer();
}

// Routine Description:
// - A private API call for setting a VT tab stop in the cursor's current column.
// Parameters:
// <none>
// Return value:
// - STATUS_SUCCESS if handled successfully. Otherwise, an appropriate status code indicating the error.
[[nodiscard]] NTSTATUS DoSrvPrivateHorizontalTabSet()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& _screenBuffer = gci.GetActiveOutputBuffer().GetActiveBuffer();

    const COORD cursorPos = _screenBuffer.GetTextBuffer().GetCursor().GetPosition();
    try
    {
        _screenBuffer.AddTabStop(cursorPos.X);
    }
    catch (...)
    {
        return NTSTATUS_FROM_HRESULT(wil::ResultFromCaughtException());
    }
    return STATUS_SUCCESS;
}

// Routine Description:
// - A private helper for executing a number of tabs.
// Parameters:
// sNumTabs - The number of tabs to execute
// fForward - whether to tab forward or backwards
// Return value:
// - STATUS_SUCCESS if handled successfully. Otherwise, an appropriate status code indicating the error.
[[nodiscard]] NTSTATUS DoPrivateTabHelper(const SHORT sNumTabs, _In_ bool fForward)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& _screenBuffer = gci.GetActiveOutputBuffer().GetActiveBuffer();

    NTSTATUS Status = STATUS_SUCCESS;
    FAIL_FAST_IF(!(sNumTabs >= 0));
    for (SHORT sTabsExecuted = 0; sTabsExecuted < sNumTabs && NT_SUCCESS(Status); sTabsExecuted++)
    {
        const COORD cursorPos = _screenBuffer.GetTextBuffer().GetCursor().GetPosition();
        COORD cNewPos = (fForward) ? _screenBuffer.GetForwardTab(cursorPos) : _screenBuffer.GetReverseTab(cursorPos);
        // GetForwardTab is smart enough to move the cursor to the next line if
        // it's at the end of the current one already. AdjustCursorPos shouldn't
        // to be doing anything funny, just moving the cursor to the location GetForwardTab returns
        Status = AdjustCursorPosition(_screenBuffer, cNewPos, TRUE, nullptr);
    }
    return Status;
}

// Routine Description:
// - A private API call for performing a forwards tab. This will take the
//     cursor to the tab stop following its current location. If there are no
//     more tabs in this row, it will take it to the right side of the window.
//     If it's already in the last column of the row, it will move it to the next line.
// Parameters:
// - sNumTabs - The number of tabs to perform.
// Return value:
// - STATUS_SUCCESS if handled successfully. Otherwise, an appropriate status code indicating the error.
[[nodiscard]] NTSTATUS DoSrvPrivateForwardTab(const SHORT sNumTabs)
{
    return DoPrivateTabHelper(sNumTabs, true);
}

// Routine Description:
// - A private API call for performing a backwards tab. This will take the
//     cursor to the tab stop previous to its current location. It will not reverse line feed.
// Parameters:
// - sNumTabs - The number of tabs to perform.
// Return value:
// - STATUS_SUCCESS if handled successfully. Otherwise, an appropriate status code indicating the error.
[[nodiscard]] NTSTATUS DoSrvPrivateBackwardsTab(const SHORT sNumTabs)
{
    return DoPrivateTabHelper(sNumTabs, false);
}

// Routine Description:
// - A private API call for clearing the VT tabs that have been set.
// Parameters:
// - fClearAll - If false, only clears the tab in the current column (if it exists)
//      otherwise clears all set tabs. (and reverts to legacy 8-char tabs behavior.)
// Return value:
// - None
void DoSrvPrivateTabClear(const bool fClearAll)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    SCREEN_INFORMATION& screenBuffer = gci.GetActiveOutputBuffer().GetActiveBuffer();
    if (fClearAll)
    {
        screenBuffer.ClearTabStops();
    }
    else
    {
        const COORD cursorPos = screenBuffer.GetTextBuffer().GetCursor().GetPosition();
        screenBuffer.ClearTabStop(cursorPos.X);
    }
}

// Routine Description:
// - A private API call for enabling VT200 style mouse mode.
// Parameters:
// - fEnable - true to enable default tracking mode, false to disable mouse mode.
// Return value:
// - None
void DoSrvPrivateEnableVT200MouseMode(const bool fEnable)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.terminalMouseInput.EnableDefaultTracking(fEnable);
}

// Routine Description:
// - A private API call for enabling utf8 style mouse mode.
// Parameters:
// - fEnable - true to enable, false to disable.
// Return value:
// - None
void DoSrvPrivateEnableUTF8ExtendedMouseMode(const bool fEnable)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.terminalMouseInput.SetUtf8ExtendedMode(fEnable);
}

// Routine Description:
// - A private API call for enabling SGR style mouse mode.
// Parameters:
// - fEnable - true to enable, false to disable.
// Return value:
// - None
void DoSrvPrivateEnableSGRExtendedMouseMode(const bool fEnable)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.terminalMouseInput.SetSGRExtendedMode(fEnable);
}

// Routine Description:
// - A private API call for enabling button-event mouse mode.
// Parameters:
// - fEnable - true to enable button-event mode, false to disable mouse mode.
// Return value:
// - None
void DoSrvPrivateEnableButtonEventMouseMode(const bool fEnable)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.terminalMouseInput.EnableButtonEventTracking(fEnable);
}

// Routine Description:
// - A private API call for enabling any-event mouse mode.
// Parameters:
// - fEnable - true to enable any-event mode, false to disable mouse mode.
// Return value:
// - None
void DoSrvPrivateEnableAnyEventMouseMode(const bool fEnable)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.terminalMouseInput.EnableAnyEventTracking(fEnable);
}

// Routine Description:
// - A private API call for enabling alternate scroll mode
// Parameters:
// - fEnable - true to enable alternate scroll mode, false to disable.
// Return value:
// None
void DoSrvPrivateEnableAlternateScroll(const bool fEnable)
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    gci.terminalMouseInput.EnableAlternateScroll(fEnable);
}

// Routine Description:
// - A private API call for performing a VT-style erase all operation on the buffer.
//      See SCREEN_INFORMATION::VtEraseAll's description for details.
// Parameters:
//  The ScreenBuffer to perform the erase on.
// Return value:
// - STATUS_SUCCESS if we succeeded, otherwise the NTSTATUS version of the failure.
[[nodiscard]] NTSTATUS DoSrvPrivateEraseAll(SCREEN_INFORMATION& screenInfo)
{
    return NTSTATUS_FROM_HRESULT(screenInfo.GetActiveBuffer().VtEraseAll());
}

void DoSrvSetCursorStyle(SCREEN_INFORMATION& screenInfo,
                         const CursorType cursorType)
{
    screenInfo.GetActiveBuffer().GetTextBuffer().GetCursor().SetType(cursorType);
}

void DoSrvSetCursorColor(SCREEN_INFORMATION& screenInfo,
                         const COLORREF cursorColor)
{
    screenInfo.GetActiveBuffer().GetTextBuffer().GetCursor().SetColor(cursorColor);
}

// Routine Description:
// - A private API call to get only the default color attributes of the screen buffer.
// - This is used as a performance optimization by the VT adapter in SGR (Set Graphics Rendition) instead
//   of calling for this information through the public API GetConsoleScreenBufferInfoEx which returns a lot
//   of extra unnecessary data and takes a lot of extra processing time.
// Parameters
// - screenInfo - The screen buffer to retrieve default color attributes information from
// - pwAttributes - Pointer to space that will receive color attributes data
// Return Value:
// - STATUS_SUCCESS if we succeeded or STATUS_INVALID_PARAMETER for bad params (nullptr).
[[nodiscard]] NTSTATUS DoSrvPrivateGetConsoleScreenBufferAttributes(_In_ const SCREEN_INFORMATION& screenInfo, _Out_ WORD* const pwAttributes)
{
    NTSTATUS Status = STATUS_SUCCESS;

    if (pwAttributes == nullptr)
    {
        Status = STATUS_INVALID_PARAMETER;
    }

    if (NT_SUCCESS(Status))
    {
        *pwAttributes = screenInfo.GetActiveBuffer().GetAttributes().GetLegacyAttributes();
    }

    return Status;
}

// Routine Description:
// - A private API call for forcing the renderer to repaint the screen. If the
//      input screen buffer is not the active one, then just do nothing. We only
//      want to redraw the screen buffer that requested the repaint, and
//      switching screen buffers will already force a repaint.
// Parameters:
//  The ScreenBuffer to perform the repaint for.
// Return value:
// - None
void DoSrvPrivateRefreshWindow(_In_ const SCREEN_INFORMATION& screenInfo)
{
    Globals& g = ServiceLocator::LocateGlobals();
    if (&screenInfo == &g.getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer())
    {
        g.pRender->TriggerRedrawAll();
    }
}

// Routine Description:
// - Gets title information from the console. It can be truncated if the buffer is too small.
// Arguments:
// - title - If given, this buffer is filled with the title information requested.
//         - Use nullopt to request buffer size required.
// - written - The number of characters filled in the title buffer.
// - needed - The number of characters we would need to completely write out the title.
// - isOriginal - If true, gets the title when we booted up. If false, gets whatever it is set to right now.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT GetConsoleTitleWImplHelper(std::optional<gsl::span<wchar_t>> title,
                                                 size_t& written,
                                                 size_t& needed,
                                                 const bool isOriginal) noexcept
{
    try
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // Ensure output variables are initialized.
        written = 0;
        needed = 0;

        if (title.has_value() && title.value().size() > 0)
        {
            title.value().at(0) = ANSI_NULL;
        }

        // Get the appropriate title and length depending on the mode.
        const wchar_t* pwszTitle;
        size_t cchTitleLength;

        if (isOriginal)
        {
            pwszTitle = gci.GetOriginalTitle().c_str();
            cchTitleLength = gci.GetOriginalTitle().length();
        }
        else
        {
            pwszTitle = gci.GetTitle().c_str();
            cchTitleLength = gci.GetTitle().length();
        }

        // Always report how much space we would need.
        needed = cchTitleLength;

        // If we have a pointer to receive the data, then copy it out.
        if (title.has_value())
        {
            HRESULT const hr = StringCchCopyNW(title.value().data(), title.value().size(), pwszTitle, cchTitleLength);

            // Insufficient buffer is allowed. If we return a partial string, that's still OK by historical/compat standards.
            // Just say how much we managed to return.
            if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr)
            {
                written = std::min(gsl::narrow<size_t>(title.value().size()), cchTitleLength);
            }
        }
        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Gets title information from the console. It can be truncated if the buffer is too small.
// Arguments:
// - title - If given, this buffer is filled with the title information requested.
//         - Use nullopt to request buffer size required.
// - written - The number of characters filled in the title buffer.
// - needed - The number of characters we would need to completely write out the title.
// - isOriginal - If true, gets the title when we booted up. If false, gets whatever it is set to right now.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception

[[nodiscard]] HRESULT GetConsoleTitleAImplHelper(gsl::span<char> title,
                                                 size_t& written,
                                                 size_t& needed,
                                                 const bool isOriginal) noexcept
{
    try
    {
        const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // Ensure output variables are initialized.
        written = 0;
        needed = 0;

        if (title.size() > 0)
        {
            title.at(0) = ANSI_NULL;
        }

        // Figure out how big our temporary Unicode buffer must be to get the title.
        size_t unicodeNeeded;
        size_t unicodeWritten;
        RETURN_IF_FAILED(GetConsoleTitleWImplHelper(std::nullopt, unicodeWritten, unicodeNeeded, isOriginal));

        // If there's nothing to get, then simply return.
        RETURN_HR_IF(S_OK, 0 == unicodeNeeded);

        // Allocate a unicode buffer of the right size.
        size_t const unicodeSize = unicodeNeeded + 1; // add one for null terminator space
        std::unique_ptr<wchar_t[]> unicodeBuffer = std::make_unique<wchar_t[]>(unicodeSize);
        RETURN_IF_NULL_ALLOC(unicodeBuffer);

        const gsl::span<wchar_t> unicodeSpan(unicodeBuffer.get(), unicodeSize);

        // Retrieve the title in Unicode.
        RETURN_IF_FAILED(GetConsoleTitleWImplHelper(unicodeSpan, unicodeWritten, unicodeNeeded, isOriginal));

        // Convert result to A
        const auto converted = ConvertToA(gci.CP, { unicodeBuffer.get(), unicodeWritten });

        // The legacy A behavior is a bit strange. If the buffer given doesn't have enough space to hold
        // the string without null termination (e.g. the title is 9 long, 10 with null. The buffer given isn't >= 9).
        // then do not copy anything back and do not report how much space we need.
        if (gsl::narrow<size_t>(title.size()) >= converted.size())
        {
            // Say how many characters of buffer we would need to hold the entire result.
            needed = converted.size();

            // Copy safely to output buffer
            HRESULT const hr = StringCchCopyNA(title.data(), title.size(), converted.data(), converted.size());

            // Insufficient buffer is allowed. If we return a partial string, that's still OK by historical/compat standards.
            // Just say how much we managed to return.
            if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr)
            {
                // And return the size copied (either the size of the buffer or the null terminated length of the string we filled it with.)
                written = std::min(gsl::narrow<size_t>(title.size()), converted.size() + 1);

                // Another compatibility fix... If we had exactly the number of bytes needed for an unterminated string,
                // then replace the terminator left behind by StringCchCopyNA with the final character of the title string.
                if (gsl::narrow<size_t>(title.size()) == converted.size())
                {
                    title.at(title.size() - 1) = converted.data()[converted.size() - 1];
                }
            }
        }
        else
        {
            // If we didn't copy anything back and there is space, null terminate the given buffer and return.
            if (title.size() > 0)
            {
                title.at(0) = ANSI_NULL;
                written = 1;
            }
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Gets title information from the console. It can be truncated if the buffer is too small.
// Arguments:
// - title - If given, this buffer is filled with the title information requested.
//         - Use nullopt to request buffer size required.
// - written - The number of characters filled in the title buffer.
// - needed - The number of characters we would need to completely write out the title.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::GetConsoleTitleAImpl(gsl::span<char> title,
                                                        size_t& written,
                                                        size_t& needed) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        return GetConsoleTitleAImplHelper(title, written, needed, false);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Gets title information from the console. It can be truncated if the buffer is too small.
// Arguments:
// - title - If given, this buffer is filled with the title information requested.
//         - Use nullopt to request buffer size required.
// - written - The number of characters filled in the title buffer.
// - needed - The number of characters we would need to completely write out the title.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::GetConsoleTitleWImpl(gsl::span<wchar_t> title,
                                                        size_t& written,
                                                        size_t& needed) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        return GetConsoleTitleWImplHelper(title, written, needed, false);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Gets title information from the console. It can be truncated if the buffer is too small.
// Arguments:
// - title - If given, this buffer is filled with the title information requested.
//         - Use nullopt to request buffer size required.
// - written - The number of characters filled in the title buffer.
// - needed - The number of characters we would need to completely write out the title.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::GetConsoleOriginalTitleAImpl(gsl::span<char> title,
                                                                size_t& written,
                                                                size_t& needed) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        return GetConsoleTitleAImplHelper(title, written, needed, true);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Gets title information from the console. It can be truncated if the buffer is too small.
// Arguments:
// - title - If given, this buffer is filled with the title information requested.
//         - Use nullopt to request buffer size required.
// - written - The number of characters filled in the title buffer.
// - needed - The number of characters we would need to completely write out the title.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::GetConsoleOriginalTitleWImpl(gsl::span<wchar_t> title,
                                                                size_t& written,
                                                                size_t& needed) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        return GetConsoleTitleWImplHelper(title, written, needed, true);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets title information from the console.
// Arguments:
// - title - The new title to store and display on the console window.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleTitleAImpl(const std::string_view title) noexcept
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    try
    {
        const auto titleW = ConvertToW(gci.CP, title);

        return SetConsoleTitleWImpl(titleW);
    }
    CATCH_RETURN();
}

// Routine Description:
// - Sets title information from the console.
// Arguments:
// - title - The new title to store and display on the console window.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::SetConsoleTitleWImpl(const std::wstring_view title) noexcept
{
    LockConsole();
    auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

    return DoSrvSetConsoleTitleW(title);
}

[[nodiscard]] HRESULT DoSrvSetConsoleTitleW(const std::wstring_view title) noexcept
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

    // Sanitize the input if we're in pty mode. No control chars - this string
    //      will get emitted back to the TTY in a VT sequence, and we don't want
    //      to embed control characters in that string.
    if (gci.IsInVtIoMode())
    {
        std::wstring sanitized;
        sanitized.reserve(title.size());
        for (size_t i = 0; i < title.size(); i++)
        {
            if (title.at(i) >= UNICODE_SPACE)
            {
                sanitized.push_back(title.at(i));
            }
        }

        gci.SetTitle({ sanitized });
    }
    else
    {
        // SetTitle will trigger the renderer to update the titlebar for us.
        gci.SetTitle(title);
    }

    return S_OK;
}

// Routine Description:
// - A private API call for forcing the VT Renderer to NOT paint the next resize
//      event. This is used by InteractDispatch, to prevent resizes from echoing
//      between terminal and host.
// Parameters:
//  <none>
// Return value:
// - STATUS_SUCCESS if we succeeded, otherwise the NTSTATUS version of the failure.
[[nodiscard]] NTSTATUS DoSrvPrivateSuppressResizeRepaint()
{
    CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    FAIL_FAST_IF(!(gci.IsInVtIoMode()));
    return NTSTATUS_FROM_HRESULT(gci.GetVtIo()->SuppressResizeRepaint());
}

// Routine Description:
// - An API call for checking if the console host is acting as a pty.
// Parameters:
// - isPty: receives the bool indicating whether or not we're in pty mode.
// Return value:
//  <none>
void DoSrvIsConsolePty(_Out_ bool* const pIsPty)
{
    const CONSOLE_INFORMATION& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
    *pIsPty = gci.IsInVtIoMode();
}

// Routine Description:
// - a private API call for setting the default tab stops in the active screen buffer.
void DoSrvPrivateSetDefaultTabStops()
{
    ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer().SetDefaultVtTabStops();
}

// Routine Description:
// - internal logic for adding or removing lines in the active screen buffer
//   this also moves the cursor to the left margin, which is expected behaviour for IL and DL
// Parameters:
// - count - the number of lines to modify
// - insert - true if inserting lines, false if deleting lines
void DoSrvPrivateModifyLinesImpl(const unsigned int count, const bool insert)
{
    auto& screenInfo = ServiceLocator::LocateGlobals().getConsoleInformation().GetActiveOutputBuffer().GetActiveBuffer();
    auto& textBuffer = screenInfo.GetTextBuffer();
    const auto cursorPosition = textBuffer.GetCursor().GetPosition();
    if (screenInfo.IsCursorInMargins(cursorPosition))
    {
        // Rectangle to cut out of the existing buffer. This is inclusive.
        // It will be clipped to the buffer boundaries so SHORT_MAX gives us the full buffer width.
        SMALL_RECT srScroll;
        srScroll.Left = 0;
        srScroll.Right = SHORT_MAX;
        srScroll.Top = cursorPosition.Y;
        srScroll.Bottom = screenInfo.GetViewport().BottomInclusive();
        // Clip to the DECSTBM margin boundary
        if (screenInfo.AreMarginsSet())
        {
            srScroll.Bottom = screenInfo.GetAbsoluteScrollMargins().BottomInclusive();
        }
        // Paste coordinate for cut text above
        COORD coordDestination;
        coordDestination.X = 0;
        if (insert)
        {
            coordDestination.Y = (cursorPosition.Y) + gsl::narrow<short>(count);
        }
        else
        {
            coordDestination.Y = (cursorPosition.Y) - gsl::narrow<short>(count);
        }

        // Here we previously called to ScrollConsoleScreenBufferWImpl to
        // perform the scrolling operation. However, that function only accepts
        // a WORD for the fill attributes. That means we'd lose 256/RGB fidelity
        // for fill attributes. So instead, we'll just call ScrollRegion
        // ourselves, with the same params that ScrollConsoleScreenBufferWImpl
        // would have.
        // See microsoft/terminal#832 for more context.
        try
        {
            LockConsole();
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });
            ScrollRegion(screenInfo,
                         srScroll,
                         srScroll,
                         coordDestination,
                         UNICODE_SPACE,
                         screenInfo.GetAttributes());
        }
        CATCH_LOG();

        // The IL and DL controls are also expected to move the cursor to the left margin.
        // For now this is just column 0, since we don't yet support DECSLRM.
        LOG_IF_NTSTATUS_FAILED(screenInfo.SetCursorPosition({ 0, cursorPosition.Y }, false));
    }
}

// Routine Description:
// - a private API call for deleting lines in the active screen buffer.
// Parameters:
// - count - the number of lines to delete
void DoSrvPrivateDeleteLines(const unsigned int count)
{
    DoSrvPrivateModifyLinesImpl(count, false);
}

// Routine Description:
// - a private API call for inserting lines in the active screen buffer.
// Parameters:
// - count - the number of lines to insert
void DoSrvPrivateInsertLines(const unsigned int count)
{
    DoSrvPrivateModifyLinesImpl(count, true);
}

// Method Description:
// - Snaps the screen buffer's viewport to the "virtual bottom", the last place
//the viewport was before the user scrolled it (with the mouse or scrollbar)
// Arguments:
// - screenInfo: the buffer to move the viewport for.
// Return Value:
// - <none>
void DoSrvPrivateMoveToBottom(SCREEN_INFORMATION& screenInfo)
{
    screenInfo.GetActiveBuffer().MoveToBottom();
}

// Method Description:
// - Sets the color table value in index to the color specified in value.
//      Can be used to set the 256-color table as well as the 16-color table.
// Arguments:
// - index: the index in the table to change.
// - value: the new RGB value to use for that index in the color table.
// Return Value:
// - E_INVALIDARG if index is >= 256, else S_OK
// Notes:
//  Does not take a buffer parameter. The color table for a console and for
//      terminals as well is global, not per-screen-buffer.
[[nodiscard]] HRESULT DoSrvPrivateSetColorTableEntry(const short index, const COLORREF value) noexcept
{
    RETURN_HR_IF(E_INVALIDARG, index >= 256);
    try
    {
        Globals& g = ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();

        gci.SetColorTableEntry(index, value);

        // Update the screen colors if we're not a pty
        // No need to force a redraw in pty mode.
        if (g.pRender && !gci.IsInVtIoMode())
        {
            g.pRender->TriggerRedrawAll();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Method Description:
// - Sets the default foreground color to the color specified in value.
// Arguments:
// - value: the new RGB value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DoSrvPrivateSetDefaultForegroundColor(const COLORREF value) noexcept
{
    try
    {
        Globals& g = ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();

        gci.SetDefaultForegroundColor(value);

        // Update the screen colors if we're not a pty
        // No need to force a redraw in pty mode.
        if (g.pRender && !gci.IsInVtIoMode())
        {
            g.pRender->TriggerRedrawAll();
        }

        return S_OK;
    }
    CATCH_RETURN();
}

// Method Description:
// - Sets the default background color to the color specified in value.
// Arguments:
// - value: the new RGB value to use, as a COLORREF, format 0x00BBGGRR.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DoSrvPrivateSetDefaultBackgroundColor(const COLORREF value) noexcept
{
    try
    {
        Globals& g = ServiceLocator::LocateGlobals();
        CONSOLE_INFORMATION& gci = g.getConsoleInformation();

        gci.SetDefaultBackgroundColor(value);

        // Update the screen colors if we're not a pty
        // No need to force a redraw in pty mode.
        if (g.pRender && !gci.IsInVtIoMode())
        {
            g.pRender->TriggerRedrawAll();
        }

        return S_OK;
    }
    CATCH_RETURN();
}
