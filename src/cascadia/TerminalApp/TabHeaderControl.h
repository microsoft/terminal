// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"
#include "inc/cppwinrt_utils.h"

#include "TabHeaderControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TabHeaderControl : TabHeaderControlT<TabHeaderControl>
    {
        TabHeaderControl();
        void BeginRename();

        void RenameBoxLostFocusHandler(winrt::Windows::Foundation::IInspectable const& sender,
                                       winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        WINRT_CALLBACK(TitleChangeRequested, TerminalApp::TitleChangeRequestedArgs);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsPaneZoomed, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(double, RenamerMaxWidth, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsProgressRingActive, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, IsProgressRingIndeterminate, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(bool, BellIndicator, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(uint32_t, ProgressValue, _PropertyChangedHandlers);

    private:
        bool _receivedKeyDown{ false };
        bool _renameCancelled{ false };

        void _CloseRenameBox();
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TabHeaderControl);
}
