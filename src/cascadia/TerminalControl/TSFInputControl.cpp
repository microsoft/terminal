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
        _editContext{ nullptr }
    {
        _Create();
    }

    // Method Description:
    // - Creates XAML controls for displaying user input and hooks up CoreTextEditContext handlers
    //   for handling text input from the Text Services Framework.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TSFInputControl::_Create()
    {
        // TextBlock for user input form TSF
        _textBlock = Controls::TextBlock();
        _textBlock.Visibility(Visibility::Collapsed);
        _textBlock.IsTextSelectionEnabled(false);
        _textBlock.TextDecorations(TextDecorations::Underline);

        // Canvas for controlling exact position of the TextBlock
        _canvas = Windows::UI::Xaml::Controls::Canvas();
        _canvas.Visibility(Visibility::Collapsed);

        // add the Textblock to the Canvas
        _canvas.Children().Append(_textBlock);

        // set the content of this control to be the Canvas
        this->Content(_canvas);

        // Create a CoreTextEditingContext for since we are acting like a custom edit control
        auto manager = Core::CoreTextServicesManager::GetForCurrentView();
        _editContext = manager.CreateEditContext();

        // sets the Input Pane display policy to Manual for now so that it can manually show the
        // software keyboard when the control gains focus and dismiss it when the control loses focus.
        // TODO GitHub #3639: Should Input Pane display policy be Automatic
        _editContext.InputPaneDisplayPolicy(Core::CoreTextInputPaneDisplayPolicy::Manual);

        // set the input scope to Text because this control is for any text.
        _editContext.InputScope(Core::CoreTextInputScope::Text);

        _editContext.TextRequested({ this, &TSFInputControl::_textRequestedHandler });

        _editContext.SelectionRequested({ this, &TSFInputControl::_selectionRequestedHandler });

        _editContext.FocusRemoved({ this, &TSFInputControl::_focusRemovedHandler });

        _editContext.TextUpdating({ this, &TSFInputControl::_textUpdatingHandler });

        _editContext.SelectionUpdating({ this, &TSFInputControl::_selectionUpdatingHandler });

        _editContext.FormatUpdating({ this, &TSFInputControl::_formatUpdatingHandler });

        _editContext.LayoutRequested({ this, &TSFInputControl::_layoutRequestedHandler });

        _editContext.CompositionStarted({ this, &TSFInputControl::_compositionStartedHandler });

        _editContext.CompositionCompleted({ this, &TSFInputControl::_compositionCompletedHandler });
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
            // _editContext.NotifyFocusLeave(); TODO GitHub #3645: Enabling causes IME to no longer show up, need to determine if required
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
        const COORD cursorPos = { gsl::narrow_cast<SHORT>(cursorArgs->CurrentPosition().X), gsl::narrow_cast<SHORT>(cursorArgs->CurrentPosition().Y) };

        // Get Font Info as we use this is the pixel size for characters in the display
        auto fontArgs = winrt::make_self<FontInfoEventArgs>();
        _CurrentFontInfoHandlers(*this, *fontArgs);

        const float fontWidth = fontArgs->FontSize().Width;
        const float fontHeight = fontArgs->FontSize().Height;

        // Convert text buffer cursor position to client coordinate position within the window
        COORD clientCursorPos;
        COORD screenCursorPos;
        THROW_IF_FAILED(ShortMult(cursorPos.X, gsl::narrow<SHORT>(fontWidth), &clientCursorPos.X));
        THROW_IF_FAILED(ShortMult(cursorPos.Y, gsl::narrow<SHORT>(fontHeight), &clientCursorPos.Y));

        // Convert from client coordinate to screen coordinate by adding window position
        THROW_IF_FAILED(ShortAdd(clientCursorPos.X, gsl::narrow_cast<SHORT>(windowBounds.X), &screenCursorPos.X));
        THROW_IF_FAILED(ShortAdd(clientCursorPos.Y, gsl::narrow_cast<SHORT>(windowBounds.Y), &screenCursorPos.Y));

        // get any offset (margin + tabs, etc..) of the control within the window
        const auto offsetPoint = this->TransformToVisual(nullptr).TransformPoint(winrt::Windows::Foundation::Point(0, 0));

        // add the margin offsets if any
        const auto currentMargin = this->Margin();
        THROW_IF_FAILED(ShortAdd(screenCursorPos.X, gsl::narrow_cast<SHORT>(offsetPoint.X), &screenCursorPos.X));
        THROW_IF_FAILED(ShortAdd(screenCursorPos.Y, gsl::narrow_cast<SHORT>(offsetPoint.Y), &screenCursorPos.Y));

        // Get scale factor for view
        const double scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();

        // Set the selection layout bounds
        Rect selectionRect = Rect(screenCursorPos.X, screenCursorPos.Y, 0, fontHeight);
        request.LayoutBounds().TextBounds(ScaleRect(selectionRect, scaleFactor));

        // Set the control bounds of the whole control
        Rect controlRect = Rect(screenCursorPos.X, screenCursorPos.Y, 0, fontHeight);
        request.LayoutBounds().ControlBounds(ScaleRect(controlRect, scaleFactor));

        // position textblock to cursor position
        _canvas.SetLeft(_textBlock, clientCursorPos.X);
        _canvas.SetTop(_textBlock, static_cast<double>(clientCursorPos.Y));

        // width is cursor to end of canvas
        _textBlock.Width(200); // TODO GitHub #3640: Determine proper Width
        _textBlock.Height(fontHeight);

        // calculate FontSize in pixels from DIPs
        const double fontSizePx = (fontHeight * 72) / USER_DEFAULT_SCREEN_DPI;
        _textBlock.FontSize(fontSizePx);

        _textBlock.FontFamily(Media::FontFamily(fontArgs->FontFace()));
    }

    // Method Description:
    // - Handler for CompositionStarted event by CoreEditContext responsible
    //   for making internal TSFInputControl controls visisble.
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextCompositionStartedEventArgs. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_compositionStartedHandler(CoreTextEditContext sender, CoreTextCompositionStartedEventArgs const& /*args*/)
    {
        _canvas.Visibility(Visibility::Visible);
        _textBlock.Visibility(Visibility::Visible);
    }

    // Method Description:
    // - Handler for CompositionCompleted event by CoreEditContext responsible
    //   for making internal TSFInputControl controls visisble.
    // Arguments:
    // - sender: CoreTextEditContext sending the request. Not used in method.
    // - args: CoreTextCompositionCompletedEventArgs. Not used in method.
    // Return Value:
    // - <none>
    void TSFInputControl::_compositionCompletedHandler(CoreTextEditContext sender, CoreTextCompositionCompletedEventArgs const& /*args*/)
    {
        // only need to do work if the current buffer has text
        if (!_inputBuffer.empty())
        {
            const auto hstr = to_hstring(_inputBuffer.c_str());

            // call event handler with data handled by parent
            _compositionCompletedHandlers(hstr);

            // clear the buffer for next round
            _inputBuffer.clear();
            _textBlock.Text(L"");

            // tell the input server that we've cleared the buffer
            CoreTextRange emptyTextRange;
            emptyTextRange.StartCaretPosition = 0;
            emptyTextRange.EndCaretPosition = 0;

            // indicate text is now 0
            _editContext.NotifyTextChanged(emptyTextRange, 0, emptyTextRange);
            _editContext.NotifySelectionChanged(emptyTextRange);

            // hide the controls until composition starts again
            _canvas.Visibility(Visibility::Collapsed);
            _textBlock.Visibility(Visibility::Collapsed);
        }
    }

    // Method Description:
    // - Handler for FocusRemoved event by CoreEditContext responsible
    //   for removing focus for the TSFInputControl control accordingly
    //   when focus was forecibly removed from text input control. (TODO GitHub #3644)
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
            const auto textRequested = _inputBuffer.substr(range.StartCaretPosition, static_cast<size_t>(range.EndCaretPosition) - static_cast<size_t>(range.StartCaretPosition));

            args.Request().Text(winrt::to_hstring(textRequested.c_str()));
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
        const auto text = args.Text();
        const auto range = args.Range();

        try
        {
            _inputBuffer = _inputBuffer.replace(
                range.StartCaretPosition,
                static_cast<size_t>(range.EndCaretPosition) - static_cast<size_t>(range.StartCaretPosition),
                text.c_str());

            _textBlock.Text(_inputBuffer);

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
