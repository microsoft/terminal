// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Declaration of the MainUserControl class.
//

#pragma once

#include "MinMaxCloseControl.g.h"
#include <ThrottledFunc.h>

namespace winrt::TerminalApp::implementation
{
    struct MinMaxCloseControl : MinMaxCloseControlT<MinMaxCloseControl>
    {
        MinMaxCloseControl();

        void SetWindowVisualState(WindowVisualState visualState);

        void HoverButton(CaptionButton button);
        void PressButton(CaptionButton button);
        void ReleaseButtons();

        void _MinimizeClick(const winrt::Windows::Foundation::IInspectable& sender,
                            const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _MaximizeClick(const winrt::Windows::Foundation::IInspectable& sender,
                            const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void _CloseClick(const winrt::Windows::Foundation::IInspectable& sender,
                         const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        til::typed_event<TerminalApp::MinMaxCloseControl, winrt::Windows::UI::Xaml::RoutedEventArgs> MinimizeClick;
        til::typed_event<TerminalApp::MinMaxCloseControl, winrt::Windows::UI::Xaml::RoutedEventArgs> MaximizeClick;
        til::typed_event<TerminalApp::MinMaxCloseControl, winrt::Windows::UI::Xaml::RoutedEventArgs> CloseClick;

        std::shared_ptr<ThrottledFuncTrailing<winrt::Windows::UI::Xaml::Controls::Button>> _displayToolTip{ nullptr };
        std::optional<CaptionButton> _lastPressedButton{ std::nullopt };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(MinMaxCloseControl);
}
