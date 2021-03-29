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

    // TODO: Doint take a Windows::UI::Input::PointerPoint here
    void ControlInteractivity::PointerPressed(const winrt::Windows::UI::Input::PointerPoint point,
                                              const uint32_t modifiers,
                                              const til::point terminalPosition,
                                              const winrt::Windows::Devices::Input::PointerDeviceType type)
    {
        if (type == Windows::Devices::Input::PointerDeviceType::Mouse ||
            type == Windows::Devices::Input::PointerDeviceType::Pen)
        {
            // const auto modifiers = static_cast<uint32_t>(args.KeyModifiers());
            // static_cast to a uint32_t because we can't use the WI_IsFlagSet
            // macro directly with a VirtualKeyModifiers
            const auto altEnabled = WI_IsFlagSet(modifiers, static_cast<uint32_t>(VirtualKeyModifiers::Menu));
            const auto shiftEnabled = WI_IsFlagSet(modifiers, static_cast<uint32_t>(VirtualKeyModifiers::Shift));
            const auto ctrlEnabled = WI_IsFlagSet(modifiers, static_cast<uint32_t>(VirtualKeyModifiers::Control));

            const auto cursorPosition = point.Position();
            // const auto terminalPosition = _GetTerminalPosition(cursorPosition);

            // GH#9396: we prioritize hyper-link over VT mouse events
            //
            // !TODO! Before we'd lock the terminal before getting the hyperlink. Do we still need to?
            auto hyperlink = _core->GetHyperlink(terminalPosition);
            if (point.Properties().IsLeftButtonPressed() &&
                ctrlEnabled && !hyperlink.empty())
            {
                const auto clickCount = _NumberOfClicks(cursorPosition, point.Timestamp());
                // Handle hyper-link only on the first click to prevent multiple activations
                if (clickCount == 1)
                {
                    _HyperlinkHandler(hyperlink);
                }
            }
            else if (_CanSendVTMouseInput())
            {
                _TrySendMouseEvent(point);
            }
            else if (point.Properties().IsLeftButtonPressed())
            {
                const auto clickCount = _NumberOfClicks(cursorPosition, point.Timestamp());
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
                    _singleClickTouchdownPos = cursorPosition;
                    _lastMouseClickPosNoSelection = cursorPosition;
                }
                const bool isOnOriginalPosition = _lastMouseClickPosNoSelection == cursorPosition;

                _core->LeftClickOnTerminal(terminalPosition,
                                           multiClickMapper,
                                           altEnabled,
                                           shiftEnabled,
                                           isOnOriginalPosition,
                                           _selectionNeedsToBeCopied);
            }
            else if (point.Properties().IsRightButtonPressed())
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
        else if (type == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            const auto contactRect = point.Properties().ContactRect();
            // Set our touch rect, to start a pan.
            _touchAnchor = winrt::Windows::Foundation::Point{ contactRect.X, contactRect.Y };
        }
    }

    // Method Description:
    // - Send this particular mouse event to the terminal.
    //   See Terminal::SendMouseEvent for more information.
    // Arguments:
    // - point: the PointerPoint object representing a mouse event from our XAML input handler
    bool ControlInteractivity::_TrySendMouseEvent(Windows::UI::Input::PointerPoint const& point,
                                                  const til::point terminalPosition)
    {
        const auto props = point.Properties();

        // Get the terminal position relative to the viewport
        // const auto terminalPosition = _GetTerminalPosition(point.Position());

        // Which mouse button changed state (and how)
        unsigned int uiButton{};
        switch (props.PointerUpdateKind())
        {
        case winrt::Windows::UI::Input::PointerUpdateKind::LeftButtonPressed:
            uiButton = WM_LBUTTONDOWN;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::LeftButtonReleased:
            uiButton = WM_LBUTTONUP;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::MiddleButtonPressed:
            uiButton = WM_MBUTTONDOWN;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::MiddleButtonReleased:
            uiButton = WM_MBUTTONUP;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::RightButtonPressed:
            uiButton = WM_RBUTTONDOWN;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::RightButtonReleased:
            uiButton = WM_RBUTTONUP;
            break;
        default:
            uiButton = WM_MOUSEMOVE;
        }

        // Mouse wheel data
        const short sWheelDelta = ::base::saturated_cast<short>(props.MouseWheelDelta());
        if (sWheelDelta != 0 && !props.IsHorizontalMouseWheel())
        {
            // if we have a mouse wheel delta and it wasn't a horizontal wheel motion
            uiButton = WM_MOUSEWHEEL;
        }

        const auto modifiers = _GetPressedModifierKeys();
        const TerminalInput::MouseButtonState state{ props.IsLeftButtonPressed(),
                                                     props.IsMiddleButtonPressed(),
                                                     props.IsRightButtonPressed() };
        return _core->SendMouseEvent(terminalPosition, uiButton, modifiers, sWheelDelta, state);
    }

    void ControlInteractivity::_HyperlinkHandler(const std::wstring_view uri)
    {
        // Save things we need to resume later.
        winrt::hstring heldUri{ uri };
        auto hyperlinkArgs = winrt::make_self<OpenHyperlinkEventArgs>(heldUri);
        _OpenHyperlinkHandlers(*this, *hyperlinkArgs);
    }

}
