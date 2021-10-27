// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AdminWarningPlaceholder.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct AdminWarningPlaceholder : AdminWarningPlaceholderT<AdminWarningPlaceholder>
    {
        AdminWarningPlaceholder(const winrt::Microsoft::Terminal::Control::TermControl& control, const winrt::hstring& cmdline);
        void FocusOnLaunch();
        winrt::Windows::UI::Xaml::Controls::UserControl Control();
        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Commandline, _PropertyChangedHandlers);
        TYPED_EVENT(PrimaryButtonClicked, TerminalApp::AdminWarningPlaceholder, winrt::Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(CancelButtonClicked, TerminalApp::AdminWarningPlaceholder, winrt::Windows::UI::Xaml::RoutedEventArgs);

    private:
        friend struct AdminWarningPlaceholderT<AdminWarningPlaceholder>; // friend our parent so it can bind private event handlers

        winrt::Microsoft::Terminal::Control::TermControl _control{ nullptr };

        void _primaryButtonClick(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _cancelButtonClick(winrt::Windows::Foundation::IInspectable const& sender,
                                winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
    };
}
