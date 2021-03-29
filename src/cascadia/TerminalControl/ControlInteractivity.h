// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

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

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlInteractivity : ControlInteractivityT<ControlInteractivity>
    {
        ControlInteractivity(IControlSettings settings,
                             TerminalConnection::ITerminalConnection connection);

        void GainFocus();
        void UpdateSettings();
        void Initialize();
        void PointerPressed(const winrt::Windows::UI::Input::PointerPoint point,
                            const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                            const bool focused,
                            const til::point terminalPosition,
                            const winrt::Windows::Devices::Input::PointerDeviceType type);
        void PointerMoved(const winrt::Windows::UI::Input::PointerPoint point,
                          const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                          const bool focused,
                          const til::point terminalPosition,
                          const winrt::Windows::Devices::Input::PointerDeviceType type);
        void PointerReleased(const winrt::Windows::UI::Input::PointerPoint point,
                             const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                             const bool focused,
                             const til::point terminalPosition,
                             const winrt::Windows::Devices::Input::PointerDeviceType type);
        bool CopySelectionToClipboard(bool singleLine,
                                      const Windows::Foundation::IReference<CopyFormat>& formats);
        void PasteTextFromClipboard();
        /////////////////////// From Control
        winrt::com_ptr<ControlCore> _core{ nullptr };
        unsigned int _rowsToScroll; // Definitely Control/Interactivity

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
        bool _selectionNeedsToBeCopied; // ->Interactivity

        /////////////////////// From Core
        bool _isReadOnly{ false }; // Probably belongs in Interactivity

        std::optional<COORD> _lastHoveredCell{ std::nullopt };
        // Track the last hyperlink ID we hovered over
        uint16_t _lastHoveredId{ 0 };

        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval{ std::nullopt };

        unsigned int _NumberOfClicks(winrt::Windows::Foundation::Point clickPos, Timestamp clickTime);
        void _UpdateSystemParameterSettings() noexcept;
        bool _TrySendMouseEvent(Windows::UI::Input::PointerPoint const& point,
                                const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                const til::point terminalPosition);
        void _HyperlinkHandler(const std::wstring_view uri);
        bool _CanSendVTMouseInput(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers);
        void _SetEndSelectionPoint(const til::point terminalPosition);

        void _SendPastedTextToConnection(const std::wstring& wstr);
        TYPED_EVENT(OpenHyperlink, IInspectable, Control::OpenHyperlinkEventArgs);
        TYPED_EVENT(PasteFromClipboard, IInspectable, Control::PasteFromClipboardEventArgs);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(ControlInteractivity);
}
