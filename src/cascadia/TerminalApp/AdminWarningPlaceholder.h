// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AdminWarningPlaceholder.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct AdminWarningPlaceholder : AdminWarningPlaceholderT<AdminWarningPlaceholder>
    {
        AdminWarningPlaceholder();
        TYPED_EVENT(PrimaryButtonClicked, TerminalApp::AdminWarningPlaceholder, winrt::Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(CancelButtonClicked, TerminalApp::AdminWarningPlaceholder, winrt::Windows::UI::Xaml::RoutedEventArgs);

    private:
        friend struct AdminWarningPlaceholderT<AdminWarningPlaceholder>; // friend our parent so it can bind private event handlers
        void _primaryButtonClick(winrt::Windows::Foundation::IInspectable const& sender,
                                 winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void _cancelButtonClick(winrt::Windows::Foundation::IInspectable const& sender,
                                winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AdminWarningPlaceholder);
}
