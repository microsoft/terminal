// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// MinMaxCloseControl.xaml.cpp
// Implementation of the MinMaxCloseControl class
//

#include "pch.h"

#include "MinMaxCloseControl.h"

#include "MinMaxCloseControl.g.cpp"
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    MinMaxCloseControl::MinMaxCloseControl()
    {
        InitializeComponent();
    }

    void MinMaxCloseControl::Maximize()
    {
        VisualStateManager::GoToState(MaximizeButton(), L"WindowStateMaximized", false);
    }

    void MinMaxCloseControl::RestoreDown()
    {
        VisualStateManager::GoToState(MaximizeButton(), L"WindowStateNormal", false);
    }

    // These event handlers simply forward each buttons click events up to the
    // events we've exposed.
    void MinMaxCloseControl::_MinimizeClick(winrt::Windows::Foundation::IInspectable const& sender,
                                            RoutedEventArgs const& e)
    {
        _minimizeClickHandlers(*this, e);
    }

    void MinMaxCloseControl::_MaximizeClick(winrt::Windows::Foundation::IInspectable const& sender,
                                            RoutedEventArgs const& e)
    {
        _maximizeClickHandlers(*this, e);
    }
    void MinMaxCloseControl::_CloseClick(winrt::Windows::Foundation::IInspectable const& sender,
                                         RoutedEventArgs const& e)
    {
        _closeClickHandlers(*this, e);
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MinMaxCloseControl, MinimizeClick, _minimizeClickHandlers, TerminalApp::MinMaxCloseControl, RoutedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MinMaxCloseControl, MaximizeClick, _maximizeClickHandlers, TerminalApp::MinMaxCloseControl, RoutedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MinMaxCloseControl, CloseClick, _closeClickHandlers, TerminalApp::MinMaxCloseControl, RoutedEventArgs);

}
