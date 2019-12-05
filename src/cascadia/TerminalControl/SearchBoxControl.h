/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- SearchBoxControl.cpp

Abstract:
- the search dialog component used in Terminal Search

Author(s):
- Kaiyu Wang (kawa) 11-27-2019

--*/

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

        void QuerySubmitted(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        bool GetGoForward();
        bool GetIsCaseSensitive();

        void SetFocusOnTextbox();
        bool ContainsFocus();

        bool _goForward; // The direction of the search, controlled by the buttons with arrows
        bool _isCaseSensitive; // If the search should be case sensitive, controlled by the checkbox

        winrt::Windows::UI::Xaml::Controls::Primitives::ToggleButton _goForwardButton;
        winrt::Windows::UI::Xaml::Controls::Primitives::ToggleButton _goBackwardButton;
        winrt::Windows::UI::Xaml::Controls::TextBox _textBox;

        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;

        void _GoBackwardClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/);
        void _GoForwardClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/);
        void _CloseClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _CaseSensitivityClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/);
        void _CheckboxKeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(Search, _searchEventHandler, TerminalControl::SearchBoxControl, winrt::hstring);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CloseButtonClicked, _CloseButtonClickedHanlder, TerminalControl::SearchBoxControl, Windows::UI::Xaml::RoutedEventArgs);
    };
}

namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    struct SearchBoxControl : SearchBoxControlT<SearchBoxControl, implementation::SearchBoxControl>
    {
    };
}
