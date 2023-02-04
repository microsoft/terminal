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

        WINRT_PROPERTY(WF::Point, CurrentPosition);
    };

    struct FontInfoEventArgs :
        public FontInfoEventArgsT<FontInfoEventArgs>
    {
    public:
        FontInfoEventArgs() = default;

        WINRT_PROPERTY(WF::Size, FontSize);

        WINRT_PROPERTY(winrt::hstring, FontFace);

        WINRT_PROPERTY(WUT::FontWeight, FontWeight);
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
        TYPED_EVENT(CurrentCursorPosition, MTControl::TSFInputControl, MTControl::CursorPositionEventArgs);
        TYPED_EVENT(CurrentFontInfo, MTControl::TSFInputControl, MTControl::FontInfoEventArgs);
        WINRT_CALLBACK(CompositionCompleted, Control::CompositionCompletedEventArgs);

    private:
        void _layoutRequestedHandler(WUT::Core::CoreTextEditContext sender, const WUT::Core::CoreTextLayoutRequestedEventArgs& args);
        void _compositionStartedHandler(WUT::Core::CoreTextEditContext sender, const WUT::Core::CoreTextCompositionStartedEventArgs& args);
        void _compositionCompletedHandler(WUT::Core::CoreTextEditContext sender, const WUT::Core::CoreTextCompositionCompletedEventArgs& args);
        void _focusRemovedHandler(WUT::Core::CoreTextEditContext sender, const WF::IInspectable& object);
        void _selectionRequestedHandler(WUT::Core::CoreTextEditContext sender, const WUT::Core::CoreTextSelectionRequestedEventArgs& args);
        void _textRequestedHandler(WUT::Core::CoreTextEditContext sender, const WUT::Core::CoreTextTextRequestedEventArgs& args);
        void _selectionUpdatingHandler(WUT::Core::CoreTextEditContext sender, const WUT::Core::CoreTextSelectionUpdatingEventArgs& args);
        void _textUpdatingHandler(WUT::Core::CoreTextEditContext sender, const WUT::Core::CoreTextTextUpdatingEventArgs& args);
        void _formatUpdatingHandler(WUT::Core::CoreTextEditContext sender, const WUT::Core::CoreTextFormatUpdatingEventArgs& args);

        void _SendAndClearText();
        void _RedrawCanvas();

        WUT::Core::CoreTextEditContext::TextRequested_revoker _textRequestedRevoker;
        WUT::Core::CoreTextEditContext::SelectionRequested_revoker _selectionRequestedRevoker;
        WUT::Core::CoreTextEditContext::FocusRemoved_revoker _focusRemovedRevoker;
        WUT::Core::CoreTextEditContext::TextUpdating_revoker _textUpdatingRevoker;
        WUT::Core::CoreTextEditContext::SelectionUpdating_revoker _selectionUpdatingRevoker;
        WUT::Core::CoreTextEditContext::FormatUpdating_revoker _formatUpdatingRevoker;
        WUT::Core::CoreTextEditContext::LayoutRequested_revoker _layoutRequestedRevoker;
        WUT::Core::CoreTextEditContext::CompositionStarted_revoker _compositionStartedRevoker;
        WUT::Core::CoreTextEditContext::CompositionCompleted_revoker _compositionCompletedRevoker;

        WUT::Core::CoreTextEditContext _editContext{ nullptr };
        std::wstring _inputBuffer;
        WUT::Core::CoreTextRange _selection{};
        size_t _activeTextStart = 0;
        bool _inComposition = false;
        bool _focused = false;

        til::point _currentTerminalCursorPos{};
        double _currentCanvasWidth = 0.0;
        double _currentTextBlockHeight = 0.0;
        WF::Rect _currentControlBounds{};
        WF::Rect _currentTextBounds{};
        WF::Rect _currentWindowBounds{};
    };
}
namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(TSFInputControl);
}
