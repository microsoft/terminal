// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"

#include "MinMaxCloseControl.h"
#include "MinMaxCloseControl.g.cpp"
#include <LibraryResources.h>

using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    static void closeToolTipForButton(const Controls::Button& button)
    {
        if (auto tt{ Controls::ToolTipService::GetToolTip(button) })
        {
            if (auto tooltip{ tt.try_as<Controls::ToolTip>() })
            {
                tooltip.IsOpen(false);
            }
        }
    }

    MinMaxCloseControl::MinMaxCloseControl()
    {
        // Get our dispatcher. This will get us the same dispatcher as
        // Dispatcher(), but it's a DispatcherQueue, so we can use it with
        // ThrottledFunc
        auto dispatcher = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();

        InitializeComponent();

        // Get the tooltip hover time from the system, or default to 400ms
        // (which should be the default, see:
        // https://docs.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-trackmouseevent#remarks)
        unsigned int hoverTimeoutMillis{ 400 };
        LOG_IF_WIN32_BOOL_FALSE(SystemParametersInfoW(SPI_GETMOUSEHOVERTIME, 0, &hoverTimeoutMillis, 0));
        const auto toolTipInterval = std::chrono::milliseconds(hoverTimeoutMillis);

        // Create a ThrottledFunc for opening the tooltip after the hover
        // timeout. If we hover another button, we should make sure to call
        // Run() with the new button. Calling `_displayToolTip.Run(nullptr)`,
        // which will cause us to not display a tooltip, which is used when we
        // leave the control entirely.
        _displayToolTip = std::make_shared<ThrottledFuncTrailing<Controls::Button>>(
            dispatcher,
            toolTipInterval,
            [weakThis = get_weak()](Controls::Button button) {
                // If we provide a button, then open the tooltip on that button.
                // We can "dismiss" this throttled func by calling it with null,
                // which will cause us to do nothing at the end of the timeout
                // instead.
                if (button)
                {
                    if (auto tt{ Controls::ToolTipService::GetToolTip(button) })
                    {
                        if (auto tooltip{ tt.try_as<Controls::ToolTip>() })
                        {
                            tooltip.IsOpen(true);
                        }
                    }
                }
            });
    }

    // These event handlers simply forward each buttons click events up to the
    // events we've exposed.
    void MinMaxCloseControl::_MinimizeClick(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                            const RoutedEventArgs& e)
    {
        _MinimizeClickHandlers(*this, e);
    }

    void MinMaxCloseControl::_MaximizeClick(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                            const RoutedEventArgs& e)
    {
        _MaximizeClickHandlers(*this, e);
    }
    void MinMaxCloseControl::_CloseClick(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                         const RoutedEventArgs& e)
    {
        _CloseClickHandlers(*this, e);
    }

    void MinMaxCloseControl::SetWindowVisualState(WindowVisualState visualState)
    {
        // Look up the heights we should use for the caption buttons from our
        // XAML resources. "CaptionButtonHeightWindowed" and
        // "CaptionButtonHeightMaximized" define the size we should use for the
        // caption buttons height for the windowed and maximized states,
        // respectively.
        //
        // use C++11 magic statics to make sure we only do this once.
        static auto heights = [this]() {
            const auto res = Resources();
            const auto windowedHeightKey = winrt::box_value(L"CaptionButtonHeightWindowed");
            const auto maximizedHeightKey = winrt::box_value(L"CaptionButtonHeightMaximized");

            auto windowedHeight = 0.0;
            auto maximizedHeight = 0.0;
            if (res.HasKey(windowedHeightKey))
            {
                const auto valFromResources = res.Lookup(windowedHeightKey);
                windowedHeight = winrt::unbox_value_or<double>(valFromResources, 0.0);
            }
            if (res.HasKey(maximizedHeightKey))
            {
                const auto valFromResources = res.Lookup(maximizedHeightKey);
                maximizedHeight = winrt::unbox_value_or<double>(valFromResources, 0.0);
            }
            return std::tuple<double, double>{ windowedHeight, maximizedHeight };
        }();
        static const auto windowedHeight = std::get<0>(heights);
        static const auto maximizedHeight = std::get<1>(heights);

        switch (visualState)
        {
        case WindowVisualState::WindowVisualStateMaximized:
            VisualStateManager::GoToState(MaximizeButton(), L"WindowStateMaximized", false);

            MinimizeButton().Height(maximizedHeight);
            MaximizeButton().Height(maximizedHeight);
            CloseButton().Height(maximizedHeight);
            MaximizeToolTip().Text(RS_(L"WindowRestoreDownButtonToolTip"));
            break;

        case WindowVisualState::WindowVisualStateNormal:
        case WindowVisualState::WindowVisualStateIconified:
        default:
            VisualStateManager::GoToState(MaximizeButton(), L"WindowStateNormal", false);

            MinimizeButton().Height(windowedHeight);
            MaximizeButton().Height(windowedHeight);
            CloseButton().Height(windowedHeight);
            MaximizeToolTip().Text(RS_(L"WindowMaximizeButtonToolTip"));
            break;
        }
    }

    // Method Description:
    // - Called when the mouse hovers a button.
    //   - Transition that button to `PointerOver`
    //   - run the throttled func with this button, to display the tooltip after
    //     a timeout
    //   - dismiss any open tooltips on other buttons.
    // Arguments:
    // - button: the button that was hovered
    void MinMaxCloseControl::HoverButton(CaptionButton button)
    {
        // Keep track of the button that's been pressed. we get a mouse move
        // message when we open the tooltip. If we move the mouse on top of this
        // button, that we've already pressed, then no need to move to the
        // "hovered" state, we should stay in the pressed state.
        if (_lastPressedButton && _lastPressedButton.value() == button)
        {
            return;
        }

        switch (button)
        {
            // Make sure to use true for the useTransitions parameter, to
            // animate the fade in/out transition between colors.
        case CaptionButton::Minimize:
            VisualStateManager::GoToState(MinimizeButton(), L"PointerOver", true);
            VisualStateManager::GoToState(MaximizeButton(), L"Normal", true);
            VisualStateManager::GoToState(CloseButton(), L"Normal", true);

            _displayToolTip->Run(MinimizeButton());
            closeToolTipForButton(MaximizeButton());
            closeToolTipForButton(CloseButton());
            break;
        case CaptionButton::Maximize:
            VisualStateManager::GoToState(MinimizeButton(), L"Normal", true);
            VisualStateManager::GoToState(MaximizeButton(), L"PointerOver", true);
            VisualStateManager::GoToState(CloseButton(), L"Normal", true);

            closeToolTipForButton(MinimizeButton());
            _displayToolTip->Run(MaximizeButton());
            closeToolTipForButton(CloseButton());
            break;
        case CaptionButton::Close:
            VisualStateManager::GoToState(MinimizeButton(), L"Normal", true);
            VisualStateManager::GoToState(MaximizeButton(), L"Normal", true);
            VisualStateManager::GoToState(CloseButton(), L"PointerOver", true);

            closeToolTipForButton(MinimizeButton());
            closeToolTipForButton(MaximizeButton());
            _displayToolTip->Run(CloseButton());
            break;
        }
    }

    // Method Description:
    // - Called when the mouse presses down on a button. NOT when it is
    //   released. That's handled one level above, in
    //   TitleBarControl::ReleaseButtons
    //   - Transition that button to `Pressed`
    // Arguments:
    // - button: the button that was pressed
    void MinMaxCloseControl::PressButton(CaptionButton button)
    {
        switch (button)
        {
        case CaptionButton::Minimize:
            VisualStateManager::GoToState(MinimizeButton(), L"Pressed", true);
            VisualStateManager::GoToState(MaximizeButton(), L"Normal", true);
            VisualStateManager::GoToState(CloseButton(), L"Normal", true);
            break;
        case CaptionButton::Maximize:
            VisualStateManager::GoToState(MinimizeButton(), L"Normal", true);
            VisualStateManager::GoToState(MaximizeButton(), L"Pressed", true);
            VisualStateManager::GoToState(CloseButton(), L"Normal", true);
            break;
        case CaptionButton::Close:
            VisualStateManager::GoToState(MinimizeButton(), L"Normal", true);
            VisualStateManager::GoToState(MaximizeButton(), L"Normal", true);
            VisualStateManager::GoToState(CloseButton(), L"Pressed", true);
            break;
        }
        _lastPressedButton = button;
    }

    // Method Description:
    // - Called when buttons are no longer hovered or pressed. Return them all
    //   to the normal state, and dismiss the tooltips.
    void MinMaxCloseControl::ReleaseButtons()
    {
        _displayToolTip->Run(nullptr);
        VisualStateManager::GoToState(MinimizeButton(), L"Normal", true);
        VisualStateManager::GoToState(MaximizeButton(), L"Normal", true);
        VisualStateManager::GoToState(CloseButton(), L"Normal", true);

        closeToolTipForButton(MinimizeButton());
        closeToolTipForButton(MaximizeButton());
        closeToolTipForButton(CloseButton());

        _lastPressedButton = std::nullopt;
    }
}
