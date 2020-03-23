// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TSFInputControl.h"
#include "TSFInputControl.g.cpp"

#include <Utils.h>

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Text::Core;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    TSFInputControl::TSFInputControl() :
        _editContext{ nullptr },
        _inComposition{ false },
        _activeTextStart{ 0 }
    {
        InitializeComponent();

        // Create a CoreTextEditingContext for since we are acting like a custom edit control
        auto manager = Core::CoreTextServicesManager::GetForCurrentView();
        _editContext = manager.CreateEditContext();

        // InputPane is manually shown inside of TermControl.
        _editContext.InputPaneDisplayPolicy(Core::CoreTextInputPaneDisplayPolicy::Manual);

        // set the input scope to Text because this control is for any text.
        _editContext.InputScope(Core::CoreTextInputScope::Text);

        _textRequestedRevoker = _editContext.TextRequested(winrt::auto_revoke, { this, &TSFInputControl::_textRequestedHandler });

        _selectionRequestedRevoker = _editContext.SelectionRequested(winrt::auto_revoke, { this, &TSFInputControl::_selectionRequestedHandler });

        _focusRemovedRevoker = _editContext.FocusRemoved(winrt::auto_revoke, { this, &TSFInputControl::_focusRemovedHandler });

        _textUpdatingRevoker = _editContext.TextUpdating(winrt::auto_revoke, { this, &TSFInputControl::_textUpdatingHandler });

        _selectionUpdatingRevoker = _editContext.SelectionUpdating(winrt::auto_revoke, { this, &TSFInputControl::_selectionUpdatingHandler });

        _formatUpdatingRevoker = _editContext.FormatUpdating(winrt::auto_revoke, { this, &TSFInputControl::_formatUpdatingHandler });

        _layoutRequestedRevoker = _editContext.LayoutRequested(winrt::auto_revoke, { this, &TSFInputControl::_layoutRequestedHandler });

        _compositionStartedRevoker = _editContext.CompositionStarted(winrt::auto_revoke, { this, &TSFInputControl::_compositionStartedHandler });

        _compositionCompletedRevoker = _editContext.CompositionCompleted(winrt::auto_revoke, { this, &TSFInputControl::_compositionCompletedHandler });
    }

    // Method Description:
    // - Prepares this TSFInputControl to be removed from the UI hierarchy.
    void TSFInputControl::Close()
    {
        // Explicitly disconnect the LayoutRequested handler -- it can cause problems during application teardown.
        // See GH#4159 for more info.
        _layoutRequestedRevoker.revoke();
    }

    // Method Description:
    // - NotifyFocusEnter handler for notifying CoreEditTextContext of focus enter
    //   when TerminalControl receives focus.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TSFInputControl::NotifyFocusEnter()
    {
        if (_editContext != nullptr)
        {
            _editContext.NotifyFocusEnter();
        }
    }

    // Method Description:
    // - NotifyFocusEnter handler for notifying CoreEditTextContext of focus leaving.
    //   when TerminalControl no longer has focus.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TSFInputControl::NotifyFocusLeave()
    {
        if (_editContext != nullptr)
        {
            _editContext.NotifyFocusLeave();
        }
    }

    // Method Description:
    // - Clears the input buffer and tells the text server to clear their buffer as well.
    //   Also clears the TextBlock and sets the active text starting point to 0.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TSFInputControl::ClearBuffer()
    {
        if (!_inputBuffer.empty())
        {
            TextBlock().Text(L"");
            const auto bufLen = ::base::ClampedNumeric<int32_t>(_inputBuffer.length());
            _inputBuffer.clear();
            _editContext.NotifyFocusLeave();
            _editContext.NotifyTextChanged({ 0, bufLen }, 0, { 0, 0 });
            _editContext.NotifyFocusEnter();
            _activeTextStart = 0;
            _inComposition = false;
        }
    }

    // Method Description:
    // - Handler for LayoutRequested event by CoreEditContext responsible
    //   for returning the current position the IME should be placed
    //   in screen coordinates on the screen.  TSFInputControls internal
    //   XAML controls (TextBlock/Canvas) are also positioned and updated.
    //   NOTE: documentation says application should handle this event
    // Arguments:
    // - sender: CoreTextEditContext sending the request.
    // - args: CoreTextLayoutRequestedEventArgs to be updated with position information.
    // Return Value:
    // - <none>
    void TSFInputControl::_layoutRequestedHandler(CoreTextEditContext sender, CoreTextLayoutRequestedEventArgs const& args)
    {
        auto request = args.Request();

        // Get window in screen coordinates, this is the entire window including tabs
        const auto windowBounds = CoreWindow::GetForCurrentThread().Bounds();

        // Get the cursor position in text buffer position
        auto cursorArgs = winrt::make_self<CursorPositionEventArgs>();
        _CurrentCursorPositionHandlers(*this, *cursorArgs);
        const COORD cursorPos = { ::base::ClampedNumeric<short>(cursorArgs->CurrentPosition().X), ::base::ClampedNumeric<short>(cursorArgs->CurrentPosition().Y) };

        // Get Font Info as we use this is the pixel size for characters in the display
        auto fontArgs = winrt::make_self<FontInfoEventArgs>();
        _CurrentFontInfoHandlers(*this, *fontArgs);

        const auto fontWidth = fontArgs->FontSize().Width;
        const auto fontHeight = fontArgs->FontSize().Height;

        // Convert text buffer cursor position to client coordinate position within the window
        COORD clientCursorPos;
        clientCursorPos.X = ::base::ClampMul(cursorPos.X, ::base::ClampedNumeric<short>(fontWidth));
        clientCursorPos.Y = ::base::ClampMul(cursorPos.Y, ::base::ClampedNumeric<short>(fontHeight));

        // Convert from client coordinate to screen coordinate by adding window position
        COORD screenCursorPos;
        screenCursorPos.X = ::base::ClampAdd(clientCursorPos.X, ::base::ClampedNumeric<short>(windowBounds.X));
        screenCursorPos.Y = ::base::ClampAdd(clientCursorPos.Y, ::base::ClampedNumeric<short>(windowBounds.Y));

        // get any offset (margin + tabs, etc..) of the control within the window
        const auto offsetPoint = this->TransformToVisual(nullptr).TransformPoint(winrt::Windows::Foundation::Point(0, 0));

        // add the margin offsets if any
        screenCursorPos.X = ::base::ClampAdd(screenCursorPos.X, ::base::ClampedNumeric<short>(offsetPoint.X));
        screenCursorPos.Y = ::base::ClampAdd(screenCursorPos.Y, ::base::ClampedNumeric<short>(offsetPoint.Y));

        // Get scale factor for view
        const double scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();

        // position textblock to cursor position
        Canvas().SetLeft(TextBlock(), clientCursorPos.X);
        Canvas().SetTop(TextBlock(), ::base::ClampedNumeric<double>(clientCursorPos.Y));

        // calculate FontSize in pixels from DIPs
        const double fontSizePx = (fontHeight * 72) / USER_DEFAULT_SCREEN_DPI;
        TextBlock().FontSize(fontSizePx);
        TextBlock().FontFamily(Media::FontFamily(fontArgs->FontFace()));

        const auto canvasActualWidth = Canvas().ActualWidth();
        const auto widthToTerminalEnd = canvasActualWidth - ::base::ClampedNumeric<double>(clientCursorPos.X);
        // Make sure that we're setting the MaxWidth to a positive number - a
        // negative number here will crash us in mysterious ways with a useless
        // stack trace
        const auto newMaxWidth = std::max<double>(0.0, widthToTerminalEnd);
        TextBlock().MaxWidth(newMaxWidth);

        // Set the text block bounds
        const auto yOffset = ::base::ClampedNumeric<float>(TextBlock().ActualHeight()) - fontHeight;
        const auto textBottom = ::base::ClampedNumeric<float>(screenCursorPos.Y) + yOffset;
        Rect selectionRect = Rect(screenCursorPos.X, textBottom, 0, fontHeight);
        request.LayoutBounds().TextBounds(ScaleRect(selectionRect, scaleFactor));

        // Set the control bounds of the whole control
        Rect controlRect = Rect(screenCursorPos.X, screenCursorPos.Y, 0, fontHeight);
        request.LayoutBounds().ControlBounds(ScaleRect(controlRect, scaleFactor));
    }

    // Method Description:
    // - Handler for CompositionStarted event by CoreEditContext responsible
    //   for making internal TSFInputControl controls visible.
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextCompositionStartedEventArgs. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_compositionStartedHandler(CoreTextEditContext sender, CoreTextCompositionStartedEventArgs const& /*args*/)
    {
        _inComposition = true;
    }

    // Method Description:
    // - Handler for CompositionCompleted event by CoreEditContext responsible
    //   for making internal TSFInputControl controls visible.
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextCompositionCompletedEventArgs. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_compositionCompletedHandler(CoreTextEditContext sender, CoreTextCompositionCompletedEventArgs const& /*args*/)
    {
        _inComposition = false;

        // only need to do work if the current buffer has text
        if (!_inputBuffer.empty())
        {
            _SendAndClearText();
        }
    }

    // Method Description:
    // - Handler for FocusRemoved event by CoreEditContext responsible
    //   for removing focus for the TSFInputControl control accordingly
    //   when focus was forcibly removed from text input control.
    //   NOTE: Documentation says application should handle this event
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - object: CoreTextCompositionStartedEventArgs. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_focusRemovedHandler(CoreTextEditContext sender, winrt::Windows::Foundation::IInspectable const& /*object*/)
    {
    }

    // Method Description:
    // - Handler for TextRequested event by CoreEditContext responsible
    //   for returning the range of text requested.
    //   NOTE: Documentation says application should handle this event
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextTextRequestedEventArgs to be updated with requested range text.
    // Return Value:
    // - <none>
    void TSFInputControl::_textRequestedHandler(CoreTextEditContext sender, CoreTextTextRequestedEventArgs const& args)
    {
        // the range the TSF wants to know about
        const auto range = args.Request().Range();

        try
        {
            const auto textEnd = ::base::ClampMin<size_t>(range.EndCaretPosition, _inputBuffer.length());
            const auto length = ::base::ClampSub<size_t>(textEnd, range.StartCaretPosition);
            const auto textRequested = _inputBuffer.substr(range.StartCaretPosition, length);

            args.Request().Text(textRequested);
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Handler for SelectionRequested event by CoreEditContext responsible
    //   for returning the currently selected text.
    //   TSFInputControl currently doesn't allow selection, so nothing happens.
    //   NOTE: Documentation says application should handle this event
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextSelectionRequestedEventArgs for providing data for the SelectionRequested event. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_selectionRequestedHandler(CoreTextEditContext sender, CoreTextSelectionRequestedEventArgs const& /*args*/)
    {
    }

    // Method Description:
    // - Handler for SelectionUpdating event by CoreEditContext responsible
    //   for handling modifications to the range of text currently selected.
    //   TSFInputControl doesn't currently allow selection, so nothing happens.
    //   NOTE: Documentation says application should set its selection range accordingly
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextSelectionUpdatingEventArgs for providing data for the SelectionUpdating event. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_selectionUpdatingHandler(CoreTextEditContext sender, CoreTextSelectionUpdatingEventArgs const& /*args*/)
    {
    }

    // Method Description:
    // - Handler for TextUpdating event by CoreEditContext responsible
    //   for handling text updates.
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextTextUpdatingEventArgs contains new text to update buffer with.
    // Return Value:
    // - <none>
    void TSFInputControl::_textUpdatingHandler(CoreTextEditContext sender, CoreTextTextUpdatingEventArgs const& args)
    {
        const auto incomingText = args.Text();
        const auto range = args.Range();

        try
        {
            _inputBuffer = _inputBuffer.replace(
                range.StartCaretPosition,
                ::base::ClampSub<size_t>(range.EndCaretPosition, range.StartCaretPosition),
                incomingText);

            // Emojis/Kaomojis/Symbols chosen through the IME without starting composition
            // will be sent straight through to the terminal.
            if (!_inComposition)
            {
                _SendAndClearText();
            }
            else
            {
                Canvas().Visibility(Visibility::Visible);
                const auto text = _inputBuffer.substr(_activeTextStart);
                TextBlock().Text(text);
            }

            // Notify the TSF that the update succeeded
            args.Result(CoreTextTextUpdatingResult::Succeeded);
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();

            // indicate updating failed.
            args.Result(CoreTextTextUpdatingResult::Failed);
        }
    }

    // Method Description:
    // - Send the portion of the textBuffer starting at _activeTextStart to the end of the buffer.
    //   Then clear the TextBlock and hide it until the next time text is received.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TSFInputControl::_SendAndClearText()
    {
        const auto text = _inputBuffer.substr(_activeTextStart);

        _compositionCompletedHandlers(text);

        _activeTextStart = _inputBuffer.length();

        TextBlock().Text(L"");

        // hide the controls until text input starts again
        Canvas().Visibility(Visibility::Collapsed);
    }

    // Method Description:
    // - Handler for FormatUpdating event by CoreEditContext responsible
    //   for handling different format updates for a particular range of text.
    //   TSFInputControl doesn't do anything with this event.
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextFormatUpdatingEventArgs Provides data for the FormatUpdating event. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_formatUpdatingHandler(CoreTextEditContext sender, CoreTextFormatUpdatingEventArgs const& /*args*/)
    {
    }

    DEFINE_EVENT(TSFInputControl, CompositionCompleted, _compositionCompletedHandlers, TerminalControl::CompositionCompletedEventArgs);
}
