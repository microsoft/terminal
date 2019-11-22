//
// SearchBoxControl.cpp
// Implementation of the SearchBoxControl class
//

#include "pch.h"
#include "SearchBoxControl.h"
#include "SearchBoxControl.g.cpp"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    SearchBoxControl::SearchBoxControl() :
        _goForward(false),
        _isCaseSensitive(false)
    {
        InitializeComponent();

        _goForwardButton = this->FindName(L"SetGoForwardButton").try_as<Controls::Primitives::ToggleButton>();
        _goBackwardButton = this->FindName(L"SetGoBackwardButton").try_as<Controls::Primitives::ToggleButton>();

        // TO DO: Is there a general way to put all the focusable elements in the
        // collection ? Maybe try DFS
        auto closeButton = this->FindName(L"CloseButton").try_as<Controls::Button>();
        auto suggestBox = this->FindName(L"TextBox").try_as<Controls::TextBox>();
        auto checkBox = this->FindName(L"CaseSensitivityCheckBox").try_as<Controls::CheckBox>();
        auto moveButton = this->FindName(L"MoveSearchBoxPositionButton").try_as<Controls::Primitives::ToggleButton>();

        _focusableElements.insert(closeButton);
        _focusableElements.insert(suggestBox);
        _focusableElements.insert(checkBox);
        _focusableElements.insert(moveButton);
        _focusableElements.insert(_goForwardButton);
        _focusableElements.insert(_goBackwardButton);
    }

    bool SearchBoxControl::GetGoForward()
    {
        return _goForward;
    }

    bool SearchBoxControl::GetIsCaseSensitive()
    {
        return _isCaseSensitive;
    }

    void SearchBoxControl::QuerySubmitted(winrt::Windows::Foundation::IInspectable const& sender, Input::KeyRoutedEventArgs const& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            _searchEventHandler(*this, sender.try_as<Controls::TextBox>().Text());
        }
    }

    void SearchBoxControl::SetFocusOnTextbox()
    {
        auto suggestBox = this->FindName(L"TextBox").try_as<Controls::TextBox>();
        if (suggestBox)
        {
            Input::FocusManager::TryFocusAsync(suggestBox, FocusState::Keyboard);
        }
    }

    bool SearchBoxControl::CheckSearchBoxElement(IInspectable const& focusedElement)
    {
        if (_focusableElements.count(focusedElement) > 0)
        {
            return true;
        }

        return false;
    }

    void SearchBoxControl::TurnAroundFocus(IInspectable const& focusedElement)
    {
        // If the current focused element is the move button, we set
        // focus to the close button to enable "turn around"
        // Due to some unknown reason, this will set focus on the TextBox instead of
        // the close button, but this realizes navagation. Close button could be accessed
        // via Esc
        auto moveButton = this->FindName(L"MoveSearchBoxPositionButton").try_as<Controls::Primitives::ToggleButton>();
        if (focusedElement == moveButton)
        {
            auto closeButton = this->FindName(L"CloseButton").try_as<Controls::Button>();
            Input::FocusManager::TryFocusAsync(closeButton, FocusState::Keyboard);
        }
    }

    void SearchBoxControl::_GoBackwardChecked(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _goForward = false;
        if (_goForwardButton.IsChecked())
        {
            _goForwardButton.IsChecked(false);
        }
    }

    void SearchBoxControl::_GoForwardChecked(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _goForward = true;
        if (_goBackwardButton.IsChecked())
        {
            _goBackwardButton.IsChecked(false);
        }
    }

    void SearchBoxControl::_MovePositionClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _MovePositionClickedHandler(sender, e);
    }

    void SearchBoxControl::_CaseSensitivityChecked(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _isCaseSensitive = true;
    }

    void SearchBoxControl::_CaseSensitivityUnChecked(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _isCaseSensitive = false;
    }

    void SearchBoxControl::_CloseClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& e)
    {
        _CloseButtonClickedHanlder(*this, e);
    }

    void SearchBoxControl::_CheckboxKeyDown(winrt::Windows::Foundation::IInspectable const& sender, Input::KeyRoutedEventArgs const& e)
    {
        // Checkbox does not got checked when focused and pressing Enter, we
        // manually listen to Enter and check here
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            auto checkbox = sender.try_as<Controls::CheckBox>();
            if (checkbox.IsChecked().GetBoolean() == true)
            {
                checkbox.IsChecked(false);
            }
            else
            {
                checkbox.IsChecked(true);
            }
            e.Handled(true);
        }
    }

    // Events proxies
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(SearchBoxControl, Search, _searchEventHandler, TerminalControl::SearchBoxControl, winrt::hstring);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(SearchBoxControl, CloseButtonClicked, _CloseButtonClickedHanlder, TerminalControl::SearchBoxControl, Windows::UI::Xaml::RoutedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(SearchBoxControl, MovePositionClicked, _MovePositionClickedHandler, winrt::Windows::Foundation::IInspectable, Windows::UI::Xaml::RoutedEventArgs);
}
