// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - ControlInteractivity.h
//
// Abstract:
// - This is a wrapper for the `ControlCore`. It holds the logic for things like
//   double-click, right click copy/paste, selection, etc. This is intended to
//   be a UI framework-independent abstraction. The methods this layer exposes
//   can be called the same from both the WinUI `TermControl` and the WPF
//   control.
//
// Author:
// - Mike Griese (zadjii-msft) 01-Apr-2021

#pragma once

#include "ControlInteractivity.g.h"
#include "EventArgs.h"
#include "../buffer/out/search.h"
#include "cppwinrt_utils.h"

#include "ControlCore.h"

namespace Microsoft::Console::VirtualTerminal
{
    struct MouseButtonState;
}

namespace ControlUnitTests
{
    class ControlCoreTests;
    class ControlInteractivityTests;
};

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlInteractivity : ControlInteractivityT<ControlInteractivity>
    {
    public:
        ControlInteractivity(IControlSettings settings,
                             TerminalConnection::ITerminalConnection connection);

        void GainFocus();
        void UpdateSettings();
        void Initialize();
        winrt::com_ptr<ControlCore> GetCore();

#pragma region InputMethods
        void PointerPressed(const winrt::Windows::Foundation::Point mouseCursorPosition,
                            ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState buttonState,
                            const unsigned int pointerUpdateKind,
                            const uint64_t timestamp,
                            const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                            const bool focused,
                            const til::point terminalPosition);
        void TouchPressed(const winrt::Windows::Foundation::Point contactPoint);

        void PointerMoved(const winrt::Windows::Foundation::Point mouseCursorPosition,
                          ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState buttonState,
                          const unsigned int pointerUpdateKind,
                          const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                          const bool focused,
                          const til::point terminalPosition);
        void TouchMoved(const winrt::Windows::Foundation::Point newTouchPoint,
                        const bool focused);

        void PointerReleased(::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState buttonState,
                             const unsigned int pointerUpdateKind,
                             const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                             const bool focused,
                             const til::point terminalPosition);
        void TouchReleased();

        bool MouseWheel(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                        const int32_t delta,
                        const til::point terminalPosition,
                        const ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState state);
#pragma endregion

        bool CopySelectionToClipboard(bool singleLine,
                                      const Windows::Foundation::IReference<CopyFormat>& formats);
        void PasteTextFromClipboard();
        void SetEndSelectionPoint(const til::point terminalPosition);

    private:
        winrt::com_ptr<ControlCore> _core{ nullptr };
        unsigned int _rowsToScroll;

        // If this is set, then we assume we are in the middle of panning the
        //      viewport via touch input.
        std::optional<winrt::Windows::Foundation::Point> _touchAnchor;

        using Timestamp = uint64_t;

        // imported from WinUser
        // Used for PointerPoint.Timestamp Property (https://docs.microsoft.com/en-us/uwp/api/windows.ui.input.pointerpoint.timestamp#Windows_UI_Input_PointerPoint_Timestamp)
        Timestamp _multiClickTimer;
        unsigned int _multiClickCounter;
        Timestamp _lastMouseClickTimestamp;
        std::optional<winrt::Windows::Foundation::Point> _lastMouseClickPos;
        std::optional<winrt::Windows::Foundation::Point> _singleClickTouchdownPos;
        std::optional<til::point> _singleClickTouchdownTerminalPos;
        std::optional<winrt::Windows::Foundation::Point> _lastMouseClickPosNoSelection;
        // This field tracks whether the selection has changed meaningfully
        // since it was last copied. It's generally used to prevent copyOnSelect
        // from firing when the pointer _just happens_ to be released over the
        // terminal.
        bool _selectionNeedsToBeCopied;

        std::optional<COORD> _lastHoveredCell{ std::nullopt };
        // Track the last hyperlink ID we hovered over
        uint16_t _lastHoveredId{ 0 };

        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval{ std::nullopt };

        unsigned int _numberOfClicks(winrt::Windows::Foundation::Point clickPos, Timestamp clickTime);
        void _updateSystemParameterSettings() noexcept;
        bool _trySendMouseEvent(const unsigned int updateKind,
                                const ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState buttonState,
                                const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                const til::point terminalPosition);
        bool _trySendMouseWheelEvent(const short scrollDelta,
                                     const ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState buttonState,
                                     const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                     const til::point terminalPosition);

        void _mouseTransparencyHandler(const double mouseDelta);
        void _mouseZoomHandler(const double mouseDelta);
        void _mouseScrollHandler(const double mouseDelta,
                                 const til::point terminalPosition,
                                 const bool isLeftButtonPressed);

        void _hyperlinkHandler(const std::wstring_view uri);
        bool _canSendVTMouseInput(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers);

        void _sendPastedTextToConnection(std::wstring_view wstr);
        void _updateScrollbar(const int newValue);

        TYPED_EVENT(OpenHyperlink, IInspectable, Control::OpenHyperlinkEventArgs);
        TYPED_EVENT(PasteFromClipboard, IInspectable, Control::PasteFromClipboardEventArgs);
        TYPED_EVENT(ScrollPositionChanged, IInspectable, Control::ScrollPositionChangedArgs);

        friend class ControlUnitTests::ControlCoreTests;
        friend class ControlUnitTests::ControlInteractivityTests;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(ControlInteractivity);
}
