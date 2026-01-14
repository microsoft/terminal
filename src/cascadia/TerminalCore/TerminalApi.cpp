// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "tracing.hpp"

#include "../src/inc/unicode.hpp"

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

// Note: Generate GUID using TlgGuid.exe tool
#pragma warning(suppress : 26477) // One of the macros uses 0/NULL. We don't have control to make it nullptr.
TRACELOGGING_DEFINE_PROVIDER(g_hCTerminalCoreProvider,
                             "Microsoft.Terminal.Core",
                             // {103ac8cf-97d2-51aa-b3ba-5ffd5528fa5f}
                             (0x103ac8cf, 0x97d2, 0x51aa, 0xb3, 0xba, 0x5f, 0xfd, 0x55, 0x28, 0xfa, 0x5f),
                             TraceLoggingOptionMicrosoftTelemetry());

void Terminal::ReturnResponse(const std::wstring_view response)
{
    if (_pfnWriteInput && !response.empty())
    {
        _pfnWriteInput(response);
    }
}

Microsoft::Console::VirtualTerminal::StateMachine& Terminal::GetStateMachine() noexcept
{
    return *_stateMachine;
}

ITerminalApi::BufferState Terminal::GetBufferAndViewport() noexcept
{
    return { _activeBuffer(), til::rect{ _GetMutableViewport().ToInclusive() }, !_inAltBuffer() };
}

void Terminal::SetViewportPosition(til::point position) noexcept
try
{
    // The viewport is fixed at 0,0 for the alt buffer, so this is a no-op.
    if (!_inAltBuffer())
    {
        const auto bufferSize = _mainBuffer->GetSize().Dimensions();
        const auto viewSize = _GetMutableViewport().Dimensions();

        // Ensure the given position is in bounds.
        position.x = std::clamp(position.x, 0, bufferSize.width - viewSize.width);
        position.y = std::clamp(position.y, 0, bufferSize.height - viewSize.height);

        const auto viewportDelta = position.y - _GetMutableViewport().Origin().y;
        _mutableViewport = Viewport::FromDimensions(position, viewSize);
        _PreserveUserScrollOffset(viewportDelta);
        _NotifyScrollEvent();
    }
}
CATCH_LOG()

void Terminal::SetSystemMode(const Mode mode, const bool enabled) noexcept
{
    _assertLocked();
    _systemMode.set(mode, enabled);
}

bool Terminal::GetSystemMode(const Mode mode) const noexcept
{
    _assertLocked();
    return _systemMode.test(mode);
}

void Terminal::ReturnAnswerback()
{
    ReturnResponse(_answerbackMessage);
}

void Terminal::WarningBell()
{
    _pfnWarningBell();
}

void Terminal::SetWindowTitle(const std::wstring_view title)
{
    _assertLocked();
    if (!_suppressApplicationTitle)
    {
        _title.emplace(title.empty() ? _startingTitle : title);
        _pfnTitleChanged(_title.value());
    }
}

CursorType Terminal::GetUserDefaultCursorStyle() const noexcept
{
    _assertLocked();
    return _defaultCursorShape;
}

bool Terminal::ResizeWindow(const til::CoordType width, const til::CoordType height)
{
    // TODO: This will be needed to support various resizing sequences. See also GH#1860.
    _assertLocked();

    if (width <= 0 || height <= 0 || width > SHRT_MAX || height > SHRT_MAX)
    {
        return false;
    }

    if (_pfnWindowSizeChanged)
    {
        _pfnWindowSizeChanged(width, height);
        return true;
    }

    return false;
}

void Terminal::SetCodePage(const unsigned int /*codepage*/) noexcept
{
    // Code pages are dealt with in ConHost, so this isn't needed.
}

void Terminal::ResetCodePage() noexcept
{
    // There is nothing to reset, since the code page never changes.
}

unsigned int Terminal::GetOutputCodePage() const noexcept
{
    // See above. The code page is always UTF-8.
    return CP_UTF8;
}

unsigned int Terminal::GetInputCodePage() const noexcept
{
    // See above. The code page is always UTF-8.
    return CP_UTF8;
}

