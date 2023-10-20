// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TSFInputControl.h"
#include "TSFInputControl.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Text::Core;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    TSFInputControl::TSFInputControl()
    {
        InitializeComponent();

        // Create a CoreTextEditingContext for since we are acting like a custom edit control
        auto manager = CoreTextServicesManager::GetForCurrentView();
        _editContext = manager.CreateEditContext();

        // InputPane is manually shown inside of TermControl.
        _editContext.InputPaneDisplayPolicy(CoreTextInputPaneDisplayPolicy::Manual);

        // Set the input scope to AlphanumericHalfWidth in order to facilitate those CJK input methods to open in English mode by default.
        // AlphanumericHalfWidth scope doesn't prevent input method from switching to composition mode, it accepts any character too.
        // Besides, Text scope turns on typing intelligence, but that doesn't work in this project.
        _editContext.InputScope(CoreTextInputScope::AlphanumericHalfWidth);

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
        // Also disconnect compositionCompleted and textUpdating explicitly. It seems to occasionally cause problems if
        // a composition is active during application teardown.
        _layoutRequestedRevoker.revoke();
        _compositionCompletedRevoker.revoke();
        _textUpdatingRevoker.revoke();
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
        _editContext.NotifyFocusEnter();
        _focused = true;
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
        _editContext.NotifyFocusLeave();
        _focused = false;
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
            _inputBuffer.clear();
            _selection = {};
            _activeTextStart = 0;
            _editContext.NotifyTextChanged({ 0, INT32_MAX }, 0, _selection);
            TextBlock().Text({});
        }
    }

    // Method Description:
    // - Redraw the canvas if certain dimensions have changed since the last
    //   redraw. This includes the Terminal cursor position, the Canvas width, and the TextBlock height.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TSFInputControl::TryRedrawCanvas()
    try
    {
        if (!_focused || !Canvas())
        {
            return;
        }

        // Get the cursor position in text buffer position
        auto cursorArgs = winrt::make_self<CursorPositionEventArgs>();
        _CurrentCursorPositionHandlers(*this, *cursorArgs);
        const til::point cursorPos{ til::math::flooring, cursorArgs->CurrentPosition() };

        const auto actualCanvasWidth{ Canvas().ActualWidth() };
        const auto actualTextBlockHeight{ TextBlock().ActualHeight() };
        const auto actualWindowBounds{ CoreWindow::GetForCurrentThread().Bounds() };

        if (_currentTerminalCursorPos == cursorPos &&
            _currentCanvasWidth == actualCanvasWidth &&
            _currentTextBlockHeight == actualTextBlockHeight &&
            _currentWindowBounds == actualWindowBounds)
        {
            return;
        }

        _currentTerminalCursorPos = cursorPos;
        _currentCanvasWidth = actualCanvasWidth;
        _currentTextBlockHeight = actualTextBlockHeight;
        _currentWindowBounds = actualWindowBounds;

        _RedrawCanvas();
    }
    CATCH_LOG()

    // Method Description:
    // - Redraw the Canvas and update the current Text Bounds and Control Bounds for
    //   the CoreTextEditContext.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TSFInputControl::_RedrawCanvas()
    {
        // Get Font Info as we use this is the pixel size for characters in the display
        auto fontArgs = winrt::make_self<FontInfoEventArgs>();
        _CurrentFontInfoHandlers(*this, *fontArgs);

        const til::size fontSize{ til::math::flooring, fontArgs->FontSize() };

        // Convert text buffer cursor position to client coordinate position
        // within the window. This point is in _pixels_
        const auto clientCursorPos{ _currentTerminalCursorPos * fontSize };

        // Get scale factor for view
        const auto scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();

        const til::point clientCursorInDips{ til::math::flooring, clientCursorPos.x / scaleFactor, clientCursorPos.y / scaleFactor };

        // Position our TextBlock at the cursor position
        Canvas().SetLeft(TextBlock(), clientCursorInDips.x);
        Canvas().SetTop(TextBlock(), clientCursorInDips.y);

        // calculate FontSize in pixels from Points
        const double fontSizePx = (fontSize.height * 72) / USER_DEFAULT_SCREEN_DPI;
        const auto unscaledFontSizePx = fontSizePx / scaleFactor;

        // Make sure to unscale the font size to correct for DPI! XAML needs
        // things in DIPs, and the fontSize is in pixels.
        TextBlock().FontSize(unscaledFontSizePx);
        TextBlock().FontFamily(Media::FontFamily(fontArgs->FontFace()));
        TextBlock().FontWeight(fontArgs->FontWeight());

        // TextBlock's actual dimensions right after initialization is 0w x 0h. So,
        // if an IME is displayed before TextBlock has text (like showing the emoji picker
        // using Win+.), it'll be placed higher than intended.
        TextBlock().MinWidth(unscaledFontSizePx);
        TextBlock().MinHeight(unscaledFontSizePx);
        _currentTextBlockHeight = std::max(unscaledFontSizePx, _currentTextBlockHeight);

        const auto widthToTerminalEnd = _currentCanvasWidth - clientCursorInDips.x;
        // Make sure that we're setting the MaxWidth to a positive number - a
        // negative number here will crash us in mysterious ways with a useless
        // stack trace
        const auto newMaxWidth = std::max<double>(0.0, widthToTerminalEnd);
        TextBlock().MaxWidth(newMaxWidth);

        // Get window in screen coordinates, this is the entire window including
        // tabs. THIS IS IN DIPs
        const til::point windowOrigin{ til::math::flooring, _currentWindowBounds.X, _currentWindowBounds.Y };

        // Get the offset (margin + tabs, etc..) of the control within the window
        const til::point controlOrigin{ til::math::flooring,
                                        this->TransformToVisual(nullptr).TransformPoint(Point(0, 0)) };

        // The controlAbsoluteOrigin is the origin of the control relative to
        // the origin of the displays. THIS IS IN DIPs
        const auto controlAbsoluteOrigin{ windowOrigin + controlOrigin };

        // Convert the control origin to pixels
        const auto scaledFrameOrigin = til::point{ til::math::flooring, controlAbsoluteOrigin.x * scaleFactor, controlAbsoluteOrigin.y * scaleFactor };

        // Get the location of the cursor in the display, in pixels.
        auto screenCursorPos{ scaledFrameOrigin + clientCursorPos };

        // GH #5007 - make sure to account for wrapping the IME composition at
        // the right side of the viewport.
        const auto textBlockHeight = ::base::ClampMul(_currentTextBlockHeight, scaleFactor);

        // Get the bounds of the composition text, in pixels.
        const til::rect textBounds{ til::point{ screenCursorPos.x, screenCursorPos.y },
                                    til::size{ 0, textBlockHeight } };

        _currentTextBounds = textBounds.to_winrt_rect();

        _currentControlBounds = Rect(static_cast<float>(screenCursorPos.x),
                                     static_cast<float>(screenCursorPos.y),
                                     0.0f,
                                     static_cast<float>(fontSize.height));
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
    void TSFInputControl::_layoutRequestedHandler(CoreTextEditContext sender, const CoreTextLayoutRequestedEventArgs& args)
    {
        auto request = args.Request();

        TryRedrawCanvas();

        // Set the text block bounds
        request.LayoutBounds().TextBounds(_currentTextBounds);

        // Set the control bounds of the whole control
        request.LayoutBounds().ControlBounds(_currentControlBounds);
    }

    // Method Description:
    // - Handler for CompositionStarted event by CoreEditContext responsible
    //   for making internal TSFInputControl controls visible.
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextCompositionStartedEventArgs. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_compositionStartedHandler(CoreTextEditContext sender, const CoreTextCompositionStartedEventArgs& /*args*/)
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
    void TSFInputControl::_compositionCompletedHandler(CoreTextEditContext sender, const CoreTextCompositionCompletedEventArgs& /*args*/)
    {
        _inComposition = false;
        _SendAndClearText();
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
    void TSFInputControl::_focusRemovedHandler(CoreTextEditContext sender, const winrt::Windows::Foundation::IInspectable& /*object*/)
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
    void TSFInputControl::_textRequestedHandler(CoreTextEditContext sender, const CoreTextTextRequestedEventArgs& args)
    {
        try
        {
            const auto range = args.Request().Range();
            const auto text = _inputBuffer.substr(
                range.StartCaretPosition,
                range.EndCaretPosition - range.StartCaretPosition);
            args.Request().Text(text);
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
    void TSFInputControl::_selectionRequestedHandler(CoreTextEditContext sender, const CoreTextSelectionRequestedEventArgs& args)
    {
        args.Request().Selection(_selection);
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
    void TSFInputControl::_selectionUpdatingHandler(CoreTextEditContext sender, const CoreTextSelectionUpdatingEventArgs& args)
    {
        _selection = args.Selection();
        args.Result(CoreTextSelectionUpdatingResult::Succeeded);
    }

    // Method Description:
    // - Handler for TextUpdating event by CoreEditContext responsible
    //   for handling text updates.
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextTextUpdatingEventArgs contains new text to update buffer with.
    // Return Value:
    // - <none>
    void TSFInputControl::_textUpdatingHandler(CoreTextEditContext sender, const CoreTextTextUpdatingEventArgs& args)
    {
        try
        {
            const auto incomingText = args.Text();
            const auto range = args.Range();

            _inputBuffer = _inputBuffer.replace(
                range.StartCaretPosition,
                range.EndCaretPosition - range.StartCaretPosition,
                incomingText);
            _selection = args.NewSelection();
            // GH#5054: Pressing backspace might move the caret before the _activeTextStart.
            _activeTextStart = std::min(_activeTextStart, _inputBuffer.size());

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

    void TSFInputControl::_SendAndClearText()
    {
        const auto text = _inputBuffer.substr(_activeTextStart);
        if (text.empty())
        {
            return;
        }

        _CompositionCompletedHandlers(text);

        _activeTextStart = _inputBuffer.size();

        TextBlock().Text({});

        // After we reset the TextBlock to empty string, we want to make sure
        // ActualHeight reflects the respective height. It seems that ActualHeight
        // isn't updated until there's new text in the TextBlock, so the next time a user
        // invokes "Win+." for the emoji picker IME, it would end up
        // using the pre-reset TextBlock().ActualHeight().
        TextBlock().UpdateLayout();

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
    void TSFInputControl::_formatUpdatingHandler(CoreTextEditContext sender, const CoreTextFormatUpdatingEventArgs& /*args*/)
    {
    }
}
