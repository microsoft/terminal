// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Declaration of the MainUserControl class.
//

#pragma once

#include "winrt/Windows.UI.Xaml.h"
#include "winrt/Windows.UI.Xaml.Markup.h"
#include "winrt/Windows.UI.Xaml.Interop.h"
#include "MinMaxCloseControl.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct MinMaxCloseControl : MinMaxCloseControlT<MinMaxCloseControl>
    {
        MinMaxCloseControl();

        void SetWindowVisualState(WindowVisualState visualState);

        void _MinimizeClick(winrt::Windows::Foundation::IInspectable const& sender,
                            winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _MaximizeClick(winrt::Windows::Foundation::IInspectable const& sender,
                            winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _CloseClick(winrt::Windows::Foundation::IInspectable const& sender,
                         winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(MinimizeClick, _minimizeClickHandlers, TerminalApp::MinMaxCloseControl, winrt::Windows::UI::Xaml::RoutedEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(MaximizeClick, _maximizeClickHandlers, TerminalApp::MinMaxCloseControl, winrt::Windows::UI::Xaml::RoutedEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CloseClick, _closeClickHandlers, TerminalApp::MinMaxCloseControl, winrt::Windows::UI::Xaml::RoutedEventArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct MinMaxCloseControl : MinMaxCloseControlT<MinMaxCloseControl, implementation::MinMaxCloseControl>
    {
    };
}
