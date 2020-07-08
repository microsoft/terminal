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

        GETSET_PROPERTY(Windows::UI::Text::FontWeight, FontWeight);
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

        winrt::Windows::UI::Text::Core::CoreTextEditContext::TextRequested_revoker _textRequestedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::SelectionRequested_revoker _selectionRequestedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::FocusRemoved_revoker _focusRemovedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::TextUpdating_revoker _textUpdatingRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::SelectionUpdating_revoker _selectionUpdatingRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::FormatUpdating_revoker _formatUpdatingRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::LayoutRequested_revoker _layoutRequestedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::CompositionStarted_revoker _compositionStartedRevoker;
        winrt::Windows::UI::Text::Core::CoreTextEditContext::CompositionCompleted_revoker _compositionCompletedRevoker;

        Windows::UI::Text::Core::CoreTextEditContext _editContext;

        std::wstring _inputBuffer;

        bool _inComposition;
        size_t _activeTextStart;
        void _SendAndClearText();
        void _RedrawCanvas();
        bool _focused;

        til::point _currentTerminalCursorPos;
        double _currentCanvasWidth;
        double _currentTextBlockHeight;
        winrt::Windows::Foundation::Rect _currentControlBounds;
        winrt::Windows::Foundation::Rect _currentTextBounds;
        winrt::Windows::Foundation::Rect _currentWindowBounds;
    };
}
namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    struct TSFInputControl : TSFInputControlT<TSFInputControl, implementation::TSFInputControl>
    {
    };
}
