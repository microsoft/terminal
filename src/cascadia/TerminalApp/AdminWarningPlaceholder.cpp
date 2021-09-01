// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "AdminWarningPlaceholder.h"
#include "AdminWarningPlaceholder.g.cpp"
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    AdminWarningPlaceholder::AdminWarningPlaceholder()
    {
        InitializeComponent();
    }
    void AdminWarningPlaceholder::_primaryButtonClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                                      RoutedEventArgs const& e)
    {
        _PrimaryButtonClickedHandlers(*this, e);
    }
    void AdminWarningPlaceholder::_cancelButtonClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                                     RoutedEventArgs const& e)
    {
        _CancelButtonClickedHandlers(*this, e);
    }
}
