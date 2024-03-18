// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ControlInteractivity.h"
#include <DefaultSettings.h>
#include <unicode.hpp>
#include <Utils.h>
#include <LibraryResources.h>
#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/inc/Utils.hpp"
#include "../../buffer/out/search.h"

#include "InteractivityAutomationPeer.h"

#include "ControlInteractivity.g.cpp"

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Console::VirtualTerminal;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

static constexpr unsigned int MAX_CLICK_COUNT = 3;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    std::atomic<uint64_t> ControlInteractivity::_nextId{ 1 };

    static constexpr TerminalInput::MouseButtonState toInternalMouseState(const Control::MouseButtonState& state)
    {
        return TerminalInput::MouseButtonState{
            WI_IsFlagSet(state, MouseButtonState::IsLeftButtonDown),
            WI_IsFlagSet(state, MouseButtonState::IsMiddleButtonDown),
            WI_IsFlagSet(state, MouseButtonState::IsRightButtonDown)
        };
    }

    ControlInteractivity::ControlInteractivity(IControlSettings settings,
                                               Control::IControlAppearance unfocusedAppearance,
                                               TerminalConnection::ITerminalConnection connection) :
        _touchAnchor{ std::nullopt },
        _lastMouseClickTimestamp{},
        _lastMouseClickPos{},
        _selectionNeedsToBeCopied{ false }
    {
        _id = _nextId.fetch_add(1, std::memory_order_relaxed);

        _core = winrt::make_self<ControlCore>(settings, unfocusedAppearance, connection);

        _core->Attached([weakThis = get_weak()](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                self->_AttachedHandlers(*self, nullptr);
            }
        });
    }

    uint64_t ControlInteractivity::Id()
    {
        return _id;
    }

    void ControlInteractivity::Detach()
    {
        if (_uiaEngine)
        {
            // There's a potential race here where we've removed the TermControl
            // from the UI tree, but the UIA engine is in the middle of a paint,
            // and the UIA engine will try to dispatch to the
            // TermControlAutomationPeer, which (is now)/(will very soon be) gone.
            //
            // To alleviate, make sure to disable the UIA engine and remove it,
            // and ALSO disable the renderer. Core.Detach will take care of the
            // WaitForPaintCompletionAndDisable (which will stop the renderer
            // after all current engines are done painting).
            //
            // Simply disabling the UIA engine is not enough, because it's
            // possible that it had already started presenting here.
            LOG_IF_FAILED(_uiaEngine->Disable());
            _core->DetachUiaEngine(_uiaEngine.get());
        }
        _core->Detach();
    }

    void ControlInteractivity::AttachToNewControl(const Microsoft::Terminal::Control::IKeyBindings& keyBindings)
    {
        _core->AttachToNewControl(keyBindings);
    }

    // Method Description:
    // - Updates our internal settings. These settings should be
    //   interactivity-specific. Right now, we primarily update _rowsToScroll
    //   with the current value of SPI_GETWHEELSCROLLLINES.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ControlInteractivity::UpdateSettings()
    {
        _updateSystemParameterSettings();
    }

    void ControlInteractivity::Initialize()
    {
        _updateSystemParameterSettings();

        // import value from WinUser (convert from milli-seconds to micro-seconds)
        _multiClickTimer = GetDoubleClickTime() * 1000;
    }

    Control::ControlCore ControlInteractivity::Core()
    {
        return *_core;
    }

    void ControlInteractivity::Close()
    {
        _ClosedHandlers(*this, nullptr);
        if (_core)
        {
            _core->Close();
        }
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
    unsigned int ControlInteractivity::_numberOfClicks(Core::Point clickPos,
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

    void ControlInteractivity::GotFocus()
    {
        if (_uiaEngine.get())
        {
            THROW_IF_FAILED(_uiaEngine->Enable());
        }

        _core->GotFocus();

        _updateSystemParameterSettings();
    }

    void ControlInteractivity::LostFocus()
    {
        if (_uiaEngine.get())
        {
            THROW_IF_FAILED(_uiaEngine->Disable());
        }

        _core->LostFocus();
    }

    // Method Description
    // - Updates internal params based on system parameters
    void ControlInteractivity::_updateSystemParameterSettings() noexcept
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
    // Arguments:
    // - singleLine: collapse all of the text to one line
    // - formats: which formats to copy (defined by action's CopyFormatting arg). nullptr
    //             if we should defer which formats are copied to the global setting
    bool ControlInteractivity::CopySelectionToClipboard(bool singleLine,
                                                        const Windows::Foundation::IReference<CopyFormat>& formats)
    {
        if (_core)
        {
            // Return false if there's no selection to copy. If there's no
            // selection, returning false will indicate that the actions that
            // triggered this should _not_ be marked as handled, so ctrl+c
            // without a selection can still send ^C
            if (!_core->HasSelection())
            {
                return false;
            }

            // Mark the current selection as copied
            _selectionNeedsToBeCopied = false;

            return _core->CopySelectionToClipboard(singleLine, formats);
        }

        return false;
    }

    // Method Description:
    // - Initiate a paste operation.
    void ControlInteractivity::RequestPasteTextFromClipboard()
    {
        auto args = winrt::make<PasteFromClipboardEventArgs>(
            [core = _core](const winrt::hstring& wstr) {
                core->PasteText(wstr);
            },
            _core->BracketedPasteEnabled());

        // send paste event up to TermApp
        _PasteFromClipboardHandlers(*this, std::move(args));
    }

    void ControlInteractivity::PointerPressed(Control::MouseButtonState buttonState,
                                              const unsigned int pointerUpdateKind,
                                              const uint64_t timestamp,
                                              const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                              const Core::Point pixelPosition)
    {
        const auto terminalPosition = _getTerminalPosition(til::point{ pixelPosition });

        const auto altEnabled = modifiers.IsAltPressed();
        const auto shiftEnabled = modifiers.IsShiftPressed();
        const auto ctrlEnabled = modifiers.IsCtrlPressed();

        // GH#9396: we prioritize hyper-link over VT mouse events
        auto hyperlink = _core->GetHyperlink(terminalPosition.to_core_point());
        if (WI_IsFlagSet(buttonState, MouseButtonState::IsLeftButtonDown) &&
            ctrlEnabled &&
            !hyperlink.empty())
        {
            const auto clickCount = _numberOfClicks(pixelPosition, timestamp);
            // Handle hyper-link only on the first click to prevent multiple activations
            if (clickCount == 1)
            {
                _hyperlinkHandler(hyperlink);
            }
        }
        else if (_canSendVTMouseInput(modifiers))
        {
            _sendMouseEventHelper(terminalPosition, pointerUpdateKind, modifiers, 0, buttonState);
        }
        else if (WI_IsFlagSet(buttonState, MouseButtonState::IsLeftButtonDown))
        {
            const auto clickCount = _numberOfClicks(pixelPosition, timestamp);
            // This formula enables the number of clicks to cycle properly
            // between single-, double-, and triple-click. To increase the
            // number of acceptable click states, simply increment
            // MAX_CLICK_COUNT and add another if-statement
            const auto multiClickMapper = clickCount > MAX_CLICK_COUNT ? ((clickCount + MAX_CLICK_COUNT - 1) % MAX_CLICK_COUNT) + 1 : clickCount;

            // Capture the position of the first click when no selection is active
            if (multiClickMapper == 1)
            {
                _singleClickTouchdownPos = pixelPosition;

                if (!_core->HasSelection())
                {
                    _lastMouseClickPosNoSelection = pixelPosition;
                }
            }
            const auto isOnOriginalPosition = _lastMouseClickPosNoSelection == pixelPosition;

            _core->LeftClickOnTerminal(terminalPosition,
                                       multiClickMapper,
                                       altEnabled,
                                       shiftEnabled,
                                       isOnOriginalPosition,
                                       _selectionNeedsToBeCopied);

            if (_core->HasSelection())
            {
                // GH#9787: if selection is active we don't want to track the touchdown position
                // so that dragging the mouse will extend the selection rather than starting the new one
                _singleClickTouchdownPos = std::nullopt;
            }
        }
        else if (WI_IsFlagSet(buttonState, MouseButtonState::IsRightButtonDown))
        {
            if (_core->Settings().RightClickContextMenu())
            {
                // Let the core know we're about to open a menu here. It has
                // some separate conditional logic based on _where_ the user
                // wanted to open the menu.
                _core->AnchorContextMenu(terminalPosition);

                auto contextArgs = winrt::make<ContextMenuRequestedEventArgs>(til::point{ pixelPosition }.to_winrt_point());
                _ContextMenuRequestedHandlers(*this, contextArgs);
            }
            else
            {
                // Try to copy the text and clear the selection
                const auto successfulCopy = CopySelectionToClipboard(shiftEnabled, nullptr);
                _core->ClearSelection();
                if (_core->CopyOnSelect() || !successfulCopy)
                {
                    // CopyOnSelect: right click always pastes!
                    // Otherwise: no selection --> paste
                    RequestPasteTextFromClipboard();
                }
            }
        }
    }

    void ControlInteractivity::TouchPressed(const Core::Point contactPoint)
    {
        _touchAnchor = contactPoint;
    }

    void ControlInteractivity::PointerMoved(Control::MouseButtonState buttonState,
                                            const unsigned int pointerUpdateKind,
                                            const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                            const bool focused,
                                            const Core::Point pixelPosition,
                                            const bool pointerPressedInBounds)
    {
        const auto terminalPosition = _getTerminalPosition(til::point{ pixelPosition });

        // Short-circuit isReadOnly check to avoid warning dialog
        if (focused && !_core->IsInReadOnlyMode() && _canSendVTMouseInput(modifiers))
        {
            _sendMouseEventHelper(terminalPosition, pointerUpdateKind, modifiers, 0, buttonState);
        }
        // GH#4603 - don't modify the selection if the pointer press didn't
        // actually start _in_ the control bounds. Case in point - someone drags
        // a file into the bounds of the control. That shouldn't send the
        // selection into space.
        else if (focused && pointerPressedInBounds && WI_IsFlagSet(buttonState, MouseButtonState::IsLeftButtonDown))
        {
            if (_singleClickTouchdownPos)
            {
                // Figure out if the user's moved a 1/4th of a cell's smaller axis
                // (practically always the width) away from the clickdown point.
                const auto fontSizeInDips = _core->FontSizeInDips();
                const auto touchdownPoint = *_singleClickTouchdownPos;
                const auto dx = pixelPosition.X - touchdownPoint.X;
                const auto dy = pixelPosition.Y - touchdownPoint.Y;
                const auto w = fontSizeInDips.Width;
                const auto distanceSquared = dx * dx + dy * dy;
                const auto maxDistanceSquared = w * w / 16; // (w / 4)^2

                if (distanceSquared >= maxDistanceSquared)
                {
                    // GH#9955.c: Make sure to use the terminal location of the
                    // _touchdown_ point here. We want to start the selection
                    // from where the user initially clicked, not where they are
                    // now.
                    _core->SetSelectionAnchor(_getTerminalPosition(til::point{ touchdownPoint }));

                    // stop tracking the touchdown point
                    _singleClickTouchdownPos = std::nullopt;
                }
            }

            SetEndSelectionPoint(pixelPosition);
        }

        _core->SetHoveredCell(terminalPosition.to_core_point());
    }

    void ControlInteractivity::TouchMoved(const Core::Point newTouchPoint,
                                          const bool focused)
    {
        if (focused &&
            _touchAnchor)
        {
            const auto anchor = _touchAnchor.value();

            // Our actualFont's size is in pixels, convert to DIPs, which the
            // rest of the Points here are in.
            const auto fontSizeInDips{ _core->FontSizeInDips() };

            // Get the difference between the point we've dragged to and the start of the touch.
            const auto dy = static_cast<float>(newTouchPoint.Y - anchor.Y);

            // Start viewport scroll after we've moved more than a half row of text
            if (std::abs(dy) > (fontSizeInDips.Height / 2.0f))
            {
                // Multiply by -1, because moving the touch point down will
                // create a positive delta, but we want the viewport to move up,
                // so we'll need a negative scroll amount (and the inverse for
                // panning down)
                const auto numRows = dy / -fontSizeInDips.Height;

                const auto currentOffset = _core->ScrollOffset();
                const auto newValue = numRows + currentOffset;

                // Update the Core's viewport position, and raise a
                // ScrollPositionChanged event to update the scrollbar
                UpdateScrollbar(newValue);

                // Use this point as our new scroll anchor.
                _touchAnchor = newTouchPoint;
            }
        }
    }

    void ControlInteractivity::PointerReleased(Control::MouseButtonState buttonState,
                                               const unsigned int pointerUpdateKind,
                                               const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                               const Core::Point pixelPosition)
    {
        const auto terminalPosition = _getTerminalPosition(til::point{ pixelPosition });
        // Short-circuit isReadOnly check to avoid warning dialog
        if (!_core->IsInReadOnlyMode() && _canSendVTMouseInput(modifiers))
        {
            _sendMouseEventHelper(terminalPosition, pointerUpdateKind, modifiers, 0, buttonState);
            return;
        }

        // Only a left click release when copy on select is active should perform a copy.
        // Right clicks and middle clicks should not need to do anything when released.
        const auto isLeftMouseRelease = pointerUpdateKind == WM_LBUTTONUP;

        if (_core->CopyOnSelect() &&
            isLeftMouseRelease &&
            _selectionNeedsToBeCopied)
        {
            // IMPORTANT!
            // DO NOT clear the selection here!
            // Otherwise, the selection will be cleared immediately after you make it.
            CopySelectionToClipboard(false, nullptr);
        }

        _singleClickTouchdownPos = std::nullopt;
    }

    void ControlInteractivity::TouchReleased()
    {
        _touchAnchor = std::nullopt;
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
                                          const Core::Point pixelPosition,
                                          const Control::MouseButtonState buttonState)
    {
        const auto terminalPosition = _getTerminalPosition(til::point{ pixelPosition });

        // Short-circuit isReadOnly check to avoid warning dialog.
        //
        // GH#3321: Alternate scroll mode is a special type of mouse input mode
        // where the terminal sends arrow keys when the user mouse wheels, but
        // the client app doesn't care for other mouse input. It's tracked
        // separately from _canSendVTMouseInput.
        if (!_core->IsInReadOnlyMode() &&
            (_canSendVTMouseInput(modifiers) || _shouldSendAlternateScroll(modifiers, delta)))
        {
            // Most mouse event handlers call
            //      _trySendMouseEvent(point);
            // here with a PointerPoint. However, as of #979, we don't have a
            // PointerPoint to work with. So, we're just going to do a
            // mousewheel event manually
            return _sendMouseEventHelper(terminalPosition,
                                         WM_MOUSEWHEEL,
                                         modifiers,
                                         ::base::saturated_cast<short>(delta),
                                         buttonState);
        }

        const auto ctrlPressed = modifiers.IsCtrlPressed();
        const auto shiftPressed = modifiers.IsShiftPressed();

        if (ctrlPressed && shiftPressed)
        {
            _mouseTransparencyHandler(delta);
        }
        else if (ctrlPressed)
        {
            _mouseZoomHandler(delta);
        }
        else
        {
            _mouseScrollHandler(delta, pixelPosition, WI_IsFlagSet(buttonState, MouseButtonState::IsLeftButtonDown));
        }
        return false;
    }

    // Method Description:
    // - Adjust the opacity of the acrylic background in response to a mouse
    //   scrolling event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    void ControlInteractivity::_mouseTransparencyHandler(const int32_t mouseDelta) const
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
    void ControlInteractivity::_mouseZoomHandler(const int32_t mouseDelta) const
    {
        const auto fontDelta = mouseDelta < 0 ? -1.0f : 1.0f;
        _core->AdjustFontSize(fontDelta);
    }

    // Method Description:
    // - Scroll the visible viewport in response to a mouse wheel event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    // - pixelPosition: the location of the mouse during this event
    // - isLeftButtonPressed: true iff the left mouse button was pressed during this event.
    void ControlInteractivity::_mouseScrollHandler(const int32_t mouseDelta,
                                                   const Core::Point pixelPosition,
                                                   const bool isLeftButtonPressed)
    {
        // GH#9955.b: Start scrolling from our internal scrollbar position. This
        // lets us accumulate fractional numbers of rows to scroll with each
        // event. Especially for precision trackpads, we might be getting scroll
        // deltas smaller than a single row, but we still want lots of those to
        // accumulate.
        //
        // At the start, let's compare what we _think_ the scrollbar is, with
        // what it should be. It's possible the core scrolled out from
        // underneath us. We wouldn't know - we don't want the overhead of
        // another ScrollPositionChanged handler. If the scrollbar should be
        // somewhere other than where it is currently, then start from that row.
        const auto currentInternalRow = ::base::saturated_cast<int>(::std::round(_internalScrollbarPosition));
        const auto currentCoreRow = _core->ScrollOffset();
        const auto currentOffset = currentInternalRow == currentCoreRow ?
                                       _internalScrollbarPosition :
                                       currentCoreRow;

        // negative = down, positive = up
        // However, for us, the signs are flipped.
        // With one of the precision mice, one click is always a multiple of 120 (WHEEL_DELTA),
        // but the "smooth scrolling" mode results in non-int values
        const auto rowDelta = mouseDelta / (-1.0 * WHEEL_DELTA);

        // WHEEL_PAGESCROLL is a Win32 constant that represents the "scroll one page
        // at a time" setting. If we ignore it, we will scroll a truly absurd number
        // of rows.
        const auto rowsToScroll{ _rowsToScroll == WHEEL_PAGESCROLL ? ::base::saturated_cast<double>(_core->ViewHeight()) : _rowsToScroll };
        auto newValue = (rowsToScroll * rowDelta) + (currentOffset);

        // Update the Core's viewport position, and raise a
        // ScrollPositionChanged event to update the scrollbar
        UpdateScrollbar(newValue);

        if (isLeftButtonPressed)
        {
            // If user is mouse selecting and scrolls, they then point at new
            // character. Make sure selection reflects that immediately.
            SetEndSelectionPoint(pixelPosition);
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
    void ControlInteractivity::UpdateScrollbar(const double newValue)
    {
        // Set this as the new value of our internal scrollbar representation.
        // We're doing this so we can accumulate fractional amounts of a row to
        // scroll each time the mouse scrolls.
        _internalScrollbarPosition = std::clamp<double>(newValue, 0.0, _core->BufferHeight());

        // If the new scrollbar position, rounded to an int, is at a different
        // row, then actually update the scroll position in the core, and raise
        // a ScrollPositionChanged to inform the control.
        auto viewTop = ::base::saturated_cast<int>(::std::round(_internalScrollbarPosition));
        if (viewTop != _core->ScrollOffset())
        {
            _core->UserScrollViewport(viewTop);

            // _core->ScrollOffset() is now set to newValue
            _ScrollPositionChangedHandlers(*this,
                                           winrt::make<ScrollPositionChangedArgs>(_core->ScrollOffset(),
                                                                                  _core->ViewHeight(),
                                                                                  _core->BufferHeight()));
        }
    }

    void ControlInteractivity::_hyperlinkHandler(const std::wstring_view uri)
    {
        _OpenHyperlinkHandlers(*this, winrt::make<OpenHyperlinkEventArgs>(winrt::hstring{ uri }));
    }

    bool ControlInteractivity::_canSendVTMouseInput(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers)
    {
        // If the user is holding down Shift, suppress mouse events
        // TODO GH#4875: disable/customize this functionality
        if (modifiers.IsShiftPressed())
        {
            return false;
        }
        return _core->IsVtMouseModeEnabled();
    }

    bool ControlInteractivity::_shouldSendAlternateScroll(const ::Microsoft::Terminal::Core::ControlKeyStates modifiers, const int32_t delta)
    {
        // If the user is holding down Shift, suppress mouse events
        // TODO GH#4875: disable/customize this functionality
        if (modifiers.IsShiftPressed())
        {
            return false;
        }
        return _core->ShouldSendAlternateScroll(WM_MOUSEWHEEL, delta);
    }

    // Method Description:
    // - Sets selection's end position to match supplied cursor position, e.g. while mouse dragging.
    // Arguments:
    // - cursorPosition: in pixels, relative to the origin of the control
    void ControlInteractivity::SetEndSelectionPoint(const Core::Point pixelPosition)
    {
        _core->SetEndSelectionPoint(_getTerminalPosition(til::point{ pixelPosition }));
        _selectionNeedsToBeCopied = true;
    }

    // Method Description:
    // - Gets the corresponding viewport terminal position for the point in
    //   pixels, by normalizing with the font size.
    // Arguments:
    // - pixelPosition: the (x,y) position of a given point (i.e.: mouse cursor).
    //    NOTE: origin (0,0) is top-left.
    // Return Value:
    // - the corresponding viewport terminal position for the given Point parameter
    til::point ControlInteractivity::_getTerminalPosition(const til::point pixelPosition)
    {
        // Get the size of the font, which is in pixels
        const til::size fontSize{ _core->GetFont().GetSize() };
        // Convert the location in pixels to characters within the current viewport.
        return pixelPosition / fontSize;
    }

    bool ControlInteractivity::_sendMouseEventHelper(const til::point terminalPosition,
                                                     const unsigned int pointerUpdateKind,
                                                     const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                                                     const SHORT wheelDelta,
                                                     Control::MouseButtonState buttonState)
    {
        const auto adjustment = _core->ScrollOffset() > 0 ? _core->BufferHeight() - _core->ScrollOffset() - _core->ViewHeight() : 0;
        // If the click happened outside the active region, just don't send any mouse event
        if (const auto adjustedY = terminalPosition.y - adjustment; adjustedY >= 0)
        {
            return _core->SendMouseEvent({ terminalPosition.x, adjustedY }, pointerUpdateKind, modifiers, wheelDelta, toInternalMouseState(buttonState));
        }
        return false;
    }

    // Method Description:
    // - Creates an automation peer for the Terminal Control, enabling
    //   accessibility on our control.
    // - Our implementation implements the ITextProvider pattern, and the
    //   IControlAccessibilityInfo, to connect to the UiaEngine, which must be
    //   attached to the core's renderer.
    // - The TermControlAutomationPeer will connect this to the UI tree.
    // Arguments:
    // - None
    // Return Value:
    // - The automation peer for our control
    Control::InteractivityAutomationPeer ControlInteractivity::OnCreateAutomationPeer()
    try
    {
        const auto autoPeer = winrt::make_self<implementation::InteractivityAutomationPeer>(this);
        if (_uiaEngine)
        {
            _core->DetachUiaEngine(_uiaEngine.get());
        }
        _uiaEngine = std::make_unique<::Microsoft::Console::Render::UiaEngine>(autoPeer.get());
        _core->AttachUiaEngine(_uiaEngine.get());
        return *autoPeer;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return nullptr;
    }

    ::Microsoft::Console::Render::IRenderData* ControlInteractivity::GetRenderData() const
    {
        return _core->GetRenderData();
    }

    // Method Description:
    // - Used by the TermControl to know if it should translate drag-dropped
    //   paths into WSL-friendly paths.
    // Arguments:
    // - <none>
    // Return Value:
    // - true if the connection we were created with was a WSL profile.
    bool ControlInteractivity::ManglePathsForWsl()
    {
        return _core->Settings().ProfileSource() == L"Windows.Terminal.Wsl";
    }
}
