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

        winrt::Windows::Foundation::Rect ContentClipRect() const noexcept;
        double OpenAnimationStartPoint() const noexcept;

        void TextBoxKeyDown(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void Open(std::function<void()> callback);
        void Close();
        bool IsOpen();

        winrt::hstring Text();
        bool GoForward();
        bool CaseSensitive();
        bool RegularExpression();
        void SetFocusOnTextbox();
        void PopulateTextbox(const winrt::hstring& text);
        bool ContainsFocus();
        void SetStatus(int32_t totalMatches, int32_t currentMatch, bool searchRegexInvalid);
        void ClearStatus();

        void GoBackwardClicked(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*e*/);
        void GoForwardClicked(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*e*/);
        void CloseClick(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        void TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void CaseSensitivityButtonClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void RegexButtonClicked(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void SearchBoxPointerPressedHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void SearchBoxPointerReleasedHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        til::event<SearchHandler> Search;
        til::event<SearchHandler> SearchChanged;
        til::typed_event<Control::SearchBoxControl, Windows::UI::Xaml::RoutedEventArgs> Closed;
        til::property_changed_event PropertyChanged;

    private:
        std::unordered_set<winrt::Windows::Foundation::IInspectable> _focusableElements;
        winrt::Windows::Foundation::Rect _contentClipRect{ 0, 0, 0, 0 };
        double _openAnimationStartPoint = 0;
        winrt::Windows::UI::Xaml::FrameworkElement::Loaded_revoker _initialLoadedRevoker;
        bool _initialized = false;
        std::function<void()> _initializedCallback;

        void _Initialize();
        void _UpdateSizeDependents();
        void _ContentClipRect(const winrt::Windows::Foundation::Rect& rect);
        void _OpenAnimationStartPoint(double y);
        void _PlayOpenAnimation();
        void _PlayCloseAnimation();
        bool _AnimationEnabled();

        static winrt::hstring _FormatStatus(int32_t totalMatches, int32_t currentMatch);
        static double _TextWidth(winrt::hstring text, double fontSize);
        double _GetStatusMaxWidth();

        void _KeyDownHandler(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _CharacterHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::CharacterReceivedRoutedEventArgs& e);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(SearchBoxControl);
}
