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
    if (_pfnWriteInput)
    {
        _pfnWriteInput(response);
    }
}

Microsoft::Console::VirtualTerminal::StateMachine& Terminal::GetStateMachine() noexcept
{
    return *_stateMachine;
}

TextBuffer& Terminal::GetTextBuffer() noexcept
{
    return _activeBuffer();
}

til::rect Terminal::GetViewport() const noexcept
{
    return til::rect{ _GetMutableViewport().ToInclusive() };
}

void Terminal::SetViewportPosition(const til::point position) noexcept
{
    // The viewport is fixed at 0,0 for the alt buffer, so this is a no-op.
    if (!_inAltBuffer())
    {
        const auto viewportDelta = position.y - _GetMutableViewport().Origin().y;
        const auto dimensions = _GetMutableViewport().Dimensions();
        _mutableViewport = Viewport::FromDimensions(position, dimensions);
        _PreserveUserScrollOffset(viewportDelta);
        _NotifyScrollEvent();
    }
}

void Terminal::SetTextAttributes(const TextAttribute& attrs) noexcept
{
    _activeBuffer().SetCurrentAttributes(attrs);
}

void Terminal::SetSystemMode(const Mode mode, const bool enabled) noexcept
{
    _systemMode.set(mode, enabled);
}

bool Terminal::GetSystemMode(const Mode mode) const noexcept
{
    return _systemMode.test(mode);
}

void Terminal::WarningBell()
{
    _pfnWarningBell();
}

void Terminal::SetWindowTitle(const std::wstring_view title)
{
    if (!_suppressApplicationTitle)
    {
        _title.emplace(title);
        _pfnTitleChanged(_title.value());
    }
}

CursorType Terminal::GetUserDefaultCursorStyle() const noexcept
{
    return _defaultCursorShape;
}

bool Terminal::ResizeWindow(const til::CoordType /*width*/, const til::CoordType /*height*/) noexcept
{
    // TODO: This will be needed to support various resizing sequences. See also GH#1860.
    return false;
}

void Terminal::SetConsoleOutputCP(const unsigned int /*codepage*/) noexcept
{
    // TODO: This will be needed to support 8-bit charsets and DOCS sequences.
}

unsigned int Terminal::GetConsoleOutputCP() const noexcept
{
    // TODO: See SetConsoleOutputCP above.
    return CP_UTF8;
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
    // the new alt buffer is exactly the size of the viewport.
    _altBufferSize = _mutableViewport.Dimensions();

    const auto cursorSize = _mainBuffer->GetCursor().GetSize();

    ClearSelection();
    _mainBuffer->ClearPatternRecognizers();

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
        tgtCursor.SetBlinkingAllowed(myCursor.IsBlinkingAllowed());

        // The new position should match the viewport-relative position of the main buffer.
        auto tgtCursorPos = myCursor.GetPosition();
        tgtCursorPos.y -= _mutableViewport.Top();
        tgtCursor.SetPosition(tgtCursorPos);
    }

    // update all the hyperlinks on the screen
    _updateUrlDetection();

    // GH#3321: Make sure we let the TerminalInput know that we switched
    // buffers. This might affect how we interpret certain mouse events.
    _terminalInput.UseAlternateScreenBuffer();

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
        const auto& myCursor = _altBuffer->GetCursor();
        auto& tgtCursor = _mainBuffer->GetCursor();
        tgtCursor.SetStyle(myCursor.GetSize(), myCursor.GetType());
        tgtCursor.SetIsVisible(myCursor.IsVisible());
        tgtCursor.SetBlinkingAllowed(myCursor.IsBlinkingAllowed());

        // The new position should match the viewport-relative position of the main buffer.
        // This is the equal and opposite effect of what we did in UseAlternateScreenBuffer
        auto tgtCursorPos = myCursor.GetPosition();
        tgtCursorPos.y += _mutableViewport.Top();
        tgtCursor.SetPosition(tgtCursorPos);
    }

    _mainBuffer->SetAsActiveBuffer(true);
    // destroy the alt buffer
    _altBuffer = nullptr;

    if (_deferredResize.has_value())
    {
        LOG_IF_FAILED(UserResize(_deferredResize.value()));
        _deferredResize = std::nullopt;
    }

    // update all the hyperlinks on the screen
    _mainBuffer->ClearPatternRecognizers();
    _updateUrlDetection();

    // GH#3321: Make sure we let the TerminalInput know that we switched
    // buffers. This might affect how we interpret certain mouse events.
    _terminalInput.UseMainScreenBuffer();

    // Update scrollbars
    _NotifyScrollEvent();

    // redraw the screen
    try
    {
        _activeBuffer().TriggerRedrawAll();
    }
    CATCH_LOG();
}

