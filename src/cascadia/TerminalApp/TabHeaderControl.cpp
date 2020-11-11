// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TabHeaderControl.h"

#include "TabHeaderControl.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    TabHeaderControl::TabHeaderControl()
    {
        InitializeComponent();

        HeaderRenamerTextBox().KeyDown([&](auto&&, auto&&) {
            _receivedKeyDown = true;
        });

        HeaderRenamerTextBox().KeyUp([&](auto&&, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e) {
            if (_receivedKeyDown)
            {
                if (e.OriginalKey() == Windows::System::VirtualKey::Enter)
                {
                    // close the rename box
                    _CloseRenameBox();
                }
                else if (e.OriginalKey() == Windows::System::VirtualKey::Escape)
                {
                    // the user wants to discard the changes they made,
                    // reset the rename box text to the old text and close the rename box
                    HeaderRenamerTextBox().Text(HeaderTextBlock().Text());
                    _CloseRenameBox();
                }
            }
        });
    }

    void TabHeaderControl::UpdateHeaderText(winrt::hstring title)
    {
        HeaderTextBlock().Text(title);
    }

    void TabHeaderControl::SetZoomIcon(Windows::UI::Xaml::Visibility state)
    {
        HeaderZoomIcon().Visibility(state);
    }

    void TabHeaderControl::ConstructTabRenameBox()
    {
        _receivedKeyDown = false;

        HeaderTextBlock().Visibility(Windows::UI::Xaml::Visibility::Collapsed);
        HeaderRenamerTextBox().Visibility(Windows::UI::Xaml::Visibility::Visible);

        HeaderRenamerTextBox().Text(HeaderTextBlock().Text());
        HeaderRenamerTextBox().SelectAll();
        HeaderRenamerTextBox().Focus(Windows::UI::Xaml::FocusState::Programmatic);
    }

    void TabHeaderControl::RenameBoxLostFocusHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                                     Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        _CloseRenameBox();
    }

    void TabHeaderControl::_CloseRenameBox()
    {
        HeaderTextBlock().Text(HeaderRenamerTextBox().Text());

        HeaderRenamerTextBox().Visibility(Windows::UI::Xaml::Visibility::Collapsed);
        HeaderTextBlock().Visibility(Windows::UI::Xaml::Visibility::Visible);
    }
}
