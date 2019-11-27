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
        _textBox = this->FindName(L"TextBox").try_as<Controls::TextBox>();

        // TO DO: Is there a general way to put all the focusable elements in the
        // collection ? Maybe try DFS
        auto closeButton = this->FindName(L"CloseButton").try_as<Controls::Button>();
        auto checkBox = this->FindName(L"CaseSensitivityCheckBox").try_as<Controls::CheckBox>();
        auto moveButton = this->FindName(L"MoveSearchBoxPositionButton").try_as<Controls::Button>();

        if (closeButton)
            _focusableElements.insert(closeButton);
        if (_textBox)
            _focusableElements.insert(_textBox);
        if (checkBox)
            _focusableElements.insert(checkBox);
        if (moveButton)
            _focusableElements.insert(moveButton);
        if (_goForwardButton)
            _focusableElements.insert(_goForwardButton);
        if (_goBackwardButton)
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

    void SearchBoxControl::QuerySubmitted(winrt::Windows::Foundation::IInspectable const& /*sender*/, Input::KeyRoutedEventArgs const& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            _searchEventHandler(*this, _textBox.Text());
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

    bool SearchBoxControl::ContainsFocus()
    {
        auto focusedElement = Input::FocusManager::GetFocusedElement(this->XamlRoot());
        if (_focusableElements.count(focusedElement) > 0)
        {
            return true;
        }

        return false;
    }

    void SearchBoxControl::_GoBackwardClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _goForward = false;
        _goBackwardButton.IsChecked(true);
        if (_goForwardButton.IsChecked())
        {
            _goForwardButton.IsChecked(false);
        }

        // kick off search
        _searchEventHandler(*this, _textBox.Text());
    }

    void SearchBoxControl::_GoForwardClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, RoutedEventArgs const& /*e*/)
    {
        _goForward = true;
        _goForwardButton.IsChecked(true);
        if (_goBackwardButton.IsChecked())
        {
            _goBackwardButton.IsChecked(false);
        }

        // kick off search
        _searchEventHandler(*this, _textBox.Text());
    }

    void SearchBoxControl::_MovePositionClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        auto moveButton = sender.try_as<Controls::Button>();
        // We rotate the font icon upside down to represent "top" or "bottom"
        if (moveButton)
        {
            auto moveIcon = moveButton.FindName(L"MoveSearchBoxPositionIcon").try_as<Controls::FontIcon>();
            auto rotateTransform = moveIcon.RenderTransform().try_as<Media::RotateTransform>();
            if (rotateTransform.Angle() == 0.0)
            {
                rotateTransform.Angle(180.0);
            }
            else
            {
                rotateTransform.Angle(0.0);
            }
        }
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
