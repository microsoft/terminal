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
    SearchBoxControl::SearchBoxControl() :
        _goForward(false)
    {
        InitializeComponent();

        this->CharacterReceived({ this, &SearchBoxControl::_CharacterHandler });

        _textBox = TextBox();

        if (_textBox)
            _focusableElements.insert(_textBox);
        _focusableElements.insert(CloseButton());
        _focusableElements.insert(CaseSensitivityButton());
        _focusableElements.insert(GoForwardButton());
        _focusableElements.insert(GoBackwardButton());
    }

    // Method Description:
    // - Getter for _goForward
    // Arguments:
    // - <none>
    // Return Value:
    // - bool: the value of _goForward
    bool SearchBoxControl::GoForward()
    {
        return _goForward;
    }

    // Method Description:
    // - Get the current state of the case button
    // Arguments:
    // - <none>
    // Return Value:
    // - bool: whether the case button is checked (sensitive)
    //   or not
    bool SearchBoxControl::IsCaseSensitive()
    {
        return _caseButton.IsChecked().GetBoolean();
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
                // We do not want the direction flag to change permanately
                _goForward = !_goForward;
                _SearchHandlers(*this, _textBox.Text());
                _goForward = !_goForward;
            }
            else
            {
                _SearchHandlers(*this, _textBox.Text());
            }
            e.Handled(true);
        }
    }

    // Method Description:
    // - Handler for pressing Enter on TextBox, trigger
    //   text search
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void SearchBoxControl::SetFocusOnTextbox()
    {
        if (_textBox)
        {
            Input::FocusManager::TryFocusAsync(_textBox, FocusState::Keyboard);
            _textBox.SelectAll();
        }
    }

    // Method Description:
    // - Check if the current focus is on any element within the
    //   search box
    // Arguments:
    // - <none>
    // Return Value:
    // - bool: whether the current focus is on the search box
    bool SearchBoxControl::ContainsFocus()
    {
        auto focusedElement = Input::FocusManager::GetFocusedElement(this->XamlRoot());
        if (_focusableElements.count(focusedElement) > 0)
        {
            return true;
        }

        return false;
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
        _goForward = false;
        GoBackwardButton().IsChecked(true);
        if (GoForwardButton().IsChecked())
        {
            GoForwardButton().IsChecked(false);
        }

        // kick off search
        _SearchHandlers(*this, _textBox.Text());
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
        _goForward = true;
        GoForwardButton().IsChecked(true);
        if (GoBackwardButton().IsChecked())
        {
            GoBackwardButton().IsChecked(false);
        }

        // kick off search
        _SearchHandlers(*this, _textBox.Text());
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
