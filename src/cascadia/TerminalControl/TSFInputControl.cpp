#include "pch.h"
#include "TSFInputControl.h"
#include "TSFInputControl.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Text::Core;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    TSFInputControl::TSFInputControl() :
        _editContext { nullptr }
    {
        _Create();
    }

    void TSFInputControl::_Create()
    {
        // TextBlock for user input form TSF
        _textBlock = Controls::TextBlock();
        _textBlock.Visibility(Visibility::Collapsed);
        _textBlock.IsTextSelectionEnabled(false);
        _textBlock.TextDecorations(TextDecorations::Underline);

        // canvas for controlling exact position of the TextBlock
        _canvas = Windows::UI::Xaml::Controls::Canvas();
        _canvas.Visibility(Visibility::Collapsed);

        // add the textblock to the canvas
        _canvas.Children().Append(_textBlock);

        // set the content of this control to be the canvas
        this->Content(_canvas);

        // Create a CoreTextEditingContext for since we are like a custom edit control
        auto manager = Core::CoreTextServicesManager::GetForCurrentView();
        _editContext = manager.CreateEditContext();

        // sets the Input Pane display policy to Manual for now so that it can manually show the 
        // software keyboard when the control gains focus and dismiss it when the control loses focus.
        // Should look at Automatic int he Future Add WI TODO 
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

    Windows::UI::Xaml::DependencyProperty TSFInputControl::_fontHeightProperty =
        Windows::UI::Xaml::DependencyProperty::Register(
            L"FontHeight",
            winrt::xaml_typename<double>(),
            winrt::xaml_typename<TerminalControl::TSFInputControl>(),
            nullptr
    );

    Windows::UI::Xaml::DependencyProperty TSFInputControl::_fontWidthProperty =
        Windows::UI::Xaml::DependencyProperty::Register(
            L"FontWidth",
            winrt::xaml_typename<double>(),
            winrt::xaml_typename<TerminalControl::TSFInputControl>(),
            nullptr
    );

    void TSFInputControl::NotifyFocusEnter()
    {
        if (_editContext != nullptr)
        {
            OutputDebugString(L"_NotifyFocusEnter\n");
            _editContext.NotifyFocusEnter();
        }
    }

    void TSFInputControl::NotifyFocusLeave()
    {
        if (_editContext != nullptr)
        {
            OutputDebugString(L"_NotifyFocusLeave\n");
            //_editContext.NotifyFocusLeave();
        }
    }

    static Rect ScaleRect(Rect rect, double scale)
    {
        const float scaleLocal = gsl::narrow<float>(scale);
        rect.X *= scaleLocal;
        rect.Y *= scaleLocal;
        rect.Width *= scaleLocal;
        rect.Height *= scaleLocal;
        return rect;
    }

    inline winrt::Windows::UI::Color ColorRefToColor(const COLORREF& colorref)
    {
        winrt::Windows::UI::Color color;
        //color.A = static_cast<BYTE>(colorref >> 24);
        color.R = GetRValue(colorref);
        color.G = GetGValue(colorref);
        color.B = GetBValue(colorref);
        return color;
    }

    // documentation says application should handle this event
    void TSFInputControl::_layoutRequestedHandler(CoreTextEditContext sender, CoreTextLayoutRequestedEventArgs const& args)
    {
        OutputDebugString(L"_editContextlayoutRequested\n");
        auto request = args.Request();

        // Get window in screen coordinates, this is the entire window including tabs
        auto windowBounds = Window::Current().CoreWindow().Bounds();

        // Get the cursor position in text buffer position
        auto cursorArgs = winrt::make_self<CursorPositionEventArgs>();
        _currentCursorPositionHandlers(*this, *cursorArgs);
        COORD cursorPos = { gsl::narrow_cast<SHORT>(cursorArgs->CurrentPosition().X), gsl::narrow_cast<SHORT>(cursorArgs->CurrentPosition().Y) }; //_terminal->GetCursorPosition();

        WCHAR buff[100];
        StringCchPrintfW(buff, sizeof(buff), L"Cursor x:%d, y:%d\n", cursorPos.X, cursorPos.Y);
        OutputDebugString(buff);

        // Get Font Info as we use this is the pixel size for characters in the display
        auto fontArgs = winrt::make_self<FontInfoEventArgs>();
        _currentFontInfoHandlers(*this, *fontArgs);

        const float fontWidth = fontArgs->FontSize().X;
        const float fontHeight = fontArgs->FontSize().Y;

        StringCchPrintfW(buff, sizeof(buff), L"Window x:%f,y:%f\n", windowBounds.X, windowBounds.Y);

        OutputDebugString(buff);

        // Convert text buffer cursor position to client coordinate position within the window
        COORD clientCursorPos;
        COORD screenCursorPos;
        THROW_IF_FAILED(ShortMult(cursorPos.X, gsl::narrow<SHORT>(fontWidth), &clientCursorPos.X));
        THROW_IF_FAILED(ShortMult(cursorPos.Y, gsl::narrow<SHORT>(fontHeight), &clientCursorPos.Y));

        // Convert from client coordinate to screen coordinate by adding window position
        THROW_IF_FAILED(ShortAdd(clientCursorPos.X, gsl::narrow_cast<SHORT>(windowBounds.X), &screenCursorPos.X));
        THROW_IF_FAILED(ShortAdd(clientCursorPos.Y, gsl::narrow_cast<SHORT>(windowBounds.Y), &screenCursorPos.Y));

        // TODO: add tabs offset, currently a hack, since we can't determine the actual screen position of the control
        THROW_IF_FAILED(ShortAdd(screenCursorPos.Y, 34, &screenCursorPos.Y));

        // Get scale factor for view
        double scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();

        // TODO set real layout bounds
        Rect selectionRect = Rect(screenCursorPos.X, screenCursorPos.Y, 0, fontHeight);
        request.LayoutBounds().TextBounds(ScaleRect(selectionRect, scaleFactor));

        //This is the bounds of the whole control
        Rect controlRect = Rect(screenCursorPos.X, screenCursorPos.Y, 0, fontHeight);
        request.LayoutBounds().ControlBounds(ScaleRect(controlRect, scaleFactor));

        StringCchPrintfW(buff, sizeof(buff), L"clientCursorPos - x:%d,y:%d\n", clientCursorPos.X, clientCursorPos.Y);

        OutputDebugString(buff);

        // position textblock to cursor position
        _canvas.SetLeft(_textBlock, clientCursorPos.X);
        _canvas.SetTop(_textBlock, clientCursorPos.Y + 2); // TODO figure out how to align

        // width is cursor to end of canvas 
        _textBlock.Width(200); // TODO figure out proper width
        _textBlock.Height(fontHeight);

        //SHORT foo = _actualFont.GetUnscaledSize().Y;
        // TODO: font claims to be 12, but on screen 14 looks correct
        _textBlock.FontSize(14);

        _textBlock.FontFamily(Media::FontFamily(fontArgs->FontFace()));
    }

    void TSFInputControl::_compositionStartedHandler(CoreTextEditContext sender, CoreTextCompositionStartedEventArgs const& args)
    {
        OutputDebugString(L"CompositionStarted\n");
        _canvas.Visibility(Visibility::Visible);
        _textBlock.Visibility(Visibility::Visible);
    }

    void TSFInputControl::_compositionCompletedHandler(CoreTextEditContext sender, CoreTextCompositionCompletedEventArgs const& args)
    {
        if (/* !args.IsCanceled() TODO? only && */ !_inputBuffer.empty())
        {
            WCHAR buff[255];

            swprintf_s(buff, ARRAYSIZE(buff), L"Completed Text: %s\n", _inputBuffer.c_str());

            OutputDebugString(buff);

            auto hstr = to_hstring(_inputBuffer.c_str());

            // call event handler with data handled by parent
            _compositionCompletedHandlers(hstr);

            // tell the server that we've cleared the buffer
            CoreTextRange newTextRange;
            newTextRange.StartCaretPosition = 0;
            newTextRange.EndCaretPosition = 0;
            _editContext.NotifySelectionChanged(newTextRange);
        }

        // clear the buffer for next round
        _inputBuffer.clear();
        _textBlock.Text(L"");
        _canvas.Visibility(Visibility::Collapsed);
        _textBlock.Visibility(Visibility::Collapsed);
        OutputDebugString(L"CompositionCompleted\n");
    }

    // documentation says application should handle this event
    void TSFInputControl::_focusRemovedHandler(CoreTextEditContext sender, winrt::Windows::Foundation::IInspectable const& object)
    {
        OutputDebugString(L"FocusRemoved\n");
    }

    void TSFInputControl::_textRequestedHandler(CoreTextEditContext sender, CoreTextTextRequestedEventArgs const& args)
    {
        OutputDebugString(L"_editContextTextRequested\n");
    }

    void TSFInputControl::_selectionRequestedHandler(CoreTextEditContext sender, CoreTextSelectionRequestedEventArgs const& args)
    {
        OutputDebugString(L"_editContextSelectionRequested\n");
    }

    void TSFInputControl::_selectionUpdatingHandler(CoreTextEditContext sender, CoreTextSelectionUpdatingEventArgs const& args)
    {
        OutputDebugString(L"_editContextSelectionUpdating\n");
    }

    void TSFInputControl::_textUpdatingHandler(CoreTextEditContext sender, CoreTextTextUpdatingEventArgs const& args)
    {
        OutputDebugString(L"_editContextTextUpdating\n");
        auto text = args.Text();
        auto range = args.Range();

        _inputBuffer = _inputBuffer.replace(
            range.StartCaretPosition,
            range.EndCaretPosition - range.StartCaretPosition,
            text.c_str()
        );

        WCHAR buff[255];
        swprintf_s(buff, ARRAYSIZE(buff), L"Text: %s\n", text.c_str());
        OutputDebugString(buff);

        _textBlock.Text(_inputBuffer);
    }

    void TSFInputControl::_formatUpdatingHandler(CoreTextEditContext sender, CoreTextFormatUpdatingEventArgs const& args)
    {
        OutputDebugString(L"_editContextFormatUpdating\n");
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TSFInputControl, CurrentCursorPosition, _currentCursorPositionHandlers, TerminalControl::TSFInputControl, TerminalControl::CursorPositionEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TSFInputControl, CurrentFontInfo, _currentFontInfoHandlers, TerminalControl::TSFInputControl, TerminalControl::FontInfoEventArgs);
    DEFINE_EVENT(TSFInputControl, CompositionCompleted, _compositionCompletedHandlers, TerminalControl::CompositionCompletedEventArgs);
}
