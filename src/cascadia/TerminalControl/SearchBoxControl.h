//
// SearchBoxControl.h
// Declaration of the SearchBoxControl class
//

#pragma once
#include "winrt/Windows.UI.Xaml.h"
#include "winrt/Windows.UI.Xaml.Controls.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

#include "SearchBoxControl.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct SearchBoxControl : SearchBoxControlT<SearchBoxControl>
    {
        SearchBoxControl();

        void AutoSuggestBox_QuerySubmitted(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs const& e);

        bool GetGoForward();
        bool GetIsCaseSensitive();

        bool _goForward; // The the direction of the search, controlled by the buttons with arrows
        bool _isCaseSensitive; // If the search should be case sensitive, controlled by the checkbox

        winrt::Windows::UI::Xaml::Controls::Button _goForwardButton;
        winrt::Windows::UI::Xaml::Controls::Button _goBackwardButton;

        void _GoBackwardClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _GoForwardClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _CloseClick(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _CaseSensitivityChecked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _CaseSensitivityUnChecked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        void Root_SizeChanged(const winrt::Windows::Foundation::IInspectable& sender, winrt::Windows::UI::Xaml::SizeChangedEventArgs const& e);

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CreateSearch, _searchEventHandler, TerminalControl::SearchBoxControl, winrt::hstring);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CloseButtonClicked, _CloseButtonClickedHanlder, TerminalControl::SearchBoxControl, Windows::UI::Xaml::RoutedEventArgs);
    };
}

namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    struct SearchBoxControl : SearchBoxControlT<SearchBoxControl, implementation::SearchBoxControl>
    {
    };
}
