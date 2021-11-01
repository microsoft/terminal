// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "AdminWarningPlaceholder.h"
#include "AdminWarningPlaceholder.g.cpp"
#include <LibraryResources.h>
using namespace winrt::Windows::UI::Xaml;

namespace winrt::TerminalApp::implementation
{
    AdminWarningPlaceholder::AdminWarningPlaceholder(const winrt::Microsoft::Terminal::Control::TermControl& control, const winrt::hstring& cmdline) :
        _control{ control },
        _Commandline{ cmdline }
    {
        InitializeComponent();
        // If the content we're hosting is a TermControl, then use the control's
        // BG as our BG, to give the impression that it's there, under the
        // dialog.
        if (const auto termControl{ control.try_as<winrt::Microsoft::Terminal::Control::TermControl>() })
        {
            RootGrid().Background(termControl.BackgroundBrush());
        }
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
    winrt::Windows::UI::Xaml::Controls::UserControl AdminWarningPlaceholder::Control()
    {
        return _control;
    }

    // Method Description:
    // - Move the focus to the cancel button by default. This has the LOAD
    //   BEARING side effect of also triggering Narrator to read out the
    //   contents of the dialog. It's unclear why doing this works, but it does.
    // - Using a LayoutUpdated event to trigger the focus change when we're
    //   added to the UI tree did not seem to work.
    // - Whoever is adding us to the UI tree is responsible for calling this!
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AdminWarningPlaceholder::FocusOnLaunch()
    {
        CancelButton().Focus(FocusState::Programmatic);
    }

    winrt::hstring AdminWarningPlaceholder::ControlName()
    {
        return RS_(L"AdminWarningPlaceholderName");
    }

    void AdminWarningPlaceholder::_keyUpHandler(IInspectable const& /*sender*/,
                                                Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e)
    {
        // If the user presses escape, close the dialog (without confirming)
        const auto key = e.OriginalKey();
        if (key == winrt::Windows::System::VirtualKey::Escape)
        {
            _CancelButtonClickedHandlers(*this, e);
            e.Handled(true);
        }
    }
}
