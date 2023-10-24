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

#include "SearchBoxControl.g.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct SearchBoxControl : SearchBoxControlT<SearchBoxControl>
    {
        static constexpr int32_t MaximumTotalResultsToShowInStatus = 999;
        static constexpr std::wstring_view TotalResultsTooHighStatus = L"999+";
        static constexpr std::wstring_view CurrentIndexTooHighStatus = L"?";
        static constexpr std::wstring_view StatusDelimiter = L"/";

        SearchBoxControl();

        void TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);

        void SetFocusOnTextbox();
        void PopulateTextbox(const winrt::hstring& text);
        bool ContainsFocus();
        void SetStatus(int32_t totalMatches, int32_t currentMatch);
        bool NavigationEnabled();
        void NavigationEnabled(bool enabled);

        void GoBackwardClicked(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*e*/);
        void GoForwardClicked(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*e*/);
        void CloseClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        void TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void CaseSensitivityButtonClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        WINRT_CALLBACK(Search, SearchHandler);
        WINRT_CALLBACK(SearchChanged, SearchHandler);
        TYPED_EVENT(Closed, Control::SearchBoxControl, Windows::UI::Xaml::RoutedEventArgs);

    private:
        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;

        static winrt::hstring _FormatStatus(int32_t totalMatches, int32_t currentMatch);
        static double _TextWidth(winrt::hstring text, double fontSize);
        double _GetStatusMaxWidth();

        bool _GoForward();
        bool _CaseSensitive();
        void _KeyDownHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _CharacterHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::CharacterReceivedRoutedEventArgs& e);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(SearchBoxControl);
}
