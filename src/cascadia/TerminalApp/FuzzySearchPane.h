// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once
#include "winrt/TerminalApp.h"

namespace winrt::TerminalApp::implementation
{
    class FuzzySearchPane : public winrt::implements<FuzzySearchPane, IPaneContent>
    {
    public:
        FuzzySearchPane(const winrt::Microsoft::Terminal::Control::TermControl& control);
        void TextBoxTextChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/, winrt::Windows::UI::Xaml::RoutedEventArgs const& /*e*/);
        void OnTextChanged(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Controls::TextChangedEventArgs const&) const;
        void OnKeyUp(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::Input::KeyRoutedEventArgs const&);

        winrt::Windows::UI::Xaml::FrameworkElement GetRoot();

        void UpdateSettings(const winrt::Microsoft::Terminal::Settings::Model::CascadiaSettings& settings);

        winrt::Windows::Foundation::Size MinimumSize();

        void Focus(winrt::Windows::UI::Xaml::FocusState reason = winrt::Windows::UI::Xaml::FocusState::Programmatic);
        void Close();
        winrt::Microsoft::Terminal::Settings::Model::INewContentArgs GetNewTerminalArgs(BuildStartupKind kind) const;

        winrt::hstring Title() { return L"FuzzySearch"; }
        uint64_t TaskbarState() { return 0; }
        uint64_t TaskbarProgress() { return 0; }
        bool ReadOnly() { return false; }
        winrt::hstring Icon() const;
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() const noexcept { return nullptr; }
        winrt::Windows::UI::Xaml::Media::Brush BackgroundBrush();

        til::typed_event<> ConnectionStateChanged;
        til::typed_event<IPaneContent> CloseRequested;
        til::typed_event<IPaneContent, winrt::TerminalApp::BellEventArgs> BellRequested;
        til::typed_event<IPaneContent> TitleChanged;
        til::typed_event<IPaneContent> TabColorChanged;
        til::typed_event<IPaneContent> TaskbarProgressChanged;
        til::typed_event<IPaneContent> ReadOnlyChanged;
        til::typed_event<IPaneContent> FocusRequested;

    private:
        winrt::Windows::UI::Xaml::Controls::Grid _root{ nullptr };
        winrt::Windows::UI::Xaml::Controls::ListBox _listBox{ nullptr };
        winrt::Windows::UI::Xaml::Controls::TextBox _searchBox{ nullptr };
        winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };
    };
}
