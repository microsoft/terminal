// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "AdminWarningPlaceholder.h"
#include "AdminWarningPlaceholder.g.cpp"
#include <UIAutomationCore.h>
#include <LibraryResources.h>

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

        _layoutUpdatedRevoker = RootGrid().LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // Only let this succeed once.
            _layoutUpdatedRevoker.revoke();

            if (auto automationPeer{ FrameworkElementAutomationPeer::FromElement(ApproveCommandlineWarningTitle()) })
            {
                // automationPeer.try_as<FrameworkElementAutomationPeer>().RaiseStructureChangedEvent(Automation::Peers::AutomationStructureChangeType::ChildrenBulkAdded, ApproveCommandlineWarningPrefixTextBlock());

                automationPeer.RaiseNotificationEvent(
                    AutomationNotificationKind::ActionCompleted,
                    AutomationNotificationProcessing::CurrentThenMostRecent,
                    L"Foo",
                    L"ApproveCommandlineWarningTitle" /* unique name for this notification category */
                );
            }
            CancelButton().Focus(FocusState::Programmatic);
        });
    }
    void AdminWarningPlaceholder::_primaryButtonClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                                      RoutedEventArgs const& e)
    {
        if (auto automationPeer{ FrameworkElementAutomationPeer::FromElement(PrimaryButton()) })
        {
            automationPeer.RaiseNotificationEvent(
                AutomationNotificationKind::ActionCompleted,
                AutomationNotificationProcessing::CurrentThenMostRecent,
                L"PrimaryButton",
                L"_primaryButtonClick" /* unique name for this notification category */
            );
        }

        _PrimaryButtonClickedHandlers(*this, e);
    }
    void AdminWarningPlaceholder::_cancelButtonClick(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                                     RoutedEventArgs const& e)
    {
        if (auto automationPeer{ FrameworkElementAutomationPeer::FromElement(CancelButton()) })
        {
            automationPeer.RaiseNotificationEvent(
                AutomationNotificationKind::ActionCompleted,
                AutomationNotificationProcessing::CurrentThenMostRecent,
                L"CancelButton",
                L"_cancelButtonClick" /* unique name for this notification category */
            );
        }

        _CancelButtonClickedHandlers(*this, e);
    }
    winrt::Windows::UI::Xaml::Controls::UserControl AdminWarningPlaceholder::Control()
    {
        return _control;
    }

    winrt::hstring AdminWarningPlaceholder::ControlName() const
    {
        return RS_(L"AdminWarningPlaceholderControlName");
    }

}