void Terminal::CopyToClipboard(wil::zwstring_view content)
{
    if (_clipboardOperationsAllowed)
    {
        _pfnCopyToClipboard(content);
    }
}

// Method Description:
// - Updates the taskbar progress indicator
// Arguments:
// - state: indicates the progress state
// - progress: indicates the progress value
// Return Value:
// - <none>
void Terminal::SetTaskbarProgress(const ::Microsoft::Console::VirtualTerminal::DispatchTypes::TaskbarState state, const size_t progress)
{
    _assertLocked();

    _taskbarState = static_cast<size_t>(state);

    switch (state)
    {
    case DispatchTypes::TaskbarState::Clear:
        // Always set progress to 0 in this case
        _taskbarProgress = 0;
        break;
    case DispatchTypes::TaskbarState::Set:
        // Always set progress to the value given in this case
        _taskbarProgress = progress;
        break;
    case DispatchTypes::TaskbarState::Indeterminate:
        // Leave the progress value unchanged in this case
        break;
    case DispatchTypes::TaskbarState::Error:
    case DispatchTypes::TaskbarState::Paused:
        // In these 2 cases, if the given progress value is 0, then
        // leave the progress value unchanged, unless the current progress
        // value is 0, in which case set it to a 'minimum' value (10 in our case);
        // if the given progress value is greater than 0, then set the progress value
        if (progress == 0)
        {
            if (_taskbarProgress == 0)
            {
                _taskbarProgress = TaskbarMinProgress;
            }
        }
        else
        {
            _taskbarProgress = progress;
        }
        break;
    }

    if (_pfnTaskbarProgressChanged)
    {
        _pfnTaskbarProgressChanged();
    }
}

