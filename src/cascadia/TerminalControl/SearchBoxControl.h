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

#include "SearchBoxControl.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct SearchBoxControl : SearchBoxControlT<SearchBoxControl>
    {
        SearchBoxControl();

        void TextBoxKeyDown(const WF::IInspectable& /*sender*/, const WUX::Input::KeyRoutedEventArgs& e);

        void SetFocusOnTextbox();
        void PopulateTextbox(const winrt::hstring& text);
        bool ContainsFocus();

        void GoBackwardClicked(const WF::IInspectable& /*sender*/, const WUX::RoutedEventArgs& /*e*/);
        void GoForwardClicked(const WF::IInspectable& /*sender*/, const WUX::RoutedEventArgs& /*e*/);
        void CloseClick(const WF::IInspectable& /*sender*/, const WUX::RoutedEventArgs& e);

        WINRT_CALLBACK(Search, SearchHandler);
        TYPED_EVENT(Closed, Control::SearchBoxControl, WUX::RoutedEventArgs);

    private:
        std::unordered_set<WF::IInspectable> _focusableElements;

        bool _GoForward();
        bool _CaseSensitive();
        void _KeyDownHandler(const WF::IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);
        void _CharacterHandler(const WF::IInspectable& /*sender*/, const WUX::Input::CharacterReceivedRoutedEventArgs& e);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(SearchBoxControl);
}
