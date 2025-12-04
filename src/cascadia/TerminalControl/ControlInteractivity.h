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
        void PointerPressed(const uint32_t pointerId,
                            Control::MouseButtonState buttonState,
                            const unsigned int pointerUpdateKind,
                            const uint64_t timestamp,
                            const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                            const Core::Point pixelPosition);
        void TouchPressed(const Core::Point contactPoint);

        bool PointerMoved(const uint32_t pointerId,
                          Control::MouseButtonState buttonState,
                          const unsigned int pointerUpdateKind,
                          const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                          const Core::Point pixelPosition);
        void TouchMoved(const Core::Point newTouchPoint);

        void PointerReleased(const uint32_t pointerId,
                             Control::MouseButtonState buttonState,
                             const unsigned int pointerUpdateKind,
                             const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                             const Core::Point pixelPosition);
        void TouchReleased();

        bool MouseWheel(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                        const Core::Point delta,
                        const Core::Point pixelPosition,
                        const Control::MouseButtonState state);

        void UpdateScrollbar(const float newValue);

#pragma endregion

        bool CopySelectionToClipboard(bool singleLine,
                                      bool withControlSequences,
                                      const CopyFormat formats);
        void RequestPasteTextFromClipboard();
        void SetEndSelectionPoint(const Core::Point pixelPosition);

        uint64_t Id();
        void AttachToNewControl();

        til::typed_event<IInspectable, Control::OpenHyperlinkEventArgs> OpenHyperlink;
        til::typed_event<IInspectable, Control::PasteFromClipboardEventArgs> PasteFromClipboard;
        til::typed_event<IInspectable, Control::ScrollPositionChangedArgs> ScrollPositionChanged;
        til::typed_event<IInspectable, Control::ContextMenuRequestedEventArgs> ContextMenuRequested;

        til::typed_event<IInspectable, IInspectable> Attached;
        til::typed_event<IInspectable, IInspectable> Closed;

    private:
        // NOTE: _uiaEngine must be ordered before _core.
        //
        // ControlCore::AttachUiaEngine receives a IRenderEngine as a raw pointer, which we own.
        // We must ensure that we first destroy the ControlCore before the UiaEngine instance
        // in order to safely resolve this unsafe pointer dependency. Otherwise, a deallocated
        // IRenderEngine is accessed when ControlCore calls Renderer::TriggerTeardown.
        // (C++ class members are destroyed in reverse order.)
        std::unique_ptr<::Microsoft::Console::Render::UiaEngine> _uiaEngine;

        winrt::com_ptr<ControlCore> _core{ nullptr };
        UINT _rowsToScroll = 3;
        float _internalScrollbarPosition = 0;

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

        bool _focused{ false };

        // Auto scroll occurs when user, while selecting, drags cursor outside
        // viewport. View is then scrolled to 'follow' the cursor.
        double _autoScrollVelocity;
        std::optional<uint32_t> _autoScrollingPointerId;
        std::optional<Core::Point> _autoScrollingPointerPoint;
        Windows::System::DispatcherQueueTimer _autoScrollTimer{ nullptr };
        std::optional<std::chrono::high_resolution_clock::time_point> _lastAutoScrollUpdateTime;
        bool _pointerPressedInBounds{ false };

        void _tryStartAutoScroll(const uint32_t id, const Core::Point& point, const double scrollVelocity);
        void _tryStopAutoScroll(const uint32_t pointerId);
        void _updateAutoScroll(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& e);
        double _getAutoScrollSpeed(double cursorDistanceFromBorder) const;

        void _createInteractivityTimers();
        void _destroyInteractivityTimers();

        unsigned int _numberOfClicks(Core::Point clickPos, Timestamp clickTime);
        void _updateSystemParameterSettings() noexcept;

        void _mouseTransparencyHandler(const int32_t mouseDelta) const;
        void _mouseZoomHandler(const int32_t mouseDelta) const;
        void _mouseScrollHandler(const int32_t mouseDelta,
                                 const Core::Point terminalPosition,
                                 const bool isLeftButtonPressed);

        void _hyperlinkHandler(const std::wstring_view uri);
        bool _canSendVTMouseInput(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers);
        bool _shouldSendAlternateScroll(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers, const Core::Point delta);

        til::point _getTerminalPosition(const til::point pixelPosition, bool roundToNearestCell);

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
