// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "TSFInputControl.g.h"
#include "CursorPositionEventArgs.g.h"
#include "FontInfoEventArgs.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct CursorPositionEventArgs :
        public CursorPositionEventArgsT<CursorPositionEventArgs>
    {
    public:
        CursorPositionEventArgs() = default;

        WINRT_PROPERTY(Windows::Foundation::Point, CurrentPosition);
    };

    struct FontInfoEventArgs :
        public FontInfoEventArgsT<FontInfoEventArgs>
    {
    public:
        FontInfoEventArgs() = default;

        WINRT_PROPERTY(Windows::Foundation::Size, FontSize);

        WINRT_PROPERTY(winrt::hstring, FontFace);

        WINRT_PROPERTY(Windows::UI::Text::FontWeight, FontWeight);
    };

    struct TSFInputControl : TSFInputControlT<TSFInputControl>
    {
    public:
        TSFInputControl();

        void NotifyFocusEnter();
        void NotifyFocusLeave();
        void ClearBuffer();
        void TryRedrawCanvas();

        void Close();

        // -------------------------------- WinRT Events ---------------------------------
        TYPED_EVENT(CurrentCursorPosition, Control::TSFInputControl, Control::CursorPositionEventArgs);
        TYPED_EVENT(CurrentFontInfo, Control::TSFInputControl, Control::FontInfoEventArgs);
        WINRT_CALLBACK(CompositionCompleted, Control::CompositionCompletedEventArgs);

    private:
        void _layoutRequestedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::UI::Text::Core::CoreTextLayoutRequestedEventArgs& args);
        void _compositionStartedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::UI::Text::Core::CoreTextCompositionStartedEventArgs& args);
        void _compositionCompletedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::UI::Text::Core::CoreTextCompositionCompletedEventArgs& args);
        void _focusRemovedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::Foundation::IInspectable& object);
        void _selectionRequestedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::UI::Text::Core::CoreTextSelectionRequestedEventArgs& args);
        void _textRequestedHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::UI::Text::Core::CoreTextTextRequestedEventArgs& args);
        void _selectionUpdatingHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::UI::Text::Core::CoreTextSelectionUpdatingEventArgs& args);
        void _textUpdatingHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::UI::Text::Core::CoreTextTextUpdatingEventArgs& args);
        void _formatUpdatingHandler(winrt::Windows::UI::Text::Core::CoreTextEditContext sender, const winrt::Windows::UI::Text::Core::CoreTextFormatUpdatingEventArgs& args);

        void _SendAndClearText();
        void _RedrawCanvas();

        winrt::Windows::UI::Text::Core::CoreTextEditContext::TextRequested_revoker _textRequestedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::SelectionRequested_revoker _selectionRequestedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::FocusRemoved_revoker _focusRemovedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::TextUpdating_revoker _textUpdatingRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::SelectionUpdating_revoker _selectionUpdatingRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::FormatUpdating_revoker _formatUpdatingRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::LayoutRequested_revoker _layoutRequestedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::CompositionStarted_revoker _compositionStartedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::CompositionCompleted_revoker _compositionCompletedRevoker;

        Windows::UI::Text::Core::CoreTextEditContext _editContext{ nullptr };
        std::wstring _inputBuffer;
        winrt::Windows::UI::Text::Core::CoreTextRange _selection{};
        size_t _activeTextStart = 0;
        bool _inComposition = false;
        bool _focused = false;

        til::point _currentTerminalCursorPos{};
        double _currentCanvasWidth = 0.0;
        double _currentTextBlockHeight = 0.0;
        winrt::Windows::Foundation::Rect _currentControlBounds{};
        winrt::Windows::Foundation::Rect _currentTextBounds{};
        winrt::Windows::Foundation::Rect _currentWindowBounds{};
    };
}
namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(TSFInputControl);
}
