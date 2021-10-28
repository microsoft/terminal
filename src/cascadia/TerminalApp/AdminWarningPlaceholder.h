/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- AdminWarningPlaceholder

Abstract:
- The AdminWarningPlaceholder is a control used to fill space in a pane and
  present a warning to the user. It holds on to a real control that it should be
  replaced with. It looks just like a ContentDialog, except it exists per-pane,
  whereas a ContentDialog can only be added to take up the whole window.
- The caller should make sure to bind our PrimaryButtonClicked and
  CancelButtonClicked events, to be informed as to which was pressed.

Author(s):
- Mike Griese - September 2021

--*/

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
        winrt::hstring ControlName();
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
        void _keyUpHandler(Windows::Foundation::IInspectable const& sender,
                           Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
    };
}
