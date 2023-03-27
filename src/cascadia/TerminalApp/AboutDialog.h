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
        bool UpdatesAvailable() const;
        winrt::hstring PendingUpdateVersion() const;
        winrt::fire_and_forget QueueUpdateCheck();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(bool, CheckingForUpdates, _PropertyChangedHandlers, false);

    private:
        friend struct AboutDialogT<AboutDialog>; // for Xaml to bind events

        void _SetPendingUpdateVersion(const winrt::hstring& pendingUpdateVersion);

        std::chrono::system_clock::time_point _lastUpdateCheck{};
        winrt::hstring _pendingUpdateVersion;

        void _ThirdPartyNoticesOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _SendFeedbackOnClick(const IInspectable& sender, const Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& eventArgs);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AboutDialog);
}
