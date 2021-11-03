// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// MinMaxCloseControl.xaml.cpp
// Implementation of the MinMaxCloseControl class
//

#include "pch.h"

#include "MinMaxCloseControl.h"

#include "MinMaxCloseControl.g.cpp"

#include <LibraryResources.h>

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
        _MinimizeClickHandlers(*this, e);
    }

    void MinMaxCloseControl::_MaximizeClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                            RoutedEventArgs const& e)
    {
        _MaximizeClickHandlers(*this, e);
    }
    void MinMaxCloseControl::_CloseClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                         RoutedEventArgs const& e)
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

    void MinMaxCloseControl::HoverButton(CaptionButton button)
    {
        switch (button)
        {
        case CaptionButton::Minimize:
            VisualStateManager::GoToState(MinimizeButton(), L"PointerOver", false);
            VisualStateManager::GoToState(MaximizeButton(), L"Normal", false);
            VisualStateManager::GoToState(CloseButton(), L"Normal", false);
            break;
        case CaptionButton::Maximize:
            VisualStateManager::GoToState(MinimizeButton(), L"Normal", false);
            VisualStateManager::GoToState(MaximizeButton(), L"PointerOver", false);
            VisualStateManager::GoToState(CloseButton(), L"Normal", false);
            break;
        case CaptionButton::Close:
            VisualStateManager::GoToState(MinimizeButton(), L"Normal", false);
            VisualStateManager::GoToState(MaximizeButton(), L"Normal", false);
            VisualStateManager::GoToState(CloseButton(), L"PointerOver", false);
            break;
        }
    }
    void MinMaxCloseControl::PressButton(CaptionButton button)
    {
        switch (button)
        {
        case CaptionButton::Minimize:
            VisualStateManager::GoToState(MinimizeButton(), L"Pressed", false);
            VisualStateManager::GoToState(MaximizeButton(), L"Normal", false);
            VisualStateManager::GoToState(CloseButton(), L"Normal", false);
            break;
        case CaptionButton::Maximize:
            VisualStateManager::GoToState(MinimizeButton(), L"Normal", false);
            VisualStateManager::GoToState(MaximizeButton(), L"Pressed", false);
            VisualStateManager::GoToState(CloseButton(), L"Normal", false);
            break;
        case CaptionButton::Close:
            VisualStateManager::GoToState(MinimizeButton(), L"Normal", false);
            VisualStateManager::GoToState(MaximizeButton(), L"Normal", false);
            VisualStateManager::GoToState(CloseButton(), L"Pressed", false);
            break;
        }
    }
    void MinMaxCloseControl::ReleaseButtons()
    {
        VisualStateManager::GoToState(MinimizeButton(), L"Normal", false);
        VisualStateManager::GoToState(MaximizeButton(), L"Normal", false);
        VisualStateManager::GoToState(CloseButton(), L"Normal", false);
    }
}
