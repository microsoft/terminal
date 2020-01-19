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

    // These event handlers simply forward each buttons click events up to the
    // events we've exposed.
    void MinMaxCloseControl::_MinimizeClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                            RoutedEventArgs const& e)
    {
        _minimizeClickHandlers(*this, e);
    }

    void MinMaxCloseControl::_MaximizeClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                            RoutedEventArgs const& e)
    {
        _maximizeClickHandlers(*this, e);
    }
    void MinMaxCloseControl::_CloseClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                         RoutedEventArgs const& e)
    {
        _closeClickHandlers(*this, e);
    }

    void MinMaxCloseControl::SetWindowVisualState(WindowVisualState visualState)
    {
        switch (visualState)
        {
        case WindowVisualState::WindowVisualStateMaximized:
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(MaximizeButton(), L"WindowStateMaximized", false);
            break;
        case WindowVisualState::WindowVisualStateNormal:
        case WindowVisualState::WindowVisualStateIconified:
        default:
            winrt::Windows::UI::Xaml::VisualStateManager::GoToState(MaximizeButton(), L"WindowStateNormal", false);
            break;
        }
    }

    void MinMaxCloseControl::RefreshButtons()
    {
        _RefreshButton(MinimizeButton());
        _RefreshButton(MaximizeButton());
        _RefreshButton(CloseButton());
    }

    void MinMaxCloseControl::_RefreshButton(winrt::Windows::UI::Xaml::Controls::Button const& button)
    {
        const winrt::hstring commonStates = L"CommonStates";
        const winrt::hstring normalState = L"Normal";
        VisualState currentState;
        const auto states = winrt::Windows::UI::Xaml::VisualStateManager::GetVisualStateGroups(button);
        for (unsigned int i = 0; i < states.Size(); i++)
        {
            auto stateGroup = states.GetAt(i);
            if (stateGroup.Name() == commonStates)
                currentState = stateGroup.CurrentState();
        }
        // default to 'Normal'
        winrt::hstring stateName = currentState.Name() == L"" ? normalState : currentState.Name();
        // since the button state is refreshed only when picking tab color, we
        // can safely pick pointerover as the start state for the toggle
        winrt::Windows::UI::Xaml::VisualStateManager::GoToState(button, L"PointerOver", false);
        winrt::Windows::UI::Xaml::VisualStateManager::GoToState(button, stateName, false);
    }

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MinMaxCloseControl, MinimizeClick, _minimizeClickHandlers, TerminalApp::MinMaxCloseControl, RoutedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MinMaxCloseControl, MaximizeClick, _maximizeClickHandlers, TerminalApp::MinMaxCloseControl, RoutedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(MinMaxCloseControl, CloseClick, _closeClickHandlers, TerminalApp::MinMaxCloseControl, RoutedEventArgs);

}
