// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Terminal.hpp"
#include "../src/inc/unicode.hpp"

using namespace Microsoft::Terminal::Core;
using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;
using namespace Microsoft::Console::VirtualTerminal;

// Print puts the text in the buffer and moves the cursor
void Terminal::PrintString(const std::wstring_view string)
{
    _WriteBuffer(string);
}

void Terminal::ReturnResponse(const std::wstring_view response)
{
    if (_pfnWriteInput)
    {
        _pfnWriteInput(response);
    }
}

Microsoft::Console::VirtualTerminal::StateMachine& Terminal::GetStateMachine()
{
    return *_stateMachine;
}

TextBuffer& Terminal::GetTextBuffer()
{
    return _activeBuffer();
}

til::rect Terminal::GetViewport() const
{
    return til::rect{ _GetMutableViewport().ToInclusive() };
}

void Terminal::SetViewportPosition(const til::point position)
{
    const auto dimensions = _GetMutableViewport().Dimensions();
    _mutableViewport = Viewport::FromDimensions(position.to_win32_coord(), dimensions);
    Terminal::_NotifyScrollEvent();
}

void Terminal::SetTextAttributes(const TextAttribute& attrs)
{
    _activeBuffer().SetCurrentAttributes(attrs);
}

void Terminal::SetAutoWrapMode(const bool /*wrapAtEOL*/)
{
    // TODO: This will be needed to support DECAWM.
}

void Terminal::SetScrollingRegion(const til::inclusive_rect& /*scrollMargins*/)
{
    // TODO: This will be needed to fully support DECSTBM.
}

void Terminal::WarningBell()
{
    _pfnWarningBell();
}

bool Terminal::GetLineFeedMode() const
{
    // TODO: This will be needed to support LNM.
    return false;
}

void Terminal::LineFeed(const bool withReturn)
{
    auto cursorPos = _activeBuffer().GetCursor().GetPosition();

    // since we explicitly just moved down a row, clear the wrap status on the
    // row we just came from
    _activeBuffer().GetRowByOffset(cursorPos.Y).SetWrapForced(false);

    cursorPos.Y++;
    if (withReturn)
    {
        cursorPos.X = 0;
    }
    _AdjustCursorPosition(cursorPos);
}

void Terminal::SetWindowTitle(const std::wstring_view title)
{
    if (!_suppressApplicationTitle)
    {
        _title.emplace(title);
        _pfnTitleChanged(_title.value());
    }
}

CursorType Terminal::GetUserDefaultCursorStyle() const
{
    return _defaultCursorShape;
}

bool Terminal::ResizeWindow(const size_t /*width*/, const size_t /*height*/)
{
    // TODO: This will be needed to support various resizing sequences. See also GH#1860.
    return false;
}

void Terminal::SetConsoleOutputCP(const unsigned int /*codepage*/)
{
    // TODO: This will be needed to support 8-bit charsets and DOCS sequences.
}

unsigned int Terminal::GetConsoleOutputCP() const
{
    // TODO: See SetConsoleOutputCP above.
    return CP_UTF8;
}

void Terminal::EnableXtermBracketedPasteMode(const bool enabled)
{
    _bracketedPasteMode = enabled;
}

void Terminal::CopyToClipboard(std::wstring_view content)
{
    _pfnCopyToClipboard(content);
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
    _workingDirectory = uri;
}

void Terminal::UseAlternateScreenBuffer()
{
    // the new alt buffer is exactly the size of the viewport.
    _altBufferSize = til::size{ _mutableViewport.Dimensions() };

    const auto cursorSize = _mainBuffer->GetCursor().GetSize();

    ClearSelection();
    _mainBuffer->ClearPatternRecognizers();

    // Create a new alt buffer
    _altBuffer = std::make_unique<TextBuffer>(_altBufferSize.to_win32_coord(),
                                              TextAttribute{},
                                              cursorSize,
                                              true,
                                              _mainBuffer->GetRenderer());
    _mainBuffer->SetAsActiveBuffer(false);

    // Copy our cursor state to the new buffer's cursor
    {
        // Update the alt buffer's cursor style, visibility, and position to match our own.
        auto& myCursor = _mainBuffer->GetCursor();
        auto& tgtCursor = _altBuffer->GetCursor();
        tgtCursor.SetStyle(myCursor.GetSize(), myCursor.GetType());
        tgtCursor.SetIsVisible(myCursor.IsVisible());
        tgtCursor.SetBlinkingAllowed(myCursor.IsBlinkingAllowed());

        // The new position should match the viewport-relative position of the main buffer.
        auto tgtCursorPos = myCursor.GetPosition();
        tgtCursorPos.Y -= _mutableViewport.Top();
        tgtCursor.SetPosition(tgtCursorPos);
    }

    // update all the hyperlinks on the screen
    _updateUrlDetection();

    // GH#3321: Make sure we let the TerminalInput know that we switched
    // buffers. This might affect how we interpret certain mouse events.
    _terminalInput->UseAlternateScreenBuffer();

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
    // Short-circuit: do nothing.
    if (!_inAltBuffer())
    {
        return;
    }

    ClearSelection();

    // Copy our cursor state back to the main buffer's cursor
    {
        // Update the alt buffer's cursor style, visibility, and position to match our own.
        auto& myCursor = _altBuffer->GetCursor();
        auto& tgtCursor = _mainBuffer->GetCursor();
        tgtCursor.SetStyle(myCursor.GetSize(), myCursor.GetType());
        tgtCursor.SetIsVisible(myCursor.IsVisible());
        tgtCursor.SetBlinkingAllowed(myCursor.IsBlinkingAllowed());

        // The new position should match the viewport-relative position of the main buffer.
        // This is the equal and opposite effect of what we did in UseAlternateScreenBuffer
        auto tgtCursorPos = myCursor.GetPosition();
        tgtCursorPos.Y += _mutableViewport.Top();
        tgtCursor.SetPosition(tgtCursorPos);
    }

    _mainBuffer->SetAsActiveBuffer(true);
    // destroy the alt buffer
    _altBuffer = nullptr;

    if (_deferredResize.has_value())
    {
        LOG_IF_FAILED(UserResize(_deferredResize.value().to_win32_coord()));
        _deferredResize = std::nullopt;
    }

    // update all the hyperlinks on the screen
    _mainBuffer->ClearPatternRecognizers();
    _updateUrlDetection();

    // GH#3321: Make sure we let the TerminalInput know that we switched
    // buffers. This might affect how we interpret certain mouse events.
    _terminalInput->UseMainScreenBuffer();

    // Update scrollbars
    _NotifyScrollEvent();

    // redraw the screen
    try
    {
        _activeBuffer().TriggerRedrawAll();
    }
    CATCH_LOG();
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

bool Terminal::IsConsolePty() const
{
    return false;
}

bool Terminal::IsVtInputEnabled() const
{
    return false;
}

void Terminal::NotifyAccessibilityChange(const til::rect& /*changedRect*/)
{
    // This is only needed in conhost. Terminal handles accessibility in another way.
}
