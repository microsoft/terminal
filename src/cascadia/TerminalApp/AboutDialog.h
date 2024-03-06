// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AboutDialog.g.h"

namespace winrt::TerminalApp::implementation
{
    struct AboutDialog : AboutDialogT<AboutDialog>
    {
    public:
        AboutDialog();

        winrt::hstring ApplicationDisplayName();
        winrt::hstring ApplicationVersion();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(bool, UpdatesAvailable, _PropertyChangedHandlers, false);
        WINRT_OBSERVABLE_PROPERTY(bool, CheckingForUpdates, _PropertyChangedHandlers, false);

    private:
        friend struct AboutDialogT<AboutDialog>; // for Xaml to bind events

        std::chrono::system_clock::time_point _lastUpdateCheck{};

        void _ThirdPartyNoticesOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _SendFeedbackOnClick(const IInspectable& sender, const Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& eventArgs);
        winrt::fire_and_forget _queueUpdateCheck();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AboutDialog);
}
