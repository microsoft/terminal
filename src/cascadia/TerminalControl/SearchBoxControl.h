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

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct SearchBoxControl : SearchBoxControlT<SearchBoxControl>
    {
        SearchBoxControl();

        void TextBoxKeyDown(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        bool GoForward();
        bool IsCaseSensitive();

        void SetFocusOnTextbox();
        bool ContainsFocus();

        void GoBackwardClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/);
        void GoForwardClicked(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/);
        void CloseClick(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        TYPED_EVENT(Search, TerminalControl::SearchBoxControl, winrt::hstring);
        TYPED_EVENT(Closed, TerminalControl::SearchBoxControl, Windows::UI::Xaml::RoutedEventArgs);

    private:
        bool _goForward; // The direction of the search, controlled by the buttons with arrows

        winrt::Windows::UI::Xaml::Controls::Primitives::ToggleButton _goForwardButton;
        winrt::Windows::UI::Xaml::Controls::Primitives::ToggleButton _goBackwardButton;
        winrt::Windows::UI::Xaml::Controls::Primitives::ToggleButton _caseButton;
        winrt::Windows::UI::Xaml::Controls::TextBox _textBox;

        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;

        void _CharacterHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::Input::CharacterReceivedRoutedEventArgs const& e);
    };
}

namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    struct SearchBoxControl : SearchBoxControlT<SearchBoxControl, implementation::SearchBoxControl>
    {
    };
}