void Terminal::SetWorkingDirectory(std::wstring_view uri)
{
    _assertLocked();

    static bool logged = false;
    if (!logged)
    {
        TraceLoggingWrite(
            g_hCTerminalCoreProvider,
            "ShellIntegrationWorkingDirSet",
            TraceLoggingDescription("The CWD was set by the client application"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        logged = true;
    }

    _workingDirectory = uri;
}

void Terminal::PlayMidiNote(const int noteNumber, const int velocity, const std::chrono::microseconds duration)
{
    _pfnPlayMidiNote(noteNumber, velocity, duration);
}

void Terminal::UseAlternateScreenBuffer(const TextAttribute& attrs)
{
    _assertLocked();

    // the new alt buffer is exactly the size of the viewport.
    _altBufferSize = _mutableViewport.Dimensions();

    const auto cursorSize = _mainBuffer->GetCursor().GetSize();

    ClearSelection();

    // Create a new alt buffer
    _altBuffer = std::make_unique<TextBuffer>(_altBufferSize,
                                              attrs,
                                              cursorSize,
                                              true,
                                              _mainBuffer->GetRenderer());
    _mainBuffer->SetAsActiveBuffer(false);

    // Copy our cursor state to the new buffer's cursor
    {
        // Update the alt buffer's cursor style, visibility, and position to match our own.
        const auto& myCursor = _mainBuffer->GetCursor();
        auto& tgtCursor = _altBuffer->GetCursor();
        tgtCursor.SetStyle(myCursor.GetSize(), myCursor.GetType());
        tgtCursor.SetIsVisible(myCursor.IsVisible());
        tgtCursor.SetIsBlinking(myCursor.IsBlinking());

        // The new position should match the viewport-relative position of the main buffer.
        auto tgtCursorPos = myCursor.GetPosition();
        tgtCursorPos.y -= _mutableViewport.Top();
        tgtCursor.SetPosition(tgtCursorPos);
    }

    // update all the hyperlinks on the screen
    _updateUrlDetection();

    // GH#3321: Make sure we let the TerminalInput know that we switched
    // buffers. This might affect how we interpret certain mouse events.
    _getTerminalInput().UseAlternateScreenBuffer();

    // Update scrollbars
    _NotifyScrollEvent();

    // redraw the screen
    try
    {
        _activeBuffer().TriggerRedrawAll();
    }
    CATCH_LOG();
}
void Terminal::UseMainScreenBuffer()
{
    // To make UserResize() work as if we're back in the main buffer, we first need to unset
    // _altBuffer, which is used throughout this class as an indicator via _inAltBuffer().
    //
    // We delay destroying the alt buffer instance to get a valid altBuffer->GetCursor() reference below.
    const auto altBuffer = std::exchange(_altBuffer, nullptr);
    if (!altBuffer)
    {
        return;
    }

    ClearSelection();

    _mainBuffer->SetAsActiveBuffer(true);

    if (_deferredResize.has_value())
    {
        LOG_IF_FAILED(UserResize(_deferredResize.value()));
        _deferredResize = std::nullopt;
    }

    // After exiting the alt buffer, the main buffer should adopt the current cursor position and style.
    // This is the equal and opposite effect of what we did in UseAlternateScreenBuffer and matches xterm.
    //
    // We have to do this after the call to UserResize() to ensure that the TextBuffer sizes match up.
    // Otherwise the cursor position may be temporarily out of bounds and some code may choke on that.
    {
        const auto& altCursor = altBuffer->GetCursor();
        auto& mainCursor = _mainBuffer->GetCursor();

        mainCursor.SetStyle(altCursor.GetSize(), altCursor.GetType());
        mainCursor.SetIsVisible(altCursor.IsVisible());
        mainCursor.SetIsBlinking(altCursor.IsBlinking());

        auto tgtCursorPos = altCursor.GetPosition();
        tgtCursorPos.y += _mutableViewport.Top();
        mainCursor.SetPosition(tgtCursorPos);
    }

    // update all the hyperlinks on the screen
    _updateUrlDetection();

    // GH#3321: Make sure we let the TerminalInput know that we switched
    // buffers. This might affect how we interpret certain mouse events.
    _getTerminalInput().UseMainScreenBuffer();

    // Update scrollbars
    _NotifyScrollEvent();

    // redraw the screen
    _activeBuffer().TriggerRedrawAll();
}

// Method Description:
// - Reacts to a client asking us to show or hide the window.
// Arguments:
// - showOrHide - True for show. False for hide.
// Return Value:
// - <none>
void Terminal::ShowWindow(bool showOrHide)
{
    if (_pfnShowWindowChanged)
    {
        _pfnShowWindowChanged(showOrHide);
    }
}

bool Terminal::IsVtInputEnabled() const noexcept
{
    return false;
}

void Terminal::InvokeCompletions(std::wstring_view menuJson, unsigned int replaceLength)
{
    if (_pfnCompletionsChanged)
    {
        _pfnCompletionsChanged(menuJson, replaceLength);
    }
}

void Terminal::SearchMissingCommand(const std::wstring_view command)
{
    if (_pfnSearchMissingCommand)
    {
        const auto bufferRow = _activeBuffer().GetCursor().GetPosition().y;
        _pfnSearchMissingCommand(command, bufferRow);
    }
}

void Terminal::NotifyBufferRotation(const int delta)
{
    // Update our selection, so it doesn't move as the buffer is cycled
    if (_selection->active)
    {
        auto selection{ _selection.write() };
        wil::hide_name _selection;
        // If the end of the selection will be out of range after the move, we just
        // clear the selection. Otherwise, we move both the start and end points up
        // by the given delta and clamp to the first row.
        if (selection->end.y < delta)
        {
            selection->active = false;
        }
        else
        {
            // Stash this, so we can make sure to update the pivot to match later.
            const auto pivotWasStart = selection->start == selection->pivot;
            selection->start.y = std::max(selection->start.y - delta, 0);
            selection->end.y = std::max(selection->end.y - delta, 0);
            // Make sure to sync the pivot with whichever value is the right one.
            selection->pivot = pivotWasStart ? selection->start : selection->end;
        }
    }

    // manually erase our pattern intervals since the locations have changed now
    _patternIntervalTree = {};

    const auto oldScrollOffset = _scrollOffset;
    _PreserveUserScrollOffset(delta);
    if (_scrollOffset != oldScrollOffset || AlwaysNotifyOnBufferRotation())
    {
        _NotifyScrollEvent();
    }
}

void Terminal::NotifyShellIntegrationMark()
{
    // Notify the scrollbar that marks have been added so it can refresh the mark indicators
    _NotifyScrollEvent();
}
