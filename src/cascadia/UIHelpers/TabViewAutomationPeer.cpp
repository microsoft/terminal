// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "common.h"
#include "ResourceAccessor.h"
#include "TabView.h"
#include "TabViewAutomationPeer.h"
#include "Utils.h"

#include "TabViewAutomationPeer.properties.cpp"

TabViewAutomationPeer::TabViewAutomationPeer(winrt::TabView const& owner) :
    ReferenceTracker(owner)
{
}

// IAutomationPeerOverrides
winrt::IInspectable TabViewAutomationPeer::GetPatternCore(winrt::PatternInterface const& patternInterface)
{
    if (patternInterface == winrt::PatternInterface::Selection)
    {
        return *this;
    }
    return __super::GetPatternCore(patternInterface);
}

hstring TabViewAutomationPeer::GetClassNameCore()
{
    return winrt::hstring_name_of<winrt::TabView>();
}

winrt::AutomationControlType TabViewAutomationPeer::GetAutomationControlTypeCore()
{
    return winrt::AutomationControlType::Tab;
}

bool TabViewAutomationPeer::CanSelectMultiple()
{
    return false;
}

bool TabViewAutomationPeer::IsSelectionRequired()
{
    return true;
}

winrt::com_array<winrt::Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple> TabViewAutomationPeer::GetSelection()
{
    if (auto tabView = Owner().try_as<TabView>())
    {
        if (auto tabViewItem = tabView->ContainerFromIndex(tabView->SelectedIndex()).try_as<winrt::TabViewItem>())
        {
            if (auto peer = winrt::FrameworkElementAutomationPeer::CreatePeerForElement(tabViewItem))
            {
                return { ProviderFromPeer(peer) };
            }
        }
    }
    return {};
}