// NOTE: This is the version of AddMark that comes from VT
void Terminal::MarkPrompt(const ScrollMark& mark)
{
    static bool logged = false;
    if (!logged)
    {
        TraceLoggingWrite(
            g_hCTerminalCoreProvider,
            "ShellIntegrationMarkAdded",
            TraceLoggingDescription("A mark was added via VT at least once"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        logged = true;
    }

    const til::point cursorPos{ _activeBuffer().GetCursor().GetPosition() };
    AddMark(mark, cursorPos, cursorPos, false);

    if (mark.category == MarkCategory::Prompt)
    {
        _currentPromptState = PromptState::Prompt;
    }
}

void Terminal::MarkCommandStart()
{
    const til::point cursorPos{ _activeBuffer().GetCursor().GetPosition() };

    if ((_currentPromptState == PromptState::Prompt) &&
        (_activeBuffer().GetMarks().size() > 0))
    {
        // We were in the right state, and there's a previous mark to work
        // with.
        //
        //We can just do the work below safely.
    }
    else
    {
        // If there was no last mark, or we're in a weird state,
        // then abandon the current state, and just insert a new Prompt mark that
        // start's & ends here, and got to State::Command.

        ScrollMark mark;
        mark.category = MarkCategory::Prompt;
        AddMark(mark, cursorPos, cursorPos, false);
    }
    _activeBuffer().SetCurrentPromptEnd(cursorPos);
    _currentPromptState = PromptState::Command;
}

void Terminal::MarkOutputStart()
{
    const til::point cursorPos{ _activeBuffer().GetCursor().GetPosition() };

    if ((_currentPromptState == PromptState::Command) &&
        (_activeBuffer().GetMarks().size() > 0))
    {
        // We were in the right state, and there's a previous mark to work
        // with.
        //
        //We can just do the work below safely.
    }
    else
    {
        // If there was no last mark, or we're in a weird state,
        // then abandon the current state, and just insert a new Prompt mark that
        // start's & ends here, and the command ends here, and go to State::Output.

        ScrollMark mark;
        mark.category = MarkCategory::Prompt;
        AddMark(mark, cursorPos, cursorPos, false);
    }
    _activeBuffer().SetCurrentCommandEnd(cursorPos);
    _currentPromptState = PromptState::Output;
}

void Terminal::MarkCommandFinish(std::optional<unsigned int> error)
{
    const til::point cursorPos{ _activeBuffer().GetCursor().GetPosition() };
    auto category = MarkCategory::Prompt;
    if (error.has_value())
    {
        category = *error == 0u ?
                       MarkCategory::Success :
                       MarkCategory::Error;
    }

    if ((_currentPromptState == PromptState::Output) &&
        (_activeBuffer().GetMarks().size() > 0))
    {
        // We were in the right state, and there's a previous mark to work
        // with.
        //
        //We can just do the work below safely.
    }
    else
    {
        // If there was no last mark, or we're in a weird state, then abandon
        // the current state, and just insert a new Prompt mark that start's &
        // ends here, and the command ends here, AND the output ends here. and
        // go to State::Output.

        ScrollMark mark;
        mark.category = MarkCategory::Prompt;
        AddMark(mark, cursorPos, cursorPos, false);
        _activeBuffer().SetCurrentCommandEnd(cursorPos);
    }
    _activeBuffer().SetCurrentOutputEnd(cursorPos, category);
    _currentPromptState = PromptState::None;
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

bool Terminal::IsConsolePty() const noexcept
{
    return false;
}

bool Terminal::IsVtInputEnabled() const noexcept
{
    return false;
}

void Terminal::NotifyAccessibilityChange(const til::rect& /*changedRect*/) noexcept
{
    // This is only needed in conhost. Terminal handles accessibility in another way.
}

void Terminal::InvokeCompletions(std::wstring_view menuJson, unsigned int replaceLength)
{
    if (_pfnCompletionsChanged)
    {
        _pfnCompletionsChanged(menuJson, replaceLength);
    }
}

void Terminal::NotifyBufferRotation(const int delta)
{
    // Update our selection, so it doesn't move as the buffer is cycled
    if (_selection)
    {
        // If the end of the selection will be out of range after the move, we just
        // clear the selection. Otherwise we move both the start and end points up
        // by the given delta and clamp to the first row.
        if (_selection->end.y < delta)
        {
            _selection.reset();
        }
        else
        {
            // Stash this, so we can make sure to update the pivot to match later.
            const auto pivotWasStart = _selection->start == _selection->pivot;
            _selection->start.y = std::max(_selection->start.y - delta, 0);
            _selection->end.y = std::max(_selection->end.y - delta, 0);
            // Make sure to sync the pivot with whichever value is the right one.
            _selection->pivot = pivotWasStart ? _selection->start : _selection->end;
        }
    }

    // manually erase our pattern intervals since the locations have changed now
    _patternIntervalTree = {};

    auto& marks{ _activeBuffer().GetMarks() };
    const auto hasScrollMarks = marks.size() > 0;
    if (hasScrollMarks)
    {
        _activeBuffer().ScrollMarks(-delta);
    }

    const auto oldScrollOffset = _scrollOffset;
    _PreserveUserScrollOffset(delta);
    if (_scrollOffset != oldScrollOffset || hasScrollMarks)
    {
        _NotifyScrollEvent();
    }
}
