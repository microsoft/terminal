#pragma once
#include "TSFInputControl.g.h"
#include "CursorPositionEventArgs.g.h"
#include "FontInfoEventArgs.g.h"
#include "cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct CursorPositionEventArgs :
        public CursorPositionEventArgsT<CursorPositionEventArgs>
    {
    public:
        CursorPositionEventArgs() {};

        Windows::Foundation::Point CurrentPosition()
        {
            return _curPos;
        }

        void CurrentPosition(Windows::Foundation::Point newPosition)
        {
            _curPos = newPosition;
        }

    private:
        Windows::Foundation::Point _curPos;
    };

    struct FontInfoEventArgs :
        public FontInfoEventArgsT<FontInfoEventArgs>
    {
    public:
        FontInfoEventArgs() {};

        Windows::Foundation::Point FontSize()
        {
            return _curSize;
        }

        void FontSize(Windows::Foundation::Point newSize)
        {
            _curSize = newSize;
        }

        winrt::hstring FontFace()
        {
            return _fontFace;
        }

        void FontFace(winrt::hstring newFontFace)
        {
            _fontFace = newFontFace;
        }

    private:
        winrt::hstring _fontFace;
        Windows::Foundation::Point _curSize;
    };

    struct TSFInputControl : TSFInputControlT<TSFInputControl>
    {
    public:
        TSFInputControl();

        double FontWidth()
        {
            return winrt::unbox_value<double>(GetValue(_fontWidthProperty));
        }

        void FontWidth(double const& value)
        {
            SetValue(_fontWidthProperty, winrt::box_value(value));
        }

        double FontHeight()
        {
            return winrt::unbox_value<double>(GetValue(_fontHeightProperty));
        }

        void FontHeight(double const& value)
        {
            SetValue(_fontHeightProperty, winrt::box_value(value));
        }

        void NotifyFocusEnter();
        void NotifyFocusLeave();

        static Windows::UI::Xaml::DependencyProperty FontHeightProperty() { return _fontHeightProperty; }
        static Windows::UI::Xaml::DependencyProperty FontWidthProperty() { return _fontWidthProperty; }

        static void OnCompositionChanged(Windows::UI::Xaml::DependencyObject const&, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const&);

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CurrentCursorPosition, _currentCursorPositionHandlers, TerminalControl::TSFInputControl, TerminalControl::CursorPositionEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CurrentFontInfo, _currentFontInfoHandlers, TerminalControl::TSFInputControl, TerminalControl::FontInfoEventArgs);
        DECLARE_EVENT(CompositionCompleted, _compositionCompletedHandlers, TerminalControl::CompositionCompletedEventArgs);

    private:
        static Windows::UI::Xaml::DependencyProperty _fontHeightProperty;
        static Windows::UI::Xaml::DependencyProperty _fontWidthProperty;

        void _layoutRequestedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::UI::Text::Core::CoreTextLayoutRequestedEventArgs const& args);
        void _compositionStartedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::UI::Text::Core::CoreTextCompositionStartedEventArgs const& args);
        void _compositionCompletedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::UI::Text::Core::CoreTextCompositionCompletedEventArgs const& args);
        void _focusRemovedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::Foundation::IInspectable const& object);
        void _selectionRequestedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::UI::Text::Core::CoreTextSelectionRequestedEventArgs const& args);
        void _textRequestedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::UI::Text::Core::CoreTextTextRequestedEventArgs const& args);
        void _selectionUpdatingHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::UI::Text::Core::CoreTextSelectionUpdatingEventArgs const& args);
        void _textUpdatingHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::UI::Text::Core::CoreTextTextUpdatingEventArgs const& args);
        void _formatUpdatingHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, winrt::Windows::UI::Text::Core::CoreTextFormatUpdatingEventArgs const& args);

        Windows::UI::Xaml::Controls::Canvas _canvas;
        Windows::UI::Xaml::Controls::TextBlock _textBlock;

        Windows::UI::Text::Core::CoreTextEditContext _editContext;

        std::wstring _inputBuffer;

        void _Create();
    };
}
namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    struct TSFInputControl : TSFInputControlT<TSFInputControl, implementation::TSFInputControl>
    {
    };
}
