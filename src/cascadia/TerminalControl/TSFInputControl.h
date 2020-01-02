// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

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
        CursorPositionEventArgs() = default;

        GETSET_PROPERTY(Windows::Foundation::Point, CurrentPosition);
    };

    struct FontInfoEventArgs :
        public FontInfoEventArgsT<FontInfoEventArgs>
    {
    public:
        FontInfoEventArgs() = default;

        GETSET_PROPERTY(Windows::Foundation::Size, FontSize);

        GETSET_PROPERTY(winrt::hstring, FontFace);
    };

    struct TSFInputControl : TSFInputControlT<TSFInputControl>
    {
    public:
        TSFInputControl();

        void NotifyFocusEnter();
        void NotifyFocusLeave();

        static void OnCompositionChanged(Windows::UI::Xaml::DependencyObject const&, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const&);

        // -------------------------------- WinRT Events ---------------------------------
        TYPED_EVENT(CurrentCursorPosition, TerminalControl::TSFInputControl, TerminalControl::CursorPositionEventArgs);
        TYPED_EVENT(CurrentFontInfo, TerminalControl::TSFInputControl, TerminalControl::FontInfoEventArgs);
        DECLARE_EVENT(CompositionCompleted, _compositionCompletedHandlers, TerminalControl::CompositionCompletedEventArgs);

    private:
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
