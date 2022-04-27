// Copyright (c) Microsoft Corporation
// Licensed under the MIT license.

#include "pch.h"
#include "SearchBoxControl.h"
#include "SearchBoxControl.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;

namespace winrt::Microsoft::Terminal::Control::implementation
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
    void SearchBoxControl::TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            const auto state = CoreWindow::GetForCurrentThread().GetKeyState(winrt::Windows::System::VirtualKey::Shift);
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
    void SearchBoxControl::_KeyDownHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                           const Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Escape)
        {
            _ClosedHandlers(*this, e);
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
        if (TextBox())
        {
            Input::FocusManager::TryFocusAsync(TextBox(), FocusState::Keyboard);
            TextBox().SelectAll();
        }
    }

    // Method Description:
    // - Allows to set the value of the text to search
    // Arguments:
    // - text: string value to populate in the TextBox
    // Return Value:
    // - <none>
    void SearchBoxControl::PopulateTextbox(const winrt::hstring& text)
    {
        if (TextBox())
        {
            TextBox().Text(text);
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
    void SearchBoxControl::GoBackwardClicked(const winrt::Windows::Foundation::IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
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
    void SearchBoxControl::GoForwardClicked(const winrt::Windows::Foundation::IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
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
    void SearchBoxControl::CloseClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const RoutedEventArgs& e)
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
    void SearchBoxControl::_CharacterHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/, const Input::CharacterReceivedRoutedEventArgs& e)
    {
        e.Handled(true);
    }
}
