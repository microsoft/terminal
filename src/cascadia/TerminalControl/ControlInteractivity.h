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

#include "ControlCore.h"
#include "../../renderer/uia/UiaRenderer.hpp"

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
                             Control::IControlAppearance unfocusedAppearance,
                             TerminalConnection::ITerminalConnection connection);

        void GotFocus();
        void LostFocus();
        void UpdateSettings();
        void Initialize();
        Control::ControlCore Core();

        void Close();
        void Detach();

        Control::InteractivityAutomationPeer OnCreateAutomationPeer();
        ::Microsoft::Console::Render::IRenderData* GetRenderData() const;

#pragma region Input Methods
        void PointerPressed(Control::MouseButtonState buttonState,
                            const unsigned int pointerUpdateKind,
                            const uint64_t timestamp,
                            const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                            const Core::Point pixelPosition);
        void TouchPressed(const Core::Point contactPoint);

        void PointerMoved(Control::MouseButtonState buttonState,
                          const unsigned int pointerUpdateKind,
                          const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                          const bool focused,
                          const Core::Point pixelPosition,
                          const bool pointerPressedInBounds);
        void TouchMoved(const Core::Point newTouchPoint,
                        const bool focused);

        void PointerReleased(Control::MouseButtonState buttonState,
                             const unsigned int pointerUpdateKind,
                             const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                             const Core::Point pixelPosition);
        void TouchReleased();

        bool MouseWheel(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                        const int32_t delta,
                        const Core::Point pixelPosition,
                        const Control::MouseButtonState state);

        void UpdateScrollbar(const double newValue);

#pragma endregion

        bool CopySelectionToClipboard(bool singleLine,
                                      const Windows::Foundation::IReference<CopyFormat>& formats);
        void RequestPasteTextFromClipboard();
        void SetEndSelectionPoint(const Core::Point pixelPosition);
        bool ManglePathsForWsl();

        uint64_t Id();
        void AttachToNewControl(const Microsoft::Terminal::Control::IKeyBindings& keyBindings);

        TYPED_EVENT(OpenHyperlink, IInspectable, Control::OpenHyperlinkEventArgs);
        TYPED_EVENT(PasteFromClipboard, IInspectable, Control::PasteFromClipboardEventArgs);
        TYPED_EVENT(ScrollPositionChanged, IInspectable, Control::ScrollPositionChangedArgs);
        TYPED_EVENT(ContextMenuRequested, IInspectable, Control::ContextMenuRequestedEventArgs);

        TYPED_EVENT(Attached, IInspectable, IInspectable);
        TYPED_EVENT(Closed, IInspectable, IInspectable);

    private:
        // NOTE: _uiaEngine must be ordered before _core.
        //
        // ControlCore::AttachUiaEngine receives a IRenderEngine as a raw pointer, which we own.
        // We must ensure that we first destroy the ControlCore before the UiaEngine instance
        // in order to safely resolve this unsafe pointer dependency. Otherwise a deallocated
        // IRenderEngine is accessed when ControlCore calls Renderer::TriggerTeardown.
        // (C++ class members are destroyed in reverse order.)
        std::unique_ptr<::Microsoft::Console::Render::UiaEngine> _uiaEngine;

        winrt::com_ptr<ControlCore> _core{ nullptr };
        unsigned int _rowsToScroll;
        double _internalScrollbarPosition{ 0.0 };

        // If this is set, then we assume we are in the middle of panning the
        //      viewport via touch input.
        std::optional<Core::Point> _touchAnchor;

        using Timestamp = uint64_t;

        // imported from WinUser
        // Used for PointerPoint.Timestamp Property (https://docs.microsoft.com/en-us/uwp/api/windows.ui.input.pointerpoint.timestamp#Windows_UI_Input_PointerPoint_Timestamp)
        Timestamp _multiClickTimer;
        unsigned int _multiClickCounter;
        Timestamp _lastMouseClickTimestamp;
        std::optional<Core::Point> _lastMouseClickPos;
        std::optional<Core::Point> _singleClickTouchdownPos;
        std::optional<Core::Point> _lastMouseClickPosNoSelection;
        // This field tracks whether the selection has changed meaningfully
        // since it was last copied. It's generally used to prevent copyOnSelect
        // from firing when the pointer _just happens_ to be released over the
        // terminal.
        bool _selectionNeedsToBeCopied;

        std::optional<til::point> _lastHoveredCell{ std::nullopt };
        // Track the last hyperlink ID we hovered over
        uint16_t _lastHoveredId{ 0 };

        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval{ std::nullopt };

        uint64_t _id;
        static std::atomic<uint64_t> _nextId;

        unsigned int _numberOfClicks(Core::Point clickPos, Timestamp clickTime);
        void _updateSystemParameterSettings() noexcept;

        void _mouseTransparencyHandler(const int32_t mouseDelta) const;
        void _mouseZoomHandler(const int32_t mouseDelta) const;
        void _mouseScrollHandler(const int32_t mouseDelta,
                                 const Core::Point terminalPosition,
                                 const bool isLeftButtonPressed);

        void _hyperlinkHandler(const std::wstring_view uri);
        bool _canSendVTMouseInput(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers);
        bool _shouldSendAlternateScroll(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers, const int32_t delta);

        void _sendPastedTextToConnection(std::wstring_view wstr);
        til::point _getTerminalPosition(const til::point pixelPosition);

        bool _sendMouseEventHelper(const til::point terminalPosition,
                                   const unsigned int pointerUpdateKind,
                                   const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                   const SHORT wheelDelta,
                                   Control::MouseButtonState buttonState);

        friend class ControlUnitTests::ControlCoreTests;
        friend class ControlUnitTests::ControlInteractivityTests;
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(ControlInteractivity);
}
