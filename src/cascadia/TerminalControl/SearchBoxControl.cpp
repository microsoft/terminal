// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "SearchBoxControl.h"
#include "SearchBoxControl.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    // Constructor
    SearchBoxControl::SearchBoxControl()
    {
        InitializeComponent();

        this->CharacterReceived({ this, &SearchBoxControl::_CharacterHandler });
        this->KeyDown({ this, &SearchBoxControl::_KeyDownHandler });

        _focusableElements.insert(TextBox());
        _focusableElements.insert(CloseButton());
        _focusableElements.insert(CaseSensitivityButton());
        _focusableElements.insert(GoForwardButton());
        _focusableElements.insert(GoBackwardButton());
    }

    // Method Description:
    // - Check if the current search direction is forward
    // Arguments:
    // - <none>
    // Return Value:
    // - bool: the current search direction, determined by the
    //         states of the two direction buttons
    bool SearchBoxControl::_GoForward()
    {
        return GoForwardButton().IsChecked().GetBoolean();
    }

    // Method Description:
    // - Check if the current search is case sensitive
    // Arguments:
    // - <none>
    // Return Value:
    // - bool: whether the current search is case sensitive (case button is checked )
    //   or not
    bool SearchBoxControl::_CaseSensitive()
    {
        return CaseSensitivityButton().IsChecked().GetBoolean();
    }

    // Method Description:
    // - Handler for pressing Enter on TextBox, trigger
    //   text search
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::TextBoxKeyDown(winrt::Windows::Foundation::IInspectable const& /*sender*/, Input::KeyRoutedEventArgs const& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            auto const state = CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Shift);
            if (WI_IsFlagSet(state, CoreVirtualKeyStates::Down))
            {
                _SearchHandlers(TextBox().Text(), !_GoForward(), _CaseSensitive());
            }
            else
            {
                _SearchHandlers(TextBox().Text(), _GoForward(), _CaseSensitive());
            }
            e.Handled(true);
        }
    }

    // Method Description:
    // - Handler for pressing "Esc" when focusing
    //   on the search dialog, this triggers close
    //   event of the Search dialog
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::_KeyDownHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                           Input::KeyRoutedEventArgs const& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Escape)
        {
            _ClosedHandlers(*this, e);
            e.Handled(true);
        }
    }

    // Method Description:
    // - Funnels focus into the textbox and selects it
    // - Selects all text.
    void SearchBoxControl::OnGotFocus(/*const IInspectable& sender,*/ Windows::UI::Xaml::RoutedEventArgs const& args)
    {
        if (args.OriginalSource() == *this)
        {
            // The text box gets focus because it's our first focusable element,
            // but let's be safe.
            TextBox().Focus(FocusState::Keyboard);
            TextBox().SelectAll();
        }
    }

    // Method Description:
    // - Handler for clicking the GoBackward button. This change the value of _goForward,
    //   mark GoBackward button as checked and ensure GoForward button
    //   is not checked
    // Arguments:
    // - sender: not used
    // - e: not used
    // Return Value:
    // - <none>
    void SearchBoxControl::GoBackwardClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        GoBackwardButton().IsChecked(true);
        if (GoForwardButton().IsChecked())
        {
            GoForwardButton().IsChecked(false);
        }

        // kick off search
        _SearchHandlers(TextBox().Text(), _GoForward(), _CaseSensitive());
    }

    // Method Description:
    // - Handler for clicking the GoForward button. This change the value of _goForward,
    //   mark GoForward button as checked and ensure GoBackward button
    //   is not checked
    // Arguments:
    // - sender: not used
    // - e: not used
    // Return Value:
    // - <none>
    void SearchBoxControl::GoForwardClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        GoForwardButton().IsChecked(true);
        if (GoBackwardButton().IsChecked())
        {
            GoBackwardButton().IsChecked(false);
        }

        // kick off search
        _SearchHandlers(TextBox().Text(), _GoForward(), _CaseSensitive());
    }

    // Method Description:
    // - Handler for clicking the close button. This destructs the
    //   search box object in TermControl
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::CloseClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& e)
    {
        _ClosedHandlers(*this, e);
    }

    // Method Description:
    // - To avoid Characters input bubbling up to terminal, we implement this handler here,
    //   simply mark the key input as handled
    // Arguments:
    // - sender: not used
    // - e: event data
    // Return Value:
    // - <none>
    void SearchBoxControl::_CharacterHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/, Input::CharacterReceivedRoutedEventArgs const& e)
    {
        e.Handled(true);
    }
}
