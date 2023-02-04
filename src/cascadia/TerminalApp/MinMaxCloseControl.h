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

        void _MinimizeClick(const WF::IInspectable& sender,
                            const WUX::RoutedEventArgs& e);
        void _MaximizeClick(const WF::IInspectable& sender,
                            const WUX::RoutedEventArgs& e);
        void _CloseClick(const WF::IInspectable& sender,
                         const WUX::RoutedEventArgs& e);

        TYPED_EVENT(MinimizeClick, MTApp::MinMaxCloseControl, WUX::RoutedEventArgs);
        TYPED_EVENT(MaximizeClick, MTApp::MinMaxCloseControl, WUX::RoutedEventArgs);
        TYPED_EVENT(CloseClick, MTApp::MinMaxCloseControl, WUX::RoutedEventArgs);

        std::shared_ptr<ThrottledFuncTrailing<WUXC::Button>> _displayToolTip{ nullptr };
        std::optional<CaptionButton> _lastPressedButton{ std::nullopt };
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(MinMaxCloseControl);
}
