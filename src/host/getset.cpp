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

#include "../interactivity/inc/ServiceLocator.hpp"

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
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
        til::size dwSize;
        til::point dwCursorPosition;
        til::inclusive_rect srWindow;
        til::size dwMaximumWindowSize;

        context.GetActiveBuffer().GetScreenBufferInformation(&dwSize,
                                                             &dwCursorPosition,
                                                             &srWindow,
                                                             &data.wAttributes,
                                                             &dwMaximumWindowSize,
                                                             &data.wPopupAttributes,
                                                             data.ColorTable);

        // Callers of this function expect to receive an exclusive rect, not an
        // inclusive one. The driver will mangle this value for us
        // - For GetConsoleScreenBufferInfoEx, it will re-decrement these values
        //   to return an inclusive rect.
        // - For GetConsoleScreenBufferInfo, it will leave these values
        //   untouched, returning an exclusive rect.
        srWindow.right += 1;
        srWindow.bottom += 1;

        data.dwSize = til::unwrap_coord_size(dwSize);
        data.dwCursorPosition = til::unwrap_coord(dwCursorPosition);
        data.srWindow = til::unwrap_small_rect(srWindow);
        data.dwMaximumWindowSize = til::unwrap_coord_size(dwMaximumWindowSize);
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

            consoleSelectionInfo.dwSelectionAnchor = til::unwrap_coord(selection.GetSelectionAnchor());
            consoleSelectionInfo.srSelection = til::unwrap_small_rect(selection.GetSelectionRectangle());
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
                                                          til::size& size) noexcept
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
            size = {};
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

        const auto& activeScreenInfo = context.GetActiveBuffer();

        til::size WindowSize;
        if (isForMaximumWindowSize)
        {
            WindowSize = activeScreenInfo.GetMaxWindowSizeInCharacters();
        }
        else
        {
            WindowSize = activeScreenInfo.GetCurrentFont().GetUnscaledSize();
        }
        consoleFontInfoEx.dwFontSize = til::unwrap_coord_size(WindowSize);

        consoleFontInfoEx.nFont = 0;

        const auto& fontInfo = activeScreenInfo.GetCurrentFont();
        consoleFontInfoEx.FontFamily = fontInfo.GetFamily();
        consoleFontInfoEx.FontWeight = fontInfo.GetWeight();
        fontInfo.FillLegacyNameBuffer(consoleFontInfoEx.FaceName);

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
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        auto& activeScreenInfo = context.GetActiveBuffer();

        WCHAR FaceName[ARRAYSIZE(consoleFontInfoEx.FaceName)];
        RETURN_IF_FAILED(StringCchCopyW(FaceName, ARRAYSIZE(FaceName), consoleFontInfoEx.FaceName));

        FontInfo fi(FaceName,
                    gsl::narrow_cast<unsigned char>(consoleFontInfoEx.FontFamily),
                    consoleFontInfoEx.FontWeight,
                    til::wrap_coord_size(consoleFontInfoEx.dwFontSize),
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
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        const auto oldQuickEditMode{ WI_IsFlagSet(gci.Flags, CONSOLE_QUICK_EDIT_MODE) };

        if (WI_IsAnyFlagSet(mode, PRIVATE_MODES))
        {
            WI_SetFlag(gci.Flags, CONSOLE_USE_PRIVATE_FLAGS);

            WI_UpdateFlag(gci.Flags, CONSOLE_QUICK_EDIT_MODE, WI_IsFlagSet(mode, ENABLE_QUICK_EDIT_MODE));
            WI_UpdateFlag(gci.Flags, CONSOLE_AUTO_POSITION, WI_IsFlagSet(mode, ENABLE_AUTO_POSITION));

            const auto PreviousInsertMode = gci.GetInsertMode();
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

        const auto newQuickEditMode{ WI_IsFlagSet(gci.Flags, CONSOLE_QUICK_EDIT_MODE) };

        // Mouse input should be received when mouse mode is on and quick edit mode is off
        // (for more information regarding the quirks of mouse mode and why/how it relates
        //  to quick edit mode, see GH#9970)
        const auto oldMouseMode{ !oldQuickEditMode && WI_IsFlagSet(context.InputMode, ENABLE_MOUSE_INPUT) };
        const auto newMouseMode{ !newQuickEditMode && WI_IsFlagSet(mode, ENABLE_MOUSE_INPUT) };

        if (oldMouseMode != newMouseMode)
        {
            gci.GetActiveInputBuffer()->PassThroughWin32MouseRequest(newMouseMode);
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
            RETURN_HR_IF_EXPECTED(E_INVALIDARG, WI_IsFlagSet(mode, ENABLE_ECHO_INPUT) && WI_IsFlagClear(mode, ENABLE_LINE_INPUT));
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
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        // Flags we don't understand are invalid.
        RETURN_HR_IF(E_INVALIDARG, WI_IsAnyFlagSet(mode, ~OUTPUT_MODES));

        auto& screenInfo = context.GetActiveBuffer();
        const auto dwOldMode = screenInfo.OutputMode;
        const auto dwNewMode = mode;

        screenInfo.OutputMode = dwNewMode;

        // if we're moving from VT on->off
        if (WI_IsFlagClear(dwNewMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING) &&
            WI_IsFlagSet(dwOldMode, ENABLE_VIRTUAL_TERMINAL_PROCESSING))
        {
            // jiggle the handle
            screenInfo.GetStateMachine().ResetState();
        }

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
                                                  til::size& size) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        const auto& screenInfo = context.GetActiveBuffer();

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
                                                                  const til::size size) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        auto& screenInfo = context.GetActiveBuffer();

        // microsoft/terminal#3907 - We shouldn't resize the buffer to be
        // smaller than the viewport. This was previously erroneously checked
        // when the host was not in conpty mode.
        RETURN_HR_IF(E_INVALIDARG, (size.width < screenInfo.GetViewport().Width() || size.height < screenInfo.GetViewport().Height()));

        // see MSFT:17415266
        // We only really care about the minimum window size if we have a head.
        if (!ServiceLocator::LocateGlobals().IsHeadless())
        {
            const auto coordMin = screenInfo.GetMinWindowSizeInCharacters();
            // Make sure requested screen buffer size isn't smaller than the window.
            RETURN_HR_IF(E_INVALIDARG, (size.height < coordMin.height || size.width < coordMin.width));
        }

        // Ensure the requested size isn't larger than we can handle in our data type.
        RETURN_HR_IF(E_INVALIDARG, (size.width == SHORT_MAX || size.height == SHORT_MAX));

        // Only do the resize if we're actually changing one of the dimensions
        const auto coordScreenBufferSize = screenInfo.GetBufferSize().Dimensions();
        if (size.width != coordScreenBufferSize.width || size.height != coordScreenBufferSize.height)
        {
            RETURN_IF_NTSTATUS_FAILED(screenInfo.ResizeScreenBuffer(size, TRUE));
        }

        // Make sure the viewport doesn't now overflow the buffer dimensions.
        auto overflow = screenInfo.GetViewport().BottomRightExclusive() - screenInfo.GetBufferSize().Dimensions();
        if (overflow.x > 0 || overflow.y > 0)
        {
            overflow = { -std::max(overflow.x, 0), -std::max(overflow.y, 0) };
            RETURN_IF_NTSTATUS_FAILED(screenInfo.SetViewportOrigin(false, overflow, false));
        }

        // And also that the cursor position is clamped within the buffer boundaries.
        auto& cursor = screenInfo.GetTextBuffer().GetCursor();
        auto clampedCursorPosition = cursor.GetPosition();
        screenInfo.GetBufferSize().Clamp(clampedCursorPosition);
        if (clampedCursorPosition != cursor.GetPosition())
        {
            cursor.SetPosition(clampedCursorPosition);
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

        auto& g = ServiceLocator::LocateGlobals();
        auto& gci = g.getConsoleInformation();

        const auto coordScreenBufferSize = context.GetBufferSize().Dimensions();
        const auto requestedBufferSize = til::wrap_coord_size(data.dwSize);
        if (requestedBufferSize != coordScreenBufferSize)
        {
            auto& commandLine = CommandLine::Instance();

            commandLine.Hide(FALSE);

            LOG_IF_FAILED(context.ResizeScreenBuffer(requestedBufferSize, TRUE));

            commandLine.Show();
        }
        const auto newBufferSize = context.GetBufferSize().Dimensions();

        bool changedOneTableEntry = false;
        for (size_t i = 0; i < std::size(data.ColorTable); i++)
        {
            // Check if we actually changed a palette color
            const auto& newColor{ data.ColorTable[i] };
            changedOneTableEntry = changedOneTableEntry || (newColor != gci.GetColorTableEntry(i));

            // Set the new value.
            gci.SetLegacyColorTableEntry(i, newColor);
        }

        // GH#399: Trigger a redraw, so that updated colors are repainted, but
        // only do this if we're not in conpty mode. ConPTY will update the
        // colors of the palette elsewhere (after TODO GH#10639)
        //
        // Only do this if we actually changed the value of the palette though -
        // this API gets called all the time to change all sorts of things, but
        // not necessarily the palette.
        if (changedOneTableEntry && !gci.IsInVtIoMode())
        {
            if (auto* pRender{ ServiceLocator::LocateGlobals().pRender })
            {
                pRender->TriggerRedrawAll();
            }
        }

        context.SetDefaultAttributes(TextAttribute{ data.wAttributes }, TextAttribute{ data.wPopupAttributes });

        const auto requestedViewport = Viewport::FromExclusive(til::wrap_exclusive_small_rect(data.srWindow));

        auto NewSize = requestedViewport.Dimensions();
        // If we have a window, clamp the requested viewport to the max window size
        if (!ServiceLocator::LocateGlobals().IsHeadless())
        {
            NewSize.width = std::min<til::CoordType>(NewSize.width, data.dwMaximumWindowSize.X);
            NewSize.height = std::min<til::CoordType>(NewSize.height, data.dwMaximumWindowSize.Y);
        }

        // If wrap text is on, then the window width must be the same size as the buffer width
        if (gci.GetWrapText())
        {
            NewSize.width = newBufferSize.width;
        }

        if (NewSize.width != context.GetViewport().Width() ||
            NewSize.height != context.GetViewport().Height())
        {
            // GH#1856 - make sure to hide the commandline _before_ we execute
            // the resize, and the re-display it after the resize. If we leave
            // it displayed, we'll crash during the resize when we try to figure
            // out if the bounds of the old commandline fit within the new
            // window (it might not).
            auto& commandLine = CommandLine::Instance();
            commandLine.Hide(FALSE);
            context.SetViewportSize(&NewSize);
            commandLine.Show();

            const auto pWindow = ServiceLocator::LocateConsoleWindow();
            if (pWindow != nullptr)
            {
                pWindow->UpdateWindowSize(NewSize);
            }
        }

        // Despite the fact that this API takes in a srWindow for the viewport, it traditionally actually doesn't set
        //  anything using that member - for moving the viewport, you need SetConsoleWindowInfo
        //  (see https://msdn.microsoft.com/en-us/library/windows/desktop/ms686125(v=vs.85).aspx and DoSrvSetConsoleWindowInfo)
        // Note that it also doesn't set cursor position.

        // However, we do need to make sure the viewport doesn't now overflow the buffer dimensions.
        auto overflow = context.GetViewport().BottomRightExclusive() - context.GetBufferSize().Dimensions();
        if (overflow.x > 0 || overflow.y > 0)
        {
            overflow = { -std::max(overflow.x, 0), -std::max(overflow.y, 0) };
            RETURN_IF_NTSTATUS_FAILED(context.SetViewportOrigin(false, overflow, false));
        }

        // And also that the cursor position is clamped within the buffer boundaries.
        auto& cursor = context.GetTextBuffer().GetCursor();
        auto clampedCursorPosition = cursor.GetPosition();
        context.GetBufferSize().Clamp(clampedCursorPosition);
        if (clampedCursorPosition != cursor.GetPosition())
        {
            cursor.SetPosition(clampedCursorPosition);
        }

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
                                                                const til::point position) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        auto& buffer = context.GetActiveBuffer();

        const auto coordScreenBufferSize = buffer.GetBufferSize().Dimensions();
        // clang-format off
        RETURN_HR_IF(E_INVALIDARG, (position.x >= coordScreenBufferSize.width ||
                                    position.y >= coordScreenBufferSize.height ||
                                    position.x < 0 ||
                                    position.y < 0));
        // clang-format on

        // MSFT: 15813316 - Try to use this SetCursorPosition call to inherit the cursor position.
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        RETURN_IF_FAILED(gci.GetVtIo()->SetCursorPosition(position));

        RETURN_IF_NTSTATUS_FAILED(buffer.SetCursorPosition(position, true));

        LOG_IF_FAILED(ConsoleImeResizeCompStrView());

        // Attempt to "snap" the viewport to the cursor position. If the cursor
        // is not in the current viewport, we'll try and move the viewport so
        // that the cursor is visible.
        // GH#1222 and GH#9754 - Use the "virtual" viewport here, so that the
        // viewport snaps back to the virtual viewport's location.
        const auto currentViewport = buffer.GetVirtualViewport().ToInclusive();
        til::point delta;
        {
            // When evaluating the X offset, we must convert the buffer position to
            // equivalent screen coordinates, taking line rendition into account.
            const auto lineRendition = buffer.GetTextBuffer().GetLineRendition(position.y);
            const auto screenPosition = BufferToScreenLine({ position.x, position.y, position.x, position.y }, lineRendition);

            if (currentViewport.left > screenPosition.left)
            {
                delta.x = screenPosition.left - currentViewport.left;
            }
            else if (currentViewport.right < screenPosition.right)
            {
                delta.x = screenPosition.right - currentViewport.right;
            }

            if (currentViewport.top > position.y)
            {
                delta.y = position.y - currentViewport.top;
            }
            else if (currentViewport.bottom < position.y)
            {
                delta.y = position.y - currentViewport.bottom;
            }
        }

        til::point newWindowOrigin;
        newWindowOrigin.x = currentViewport.left + delta.x;
        newWindowOrigin.y = currentViewport.top + delta.y;
        // SetViewportOrigin will worry about clamping these values to the
        // buffer for us.
        RETURN_IF_NTSTATUS_FAILED(buffer.SetViewportOrigin(true, newWindowOrigin, true));

        // SetViewportOrigin will only move the virtual bottom down, but in
        // this particular case we also need to allow the virtual bottom to
        // be moved up, so we have to call UpdateBottom explicitly. This is
        // how the cmd shell's CLS command resets the buffer.
        buffer.UpdateBottom();

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
                                                            const til::inclusive_rect& windowRect) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        auto& g = ServiceLocator::LocateGlobals();
        auto Window = windowRect;

        if (!isAbsolute)
        {
            auto currentViewport = context.GetViewport().ToInclusive();
            Window.left += currentViewport.left;
            Window.right += currentViewport.right;
            Window.top += currentViewport.top;
            Window.bottom += currentViewport.bottom;
        }

        RETURN_HR_IF(E_INVALIDARG, (Window.right < Window.left || Window.bottom < Window.top));

        til::size NewWindowSize;
        NewWindowSize.width = CalcWindowSizeX(Window);
        NewWindowSize.height = CalcWindowSizeY(Window);

        // see MSFT:17415266
        // If we have a actual head, we care about the maximum size the window can be.
        // if we're headless, not so much. However, GetMaxWindowSizeInCharacters
        //      will only return the buffer size, so we can't use that to clip the arg here.
        // So only clip the requested size if we're not headless
        if (g.getConsoleInformation().IsInVtIoMode())
        {
            // SetViewportRect doesn't cause the buffer to resize. Manually resize the buffer.
            RETURN_IF_NTSTATUS_FAILED(context.ResizeScreenBuffer(Viewport::FromInclusive(Window).Dimensions(), false));
        }
        if (!g.IsHeadless())
        {
            const auto coordMax = context.GetMaxWindowSizeInCharacters();
            RETURN_HR_IF(E_INVALIDARG, (NewWindowSize.width > coordMax.width || NewWindowSize.height > coordMax.height));
        }

        // Even if it's the same size, we need to post an update in case the scroll bars need to go away.
        context.SetViewport(Viewport::FromInclusive(Window), true);
        if (context.IsActiveScreenBuffer())
        {
            // TODO: MSFT: 9574827 - shouldn't we be looking at or at least logging the failure codes here? (Or making them non-void?)
            context.PostUpdateWindowSize();

            // Use WriteToScreen to invalidate the viewport with the renderer.
            // GH#3490 - If we're in conpty mode, don't invalidate the entire
            // viewport. In conpty mode, the VtEngine will later decide what
            // part of the buffer actually needs to be re-sent to the terminal.
            if (!(g.getConsoleInformation().IsInVtIoMode() && g.getConsoleInformation().GetVtIo()->IsResizeQuirkEnabled()))
            {
                WriteToScreen(context, context.GetViewport());
            }
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
                                                                  const til::inclusive_rect& source,
                                                                  const til::point target,
                                                                  std::optional<til::inclusive_rect> clip,
                                                                  const char fillCharacter,
                                                                  const WORD fillAttribute) noexcept
{
    try
    {
        const auto unicodeFillCharacter = CharToWchar(&fillCharacter, 1);

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
// - enableCmdShim - true iff the client process that's calling this
//   method is "cmd.exe". Used to enable certain compatibility shims for
//   conpty mode. See GH#3126.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT ApiRoutines::ScrollConsoleScreenBufferWImpl(SCREEN_INFORMATION& context,
                                                                  const til::inclusive_rect& source,
                                                                  const til::point target,
                                                                  std::optional<til::inclusive_rect> clip,
                                                                  const wchar_t fillCharacter,
                                                                  const WORD fillAttribute,
                                                                  const bool enableCmdShim) noexcept
{
    try
    {
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        auto& buffer = context.GetActiveBuffer();

        TextAttribute useThisAttr(fillAttribute);
        ScrollRegion(buffer, source, clip, target, fillCharacter, useThisAttr);

        auto hr = S_OK;

        // GH#3126 - This is a shim for cmd's `cls` function. In the
        // legacy console, `cls` is supposed to clear the entire buffer. In
        // conpty however, there's no difference between the viewport and the
        // entirety of the buffer. We're going to see if this API call exactly
        // matched the way we expect cmd to call it. If it does, then
        // let's manually emit a ^[[3J to the connected terminal, so that their
        // entire buffer will be cleared as well.
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        if (enableCmdShim && gci.IsInVtIoMode())
        {
            const auto currentBufferDimensions = buffer.GetBufferSize().Dimensions();
            const auto sourceIsWholeBuffer = (source.top == 0) &&
                                             (source.left == 0) &&
                                             (source.right == currentBufferDimensions.width) &&
                                             (source.bottom == currentBufferDimensions.height);
            const auto targetIsNegativeBufferHeight = (target.x == 0) &&
                                                      (target.y == -currentBufferDimensions.height);
            const auto noClipProvided = clip == std::nullopt;
            const auto fillIsBlank = (fillCharacter == UNICODE_SPACE) &&
                                     (fillAttribute == buffer.GetAttributes().GetLegacyAttributes());

            if (sourceIsWholeBuffer && targetIsNegativeBufferHeight && noClipProvided && fillIsBlank)
            {
                // It's important that we flush the renderer at this point so we don't
                // have any pending output rendered after the scrollback is cleared.
                ServiceLocator::LocateGlobals().pRender->TriggerFlush(false);
                hr = gci.GetVtIo()->ManuallyClearScrollback();
            }
        }

        return hr;
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
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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

[[nodiscard]] HRESULT DoSrvSetConsoleOutputCodePage(const unsigned int codepage)
{
    auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

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
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });
        return DoSrvSetConsoleOutputCodePage(codepage);
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
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        LockConsole();
        auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

        codepage = gci.CP;
    }
    CATCH_LOG();
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
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        codepage = gci.OutputCP;
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
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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
        auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
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

        const auto pWindow = ServiceLocator::LocateConsoleWindow();
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
                                                             til::size& newSize) noexcept
{
    try
    {
        // SetIsFullscreen() below ultimately calls SetwindowLong, which ultimately calls SendMessage(). If we retain
        // the console lock, we'll deadlock since ConsoleWindowProc takes the lock before processing messages. Instead,
        // we'll release early.
        LockConsole();
        {
            auto Unlock = wil::scope_exit([&] { UnlockConsole(); });

            auto& screenInfo = context.GetActiveBuffer();

            newSize = screenInfo.GetBufferSize().Dimensions();
            RETURN_HR_IF(E_INVALIDARG, !(screenInfo.IsActiveScreenBuffer()));
        }

        const auto pWindow = ServiceLocator::LocateConsoleWindow();
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
// - Gets title information from the console. It can be truncated if the buffer is too small.
// Arguments:
// - title - If given, this buffer is filled with the title information requested.
//         - Use nullopt to request buffer size required.
// - written - The number of characters filled in the title buffer.
// - needed - The number of characters we would need to completely write out the title.
// - isOriginal - If true, gets the title when we booted up. If false, gets whatever it is set to right now.
// Return Value:
// - S_OK, E_INVALIDARG, or failure code from thrown exception
[[nodiscard]] HRESULT GetConsoleTitleWImplHelper(std::optional<std::span<wchar_t>> title,
                                                 size_t& written,
                                                 size_t& needed,
                                                 const bool isOriginal) noexcept
{
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // Ensure output variables are initialized.
        written = 0;
        needed = 0;

        if (title.has_value() && title->size() > 0)
        {
            til::at(*title, 0) = ANSI_NULL;
        }

        // Get the appropriate title and length depending on the mode.
        const auto storedTitle = isOriginal ? gci.GetOriginalTitle() : gci.GetTitle();

        // Always report how much space we would need.
        needed = storedTitle.size();

        // If we have a pointer to receive the data, then copy it out.
        if (title.has_value())
        {
            const auto hr = StringCchCopyNW(title->data(), title->size(), storedTitle.data(), storedTitle.size());

            // Insufficient buffer is allowed. If we return a partial string, that's still OK by historical/compat standards.
            // Just say how much we managed to return.
            if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr)
            {
                written = std::min(title->size(), storedTitle.size());
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

[[nodiscard]] HRESULT GetConsoleTitleAImplHelper(std::span<char> title,
                                                 size_t& written,
                                                 size_t& needed,
                                                 const bool isOriginal) noexcept
{
    try
    {
        const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();
        // Ensure output variables are initialized.
        written = 0;
        needed = 0;

        if (title.size() > 0)
        {
            til::at(title, 0) = ANSI_NULL;
        }

        // Figure out how big our temporary Unicode buffer must be to get the title.
        size_t unicodeNeeded;
        size_t unicodeWritten;
        RETURN_IF_FAILED(GetConsoleTitleWImplHelper(std::nullopt, unicodeWritten, unicodeNeeded, isOriginal));

        // If there's nothing to get, then simply return.
        RETURN_HR_IF(S_OK, 0 == unicodeNeeded);

        // Allocate a unicode buffer of the right size.
        const auto unicodeSize = unicodeNeeded + 1; // add one for null terminator space
        auto unicodeBuffer = std::make_unique<wchar_t[]>(unicodeSize);
        RETURN_IF_NULL_ALLOC(unicodeBuffer);

        const std::span<wchar_t> unicodeSpan(unicodeBuffer.get(), unicodeSize);

        // Retrieve the title in Unicode.
        RETURN_IF_FAILED(GetConsoleTitleWImplHelper(unicodeSpan, unicodeWritten, unicodeNeeded, isOriginal));

        // Convert result to A
        const auto converted = ConvertToA(gci.CP, { unicodeBuffer.get(), unicodeWritten });

        // The legacy A behavior is a bit strange. If the buffer given doesn't have enough space to hold
        // the string without null termination (e.g. the title is 9 long, 10 with null. The buffer given isn't >= 9).
        // then do not copy anything back and do not report how much space we need.
        if (title.size() >= converted.size())
        {
            // Say how many characters of buffer we would need to hold the entire result.
            needed = converted.size();

            // Copy safely to output buffer
            const auto hr = StringCchCopyNA(title.data(), title.size(), converted.data(), converted.size());

            // Insufficient buffer is allowed. If we return a partial string, that's still OK by historical/compat standards.
            // Just say how much we managed to return.
            if (SUCCEEDED(hr) || STRSAFE_E_INSUFFICIENT_BUFFER == hr)
            {
                // And return the size copied (either the size of the buffer or the null terminated length of the string we filled it with.)
                written = std::min(title.size(), converted.size() + 1);

                // Another compatibility fix... If we had exactly the number of bytes needed for an unterminated string,
                // then replace the terminator left behind by StringCchCopyNA with the final character of the title string.
                if (title.size() == converted.size())
                {
                    title.back() = converted.back();
                }
            }
        }
        else
        {
            // If we didn't copy anything back and there is space, null terminate the given buffer and return.
            if (title.size() > 0)
            {
                til::at(title, 0) = ANSI_NULL;
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
[[nodiscard]] HRESULT ApiRoutines::GetConsoleTitleAImpl(std::span<char> title,
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
[[nodiscard]] HRESULT ApiRoutines::GetConsoleTitleWImpl(std::span<wchar_t> title,
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
[[nodiscard]] HRESULT ApiRoutines::GetConsoleOriginalTitleAImpl(std::span<char> title,
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
[[nodiscard]] HRESULT ApiRoutines::GetConsoleOriginalTitleWImpl(std::span<wchar_t> title,
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
    const auto& gci = ServiceLocator::LocateGlobals().getConsoleInformation();

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

    ServiceLocator::LocateGlobals().getConsoleInformation().SetTitle(title);
    return S_OK;
}
