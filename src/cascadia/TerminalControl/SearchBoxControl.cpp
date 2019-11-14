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

        _goForwardButton = this->FindName(L"SetGoForwardButton").try_as<Controls::Button>();
        _goBackwardButton = this->FindName(L"SetGoBackwardButton").try_as<Controls::Button>();
    }

    bool SearchBoxControl::GetGoForward()
    {
        return _goForward;
    }

    bool SearchBoxControl::GetIsCaseSensitive()
    {
        return _isCaseSensitive;
    }

    void SearchBoxControl::AutoSuggestBox_QuerySubmitted(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs const& e)
    {
        _searchEventHandler(*this, e.QueryText());
    }

    void SearchBoxControl::_GoBackwardClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _goForward = false;

        // If button is clicked, we show the blue border around the clicked button, and eliminate
        // the border on the other direction control button
        Thickness thickness = ThicknessHelper::FromUniformLength(1);
        _goBackwardButton.BorderThickness(thickness);

        thickness = ThicknessHelper::FromUniformLength(0);
        _goForwardButton.BorderThickness(thickness);
    }

    void SearchBoxControl::_GoForwardClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _goForward = true;

        // If button is clicked, we show the blue border around the clicked button, and eliminate
        // the border on the other direction control button
        Thickness thickness = ThicknessHelper::FromUniformLength(1);
        _goForwardButton.BorderThickness(thickness);

        thickness = ThicknessHelper::FromUniformLength(0);
        _goBackwardButton.BorderThickness(thickness);
    }

    void SearchBoxControl::_MovePositionClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _MovePositionClickedHandler(*this, e);
    }

    void SearchBoxControl::_CaseSensitivityChecked(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _isCaseSensitive = true;
    }

    void SearchBoxControl::_CaseSensitivityUnChecked(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _isCaseSensitive = false;
    }

    void SearchBoxControl::_CloseClick(winrt::Windows::Foundation::IInspectable const& sender, RoutedEventArgs const& e)
    {
        _CloseButtonClickedHanlder(*this, e);
    }

    void SearchBoxControl::Root_SizeChanged(const IInspectable& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e)
    {
    }

    // Events proxies
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(SearchBoxControl, Search, _searchEventHandler, TerminalControl::SearchBoxControl, winrt::hstring);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(SearchBoxControl, CloseButtonClicked, _CloseButtonClickedHanlder, TerminalControl::SearchBoxControl, Windows::UI::Xaml::RoutedEventArgs);
    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(SearchBoxControl, MovePositionClicked, _MovePositionClickedHandler, TerminalControl::SearchBoxControl, Windows::UI::Xaml::RoutedEventArgs);
}
