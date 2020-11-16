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

        // We'll only process the KeyUp event if we received an initial KeyDown event first.
        // Avoids issue immediately closing the tab rename when we see the enter KeyUp event that was
        // sent to the command palette to trigger the openTabRenamer action in the first place.
        HeaderRenamerTextBox().KeyDown([&](auto&&, auto&&) {
            _receivedKeyDown = true;
        });

        // NOTE: (Preview)KeyDown does not work here. If you use that, we'll
        // remove the TextBox from the UI tree, then the following KeyUp
        // will bubble to the NewTabButton, which we don't want to have
        // happen.
        HeaderRenamerTextBox().KeyUp([&](auto&&, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e) {
            if (_receivedKeyDown)
            {
                if (e.OriginalKey() == Windows::System::VirtualKey::Enter)
                {
                    // User is done making changes, close the rename box
                    _CloseRenameBox();
                }
                else if (e.OriginalKey() == Windows::System::VirtualKey::Escape)
                {
                    // User wants to discard the changes they made,
                    // reset the rename box text to the old text and close the rename box
                    HeaderRenamerTextBox().Text(Title());
                    _CloseRenameBox();
                }
            }
        });
    }

    // Method Description
    // - Updates the current title and bubbles the event up to the UI
    // Arguments:
    // - The desired title
    void TabHeaderControl::UpdateHeaderText(winrt::hstring title)
    {
        Title(title);
    }

    // Method Description:
    // - Updates the zoom icon indicating whether a pane is zoomed
    // Arguments:
    // - The desired visibility state of the zoom icon
    void TabHeaderControl::SetZoomIcon(Windows::UI::Xaml::Visibility state)
    {
        HeaderZoomIcon().Visibility(state);
    }

    // Method Description:
    // - Show the tab rename box for the user to rename the tab title
    // - We automatically use the previous title as the initial text of the box
    void TabHeaderControl::ConstructTabRenameBox()
    {
        _receivedKeyDown = false;

        HeaderTextBlock().Visibility(Windows::UI::Xaml::Visibility::Collapsed);
        HeaderRenamerTextBox().Visibility(Windows::UI::Xaml::Visibility::Visible);

        HeaderRenamerTextBox().Text(Title());
        HeaderRenamerTextBox().SelectAll();
        HeaderRenamerTextBox().Focus(Windows::UI::Xaml::FocusState::Programmatic);
    }

    // Method Description:
    // - Event handler for when the rename box loses focus
    void TabHeaderControl::RenameBoxLostFocusHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                                     Windows::UI::Xaml::RoutedEventArgs const& /*e*/)
    {
        _CloseRenameBox();
    }

    // Method Description:
    // - Hides the rename box and displays the title text block
    // - Sends an event to the hosting tab to let them know we wish to change the title
    //   to whatever is in the renamer box right now - the tab will process that event
    //   and tell us to update our title
    void TabHeaderControl::_CloseRenameBox()
    {
        HeaderRenamerTextBox().Visibility(Windows::UI::Xaml::Visibility::Collapsed);
        HeaderTextBlock().Visibility(Windows::UI::Xaml::Visibility::Visible);

        _HeaderTitleChangedHandlers(HeaderRenamerTextBox().Text());
    }
}
