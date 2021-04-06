// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ControlInteractivity.h"
#include <argb.h>
#include <DefaultSettings.h>
#include <unicode.hpp>
#include <Utf16Parser.hpp>
#include <Utils.h>
#include <WinUser.h>
#include <LibraryResources.h>
#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/inc/Utils.hpp"
#include "../../buffer/out/search.h"

#include "ControlInteractivity.g.cpp"

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Console::VirtualTerminal;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    ControlInteractivity::ControlInteractivity(IControlSettings settings,
                                               TerminalConnection::ITerminalConnection connection) :
        _touchAnchor{ std::nullopt },
        _lastMouseClickTimestamp{},
        _lastMouseClickPos{},
        _selectionNeedsToBeCopied{ false }
    {
        _core = winrt::make_self<ControlCore>(settings, connection);
    }

    void ControlInteractivity::UpdateSettings()
    {
        _UpdateSystemParameterSettings();
    }

    void ControlInteractivity::Initialize()
    {
        _UpdateSystemParameterSettings();

        // import value from WinUser (convert from milli-seconds to micro-seconds)
        _multiClickTimer = GetDoubleClickTime() * 1000;
    }

    // Method Description:
    // - Returns the number of clicks that occurred (double and triple click support).
    // Every call to this function registers a click.
    // Arguments:
    // - clickPos: the (x,y) position of a given cursor (i.e.: mouse cursor).
    //    NOTE: origin (0,0) is top-left.
    // - clickTime: the timestamp that the click occurred
    // Return Value:
    // - if the click is in the same position as the last click and within the timeout, the number of clicks within that time window
    // - otherwise, 1
    unsigned int ControlInteractivity::_NumberOfClicks(winrt::Windows::Foundation::Point clickPos,
                                                       Timestamp clickTime)
    {
        // if click occurred at a different location or past the multiClickTimer...
        Timestamp delta;
        THROW_IF_FAILED(UInt64Sub(clickTime, _lastMouseClickTimestamp, &delta));
        if (clickPos != _lastMouseClickPos || delta > _multiClickTimer)
        {
            _multiClickCounter = 1;
        }
        else
        {
            _multiClickCounter++;
        }

        _lastMouseClickTimestamp = clickTime;
        _lastMouseClickPos = clickPos;
        return _multiClickCounter;
    }

    void ControlInteractivity::GainFocus()
    {
        _UpdateSystemParameterSettings();
    }

    // Method Description
    // - Updates internal params based on system parameters
    void ControlInteractivity::_UpdateSystemParameterSettings() noexcept
    {
        if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &_rowsToScroll, 0))
        {
            LOG_LAST_ERROR();
            // If SystemParametersInfoW fails, which it shouldn't, fall back to
            // Windows' default value.
            _rowsToScroll = 3;
        }
    }

    // Method Description:
    // - Given a copy-able selection, get the selected text from the buffer and send it to the
    //     Windows Clipboard (CascadiaWin32:main.cpp).
    // - CopyOnSelect does NOT clear the selection
    // Arguments:
    // - singleLine: collapse all of the text to one line
    // - formats: which formats to copy (defined by action's CopyFormatting arg). nullptr
    //             if we should defer which formats are copied to the global setting
    bool ControlInteractivity::CopySelectionToClipboard(bool singleLine,
                                                        const Windows::Foundation::IReference<CopyFormat>& formats)
    {
        if (_core)
        {
            // Note to future self: This should return false if there's no
            // selection to copy. If there's no selection, returning false will
            // indicate that the actions that trigered this should _not_ be
            // marked as handled, so ctrl+c without a selection can still send
            // ^C

            // Mark the current selection as copied
            _selectionNeedsToBeCopied = false;

            return _core->CopySelectionToClipboard(singleLine, formats);
        }

        return false;
    }

    // Method Description:
    // - Initiate a paste operation.
    void ControlInteractivity::PasteTextFromClipboard()
    {
        // attach TermControl::_SendInputToConnection() as the clipboardDataHandler.
        // This is called when the clipboard data is loaded.
        auto clipboardDataHandler = std::bind(&ControlInteractivity::_SendPastedTextToConnection, this, std::placeholders::_1);
        auto pasteArgs = winrt::make_self<PasteFromClipboardEventArgs>(clipboardDataHandler);

        // send paste event up to TermApp
        _PasteFromClipboardHandlers(*this, *pasteArgs);
    }

    // Method Description:
    // - Pre-process text pasted (presumably from the clipboard)
    //   before sending it over the terminal's connection.
    void ControlInteractivity::_SendPastedTextToConnection(const std::wstring& wstr)
    {
        _core->PasteText(winrt::hstring{ wstr });
    }

    // TODO: Don't take a Windows::UI::Input::PointerPoint here. No WinUI here.
    void ControlInteractivity::PointerPressed(const winrt::Windows::Foundation::Point mouseCursorPosition,
                                              TerminalInput::MouseButtonState buttonState,
                                              const unsigned int pointerUpdateKind,
                                              const uint64_t timestamp,
                                              const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                              const bool /*focused*/,
                                              const til::point terminalPosition)
    {
        const auto altEnabled = modifiers.IsAltPressed();
        const auto shiftEnabled = modifiers.IsShiftPressed();
        const auto ctrlEnabled = modifiers.IsCtrlPressed();

        // const auto cursorPosition = point.Position();

        // GH#9396: we prioritize hyper-link over VT mouse events
        //
        // !TODO! Before we'd lock the terminal before getting the hyperlink. Do we still need to?
        auto hyperlink = _core->GetHyperlink(terminalPosition);
        if (buttonState.isLeftButtonDown &&
            ctrlEnabled && !hyperlink.empty())
        {
            const auto clickCount = _NumberOfClicks(mouseCursorPosition, timestamp);
            // Handle hyper-link only on the first click to prevent multiple activations
            if (clickCount == 1)
            {
                _HyperlinkHandler(hyperlink);
            }
        }
        else if (_CanSendVTMouseInput(modifiers))
        {
            _TrySendMouseEvent(pointerUpdateKind, buttonState, modifiers, terminalPosition);
        }
        else if (buttonState.isLeftButtonDown)
        {
            const auto clickCount = _NumberOfClicks(mouseCursorPosition, timestamp);
            // This formula enables the number of clicks to cycle properly
            // between single-, double-, and triple-click. To increase the
            // number of acceptable click states, simply increment
            // MAX_CLICK_COUNT and add another if-statement
            const unsigned int MAX_CLICK_COUNT = 3;
            const auto multiClickMapper = clickCount > MAX_CLICK_COUNT ? ((clickCount + MAX_CLICK_COUNT - 1) % MAX_CLICK_COUNT) + 1 : clickCount;

            // Capture the position of the first click when no selection is active
            if (multiClickMapper == 1 &&
                !_core->HasSelection())
            {
                _singleClickTouchdownPos = mouseCursorPosition;
                _singleClickTouchdownTerminalPos = terminalPosition;
                _lastMouseClickPosNoSelection = mouseCursorPosition;
            }
            const bool isOnOriginalPosition = _lastMouseClickPosNoSelection == mouseCursorPosition;

            _core->LeftClickOnTerminal(terminalPosition,
                                       multiClickMapper,
                                       altEnabled,
                                       shiftEnabled,
                                       isOnOriginalPosition,
                                       _selectionNeedsToBeCopied);
        }
        else if (buttonState.isRightButtonDown)
        {
            // CopyOnSelect right click always pastes
            if (_core->CopyOnSelect() || !_core->HasSelection())
            {
                PasteTextFromClipboard();
            }
            else
            {
                CopySelectionToClipboard(shiftEnabled, nullptr);
            }
        }
    }

    void ControlInteractivity::Touched(const winrt::Windows::Foundation::Point contactPoint)
    {
        _touchAnchor = contactPoint;
    }

    // TODO: Don't take a Windows::UI::Input::PointerPoint here
    void ControlInteractivity::PointerMoved(const winrt::Windows::Foundation::Point mouseCursorPosition,
                                            TerminalInput::MouseButtonState buttonState,
                                            const unsigned int pointerUpdateKind,
                                            const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                            const bool focused,
                                            const til::point terminalPosition)
    {
        // Short-circuit isReadOnly check to avoid warning dialog
        if (focused && !_core->IsInReadOnlyMode() && _CanSendVTMouseInput(modifiers))
        {
            _TrySendMouseEvent(pointerUpdateKind, buttonState, modifiers, terminalPosition);
        }
        else if (focused && buttonState.isLeftButtonDown)
        {
            if (_singleClickTouchdownPos)
            {
                // Figure out if the user's moved a quarter of a cell's smaller axis away from the clickdown point
                auto& touchdownPoint{ *_singleClickTouchdownPos };
                auto distance{ std::sqrtf(std::powf(mouseCursorPosition.X - touchdownPoint.X, 2) +
                                          std::powf(mouseCursorPosition.Y - touchdownPoint.Y, 2)) };
                const til::size fontSize{ _core->GetFont().GetSize() };

                const auto fontSizeInDips = fontSize.scale(til::math::rounding, 1.0f / _core->RendererScale());
                if (distance >= (std::min(fontSizeInDips.width(), fontSizeInDips.height()) / 4.f))
                {
                    _core->SetSelectionAnchor(terminalPosition);
                    // stop tracking the touchdown point
                    _singleClickTouchdownPos = std::nullopt;
                    _singleClickTouchdownTerminalPos = std::nullopt;
                }
            }

            _SetEndSelectionPoint(terminalPosition);

            // const double cursorBelowBottomDist = cursorPosition.Y - SwapChainPanel().Margin().Top - SwapChainPanel().ActualHeight();
            // const double cursorAboveTopDist = -1 * cursorPosition.Y + SwapChainPanel().Margin().Top;

            // constexpr double MinAutoScrollDist = 2.0; // Arbitrary value
            // double newAutoScrollVelocity = 0.0;
            // if (cursorBelowBottomDist > MinAutoScrollDist)
            // {
            //     newAutoScrollVelocity = _GetAutoScrollSpeed(cursorBelowBottomDist);
            // }
            // else if (cursorAboveTopDist > MinAutoScrollDist)
            // {
            //     newAutoScrollVelocity = -1.0 * _GetAutoScrollSpeed(cursorAboveTopDist);
            // }

            // if (newAutoScrollVelocity != 0)
            // {
            //     _TryStartAutoScroll(point, newAutoScrollVelocity);
            // }
            // else
            // {
            //     _TryStopAutoScroll(ptr.PointerId());
            // }
        }

        _core->UpdateHoveredCell(terminalPosition);
        // else if (focused &&
        //          type == Windows::Devices::Input::PointerDeviceType::Touch &&
        //          _touchAnchor)
        // {
        //     const auto contactRect = point.Properties().ContactRect();
        //     winrt::Windows::Foundation::Point newTouchPoint{ contactRect.X, contactRect.Y };
        //     const auto anchor = _touchAnchor.value();

        //     // Our actualFont's size is in pixels, convert to DIPs, which the
        //     // rest of the Points here are in.
        //     const til::size fontSize{ _core->GetFont().GetSize() };
        //     const auto fontSizeInDips = fontSize.scale(til::math::rounding, 1.0f / _core->RendererScale());

        //     // Get the difference between the point we've dragged to and the start of the touch.
        //     const float dy = newTouchPoint.Y - anchor.Y;

        //     // Start viewport scroll after we've moved more than a half row of text
        //     if (std::abs(dy) > (fontSizeInDips.height<float>() / 2.0f))
        //     {
        //         // Multiply by -1, because moving the touch point down will
        //         // create a positive delta, but we want the viewport to move up,
        //         // so we'll need a negative scroll amount (and the inverse for
        //         // panning down)
        //         const float numRows = -1.0f * (dy / fontSizeInDips.height<float>());

        //         const auto currentOffset = ::base::ClampedNumeric<double>(_core->ScrollOffset());
        //         const auto newValue = numRows + currentOffset;

        //         // Update the Core's viewport position, and raise a
        //         // ScrollPositionChanged event to update the scrollbar
        //         _updateScrollbar(newValue);

        //         // Use this point as our new scroll anchor.
        //         _touchAnchor = newTouchPoint;
        //     }
        // }
    }

    void ControlInteractivity::TouchMoved(const winrt::Windows::Foundation::Point newTouchPoint,
                                          const bool focused)
    {
        if (focused &&
            _touchAnchor)
        {
            const auto anchor = _touchAnchor.value();

            // Our actualFont's size is in pixels, convert to DIPs, which the
            // rest of the Points here are in.
            const til::size fontSize{ _core->GetFont().GetSize() };
            const auto fontSizeInDips = fontSize.scale(til::math::rounding, 1.0f / _core->RendererScale());

            // Get the difference between the point we've dragged to and the start of the touch.
            const float dy = newTouchPoint.Y - anchor.Y;

            // Start viewport scroll after we've moved more than a half row of text
            if (std::abs(dy) > (fontSizeInDips.height<float>() / 2.0f))
            {
                // Multiply by -1, because moving the touch point down will
                // create a positive delta, but we want the viewport to move up,
                // so we'll need a negative scroll amount (and the inverse for
                // panning down)
                const float numRows = -1.0f * (dy / fontSizeInDips.height<float>());

                const auto currentOffset = ::base::ClampedNumeric<double>(_core->ScrollOffset());
                const auto newValue = numRows + currentOffset;

                // Update the Core's viewport position, and raise a
                // ScrollPositionChanged event to update the scrollbar
                _updateScrollbar(newValue);

                // Use this point as our new scroll anchor.
                _touchAnchor = newTouchPoint;
            }
        }
    }

    void ControlInteractivity::PointerReleased(TerminalInput::MouseButtonState buttonState,
                                               const unsigned int pointerUpdateKind,
                                               const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                               const bool /*focused*/,
                                               const til::point terminalPosition)
    {
        // Short-circuit isReadOnly check to avoid warning dialog
        if (!_core->IsInReadOnlyMode() && _CanSendVTMouseInput(modifiers))
        {
            _TrySendMouseEvent(pointerUpdateKind, buttonState, modifiers, terminalPosition);
            // args.Handled(true);
            return;
        }

        // Only a left click release when copy on select is active should perform a copy.
        // Right clicks and middle clicks should not need to do anything when released.
        const bool isLeftMouseRelease = pointerUpdateKind == WM_LBUTTONUP;

        if (_core->CopyOnSelect() &&
            isLeftMouseRelease &&
            _selectionNeedsToBeCopied)
        {
            CopySelectionToClipboard(false, nullptr);
        }

        _singleClickTouchdownPos = std::nullopt;
        _singleClickTouchdownTerminalPos = std::nullopt;
    }

    void ControlInteractivity::TouchReleased()
    {
        _touchAnchor = std::nullopt;
    }

    // // Method Description:
    // // - Send this particular mouse event to the terminal.
    // //   See Terminal::SendMouseEvent for more information.
    // // Arguments:
    // // - point: the PointerPoint object representing a mouse event from our XAML input handler
    // bool ControlInteractivity::_TrySendMouseEvent(Windows::UI::Input::PointerPoint const& point,
    //                                               const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
    //                                               const til::point terminalPosition)
    // {
    //     const auto props = point.Properties();

    //     // Which mouse button changed state (and how)
    //     unsigned int uiButton{};
    //     switch (props.PointerUpdateKind())
    //     {
    //     case winrt::Windows::UI::Input::PointerUpdateKind::LeftButtonPressed:
    //         uiButton = WM_LBUTTONDOWN;
    //         break;
    //     case winrt::Windows::UI::Input::PointerUpdateKind::LeftButtonReleased:
    //         uiButton = WM_LBUTTONUP;
    //         break;
    //     case winrt::Windows::UI::Input::PointerUpdateKind::MiddleButtonPressed:
    //         uiButton = WM_MBUTTONDOWN;
    //         break;
    //     case winrt::Windows::UI::Input::PointerUpdateKind::MiddleButtonReleased:
    //         uiButton = WM_MBUTTONUP;
    //         break;
    //     case winrt::Windows::UI::Input::PointerUpdateKind::RightButtonPressed:
    //         uiButton = WM_RBUTTONDOWN;
    //         break;
    //     case winrt::Windows::UI::Input::PointerUpdateKind::RightButtonReleased:
    //         uiButton = WM_RBUTTONUP;
    //         break;
    //     default:
    //         uiButton = WM_MOUSEMOVE;
    //     }

    //     // Mouse wheel data
    //     const short sWheelDelta = ::base::saturated_cast<short>(props.MouseWheelDelta());
    //     if (sWheelDelta != 0 && !props.IsHorizontalMouseWheel())
    //     {
    //         // if we have a mouse wheel delta and it wasn't a horizontal wheel motion
    //         uiButton = WM_MOUSEWHEEL;
    //     }

    //     const TerminalInput::MouseButtonState state{ props.IsLeftButtonPressed(),
    //                                                  props.IsMiddleButtonPressed(),
    //                                                  props.IsRightButtonPressed() };
    //     return _core->SendMouseEvent(terminalPosition, uiButton, modifiers, sWheelDelta, state);
    // }

    // Method Description:
    // - Send this particular mouse event to the terminal.
    //   See Terminal::SendMouseEvent for more information.
    // Arguments:
    // - point: the PointerPoint object representing a mouse event from our XAML input handler
    bool ControlInteractivity::_TrySendMouseEvent(const unsigned int updateKind,
                                                  const TerminalInput::MouseButtonState buttonState,
                                                  const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                                  const til::point terminalPosition)
    {
        return _core->SendMouseEvent(terminalPosition, updateKind, modifiers, 0, buttonState);
    }

    // Method Description:
    // - Send this particular mouse event to the terminal.
    //   See Terminal::SendMouseEvent for more information.
    // Arguments:
    // - point: the PointerPoint object representing a mouse event from our XAML input handler
    bool ControlInteractivity::_TrySendMouseWheelEvent(const short scrollDelta,
                                                       const TerminalInput::MouseButtonState buttonState,
                                                       const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                                       const til::point terminalPosition)
    {
        return _core->SendMouseEvent(terminalPosition, WM_MOUSEWHEEL, modifiers, scrollDelta, buttonState);
    }

    // Method Description:
    // - Actually handle a scrolling event, whether from a mouse wheel or a
    //   touchpad scroll. Depending upon what modifier keys are pressed,
    //   different actions will take place.
    //   * Attempts to first dispatch the mouse scroll as a VT event
    //   * If Ctrl+Shift are pressed, then attempts to change our opacity
    //   * If just Ctrl is pressed, we'll attempt to "zoom" by changing our font size
    //   * Otherwise, just scrolls the content of the viewport
    // Arguments:
    // - point: the location of the mouse during this event
    // - modifiers: The modifiers pressed during this event, in the form of a VirtualKeyModifiers
    // - delta: the mouse wheel delta that triggered this event.
    bool ControlInteractivity::MouseWheel(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                          const int32_t delta,
                                          const til::point terminalPosition,
                                          const TerminalInput::MouseButtonState state)
    {
        // Short-circuit isReadOnly check to avoid warning dialog
        if (!_core->IsInReadOnlyMode() && _CanSendVTMouseInput(modifiers))
        {
            // Most mouse event handlers call
            //      _TrySendMouseEvent(point);
            // here with a PointerPoint. However, as of #979, we don't have a
            // PointerPoint to work with. So, we're just going to do a
            // mousewheel event manually
            return _core->SendMouseEvent(terminalPosition,
                                         WM_MOUSEWHEEL,
                                         modifiers,
                                         ::base::saturated_cast<short>(delta),
                                         state);
        }

        const auto ctrlPressed = modifiers.IsCtrlPressed();
        const auto shiftPressed = modifiers.IsShiftPressed();

        if (ctrlPressed && shiftPressed)
        {
            _MouseTransparencyHandler(delta);
        }
        else if (ctrlPressed)
        {
            _MouseZoomHandler(delta);
        }
        else
        {
            _MouseScrollHandler(delta, terminalPosition, state.isLeftButtonDown);
        }
        return false;
    }

    // Method Description:
    // - Adjust the opacity of the acrylic background in response to a mouse
    //   scrolling event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    void ControlInteractivity::_MouseTransparencyHandler(const double mouseDelta)
    {
        // Transparency is on a scale of [0.0,1.0], so only increment by .01.
        const auto effectiveDelta = mouseDelta < 0 ? -.01 : .01;
        _core->AdjustOpacity(effectiveDelta);
    }

    // Method Description:
    // - Adjust the font size of the terminal in response to a mouse scrolling
    //   event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    void ControlInteractivity::_MouseZoomHandler(const double mouseDelta)
    {
        const auto fontDelta = mouseDelta < 0 ? -1 : 1;
        _core->AdjustFontSize(fontDelta);
    }

    // Method Description:
    // - Scroll the visible viewport in response to a mouse wheel event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    // - point: the location of the mouse during this event
    // - isLeftButtonPressed: true iff the left mouse button was pressed during this event.
    void ControlInteractivity::_MouseScrollHandler(const double mouseDelta,
                                                   const til::point terminalPosition,
                                                   const bool isLeftButtonPressed)
    {
        const auto currentOffset = _core->ScrollOffset();

        // negative = down, positive = up
        // However, for us, the signs are flipped.
        // With one of the precision mice, one click is always a multiple of 120 (WHEEL_DELTA),
        // but the "smooth scrolling" mode results in non-int values
        const auto rowDelta = mouseDelta / (-1.0 * WHEEL_DELTA);

        // WHEEL_PAGESCROLL is a Win32 constant that represents the "scroll one page
        // at a time" setting. If we ignore it, we will scroll a truly absurd number
        // of rows.
        const auto rowsToScroll{ _rowsToScroll == WHEEL_PAGESCROLL ? _core->ViewHeight() : _rowsToScroll };
        double newValue = (rowsToScroll * rowDelta) + (currentOffset);

        // Update the Core's viewport position, and raise a
        // ScrollPositionChanged event to update the scrollbar
        _updateScrollbar(::base::saturated_cast<int>(newValue));

        if (isLeftButtonPressed)
        {
            // If user is mouse selecting and scrolls, they then point at new
            // character. Make sure selection reflects that immediately.
            _SetEndSelectionPoint(terminalPosition);
        }
    }

    // Method Description:
    // - Update the scroll position in such a way that should update the
    //   scrollbar. For example, when scrolling the buffer with the mouse or
    //   touch input. This will both update the Core's Terminal's buffer
    //   location, then also raise our own ScrollPositionChanged event.
    //   UserScrollViewport _won't_ raise the core's ScrollPositionChanged
    //   event, because it's assumed that's already being called from a context
    //   that knows about the change to the scrollbar. So we need to raise the
    //   event on our own.
    // - The hosting control should make sure to listen to our own
    //   ScrollPositionChanged event and use that as an opportunity to update
    //   the location of the scrollbar.
    // Arguments:
    // - newValue: The new top of the viewport
    // Return Value:
    // - <none>
    void ControlInteractivity::_updateScrollbar(const int newValue)
    {
        _core->UserScrollViewport(newValue);

        // _core->ScrollOffset() is new set to newValue
        auto scrollArgs = winrt::make_self<ScrollPositionChangedArgs>(_core->ScrollOffset(),
                                                                      _core->ViewHeight(),
                                                                      _core->BufferHeight());
        _ScrollPositionChangedHandlers(*this, *scrollArgs);
    }

    void ControlInteractivity::_HyperlinkHandler(const std::wstring_view uri)
    {
        // Save things we need to resume later.
        winrt::hstring heldUri{ uri };
        auto hyperlinkArgs = winrt::make_self<OpenHyperlinkEventArgs>(heldUri);
        _OpenHyperlinkHandlers(*this, *hyperlinkArgs);
    }

    bool ControlInteractivity::_CanSendVTMouseInput(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers)
    {
        // If the user is holding down Shift, suppress mouse events
        // TODO GH#4875: disable/customize this functionality
        if (modifiers.IsShiftPressed())
        {
            return false;
        }
        return _core->IsVtMouseModeEnabled();
    }

    // Method Description:
    // - Sets selection's end position to match supplied cursor position, e.g. while mouse dragging.
    // Arguments:
    // - cursorPosition: in pixels, relative to the origin of the control
    void ControlInteractivity::_SetEndSelectionPoint(const til::point terminalPosition)
    {
        _core->SetEndSelectionPoint(terminalPosition);
        _selectionNeedsToBeCopied = true;
    }
}
