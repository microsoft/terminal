// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "AdminWarningPlaceholder.h"
#include "AdminWarningPlaceholder.g.cpp"
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;

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

        CancelButton().LayoutUpdated([this](auto&&, auto&&) { CancelButton().Focus(FocusState::Programmatic); });
        //LayoutUpdated([this](auto&&, auto&&) {
        //    if (auto automationPeer{ FrameworkElementAutomationPeer::FromElement(ApproveCommandlineWarningTitle()) })
        //    {
        //        //auto foo{ automationPeer.try_as<FrameworkElementAutomationPeer>() };
        //        //foo.RaiseStructureChangedEvent(Automation::Peers::AutomationStructureChangeType::ChildrenBulkAdded,
        //        //                               automationPeer);

        //        //automationPeer.RaiseNotificationEvent(
        //        //    AutomationNotificationKind::ActionCompleted,
        //        //    AutomationNotificationProcessing::CurrentThenMostRecent,
        //        //    L"Foo",
        //        //    L"ApproveCommandlineWarningTitle" /* unique name for this notification category */
        //        //);

        //        automationPeer.RaiseAutomationEvent(AutomationEvents::StructureChanged);
        //    }

        //});
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

    void AdminWarningPlaceholder::FocusOnLaunch() {
        CancelButton().Focus(FocusState::Programmatic);
    }
}
