// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#include "pch.h"
#include "common.h"
#include "ResourceAccessor.h"
#include "TabViewItem.h"
#include "TabViewItemAutomationPeer.h"
#include "Utils.h"
#include "SharedHelpers.h"

#include "TabViewItemAutomationPeer.properties.cpp"

TabViewItemAutomationPeer::TabViewItemAutomationPeer(winrt::TabViewItem const& owner) :
    ReferenceTracker(owner)
{
}

// IAutomationPeerOverrides
winrt::IInspectable TabViewItemAutomationPeer::GetPatternCore(winrt::PatternInterface const& patternInterface)
{
    // We subclass from ListViewItemAutomationPeer without using everything from it,
    // so we need to make sure we are returning TabViewItem's AutomationPeer for selection and drag, and not the parent class version
    if (patternInterface == winrt::PatternInterface::SelectionItem || patternInterface == winrt::PatternInterface::Drag)
    {
        return *this;
    }
    return __super::GetPatternCore(patternInterface);
}

hstring TabViewItemAutomationPeer::GetClassNameCore()
{
    return winrt::hstring_name_of<winrt::TabViewItem>();
}

winrt::AutomationControlType TabViewItemAutomationPeer::GetAutomationControlTypeCore()
{
    return winrt::AutomationControlType::TabItem;
}

winrt::hstring TabViewItemAutomationPeer::GetNameCore()
{
    winrt::hstring returnHString = __super::GetNameCore();

    // If a name hasn't been provided by AutomationProperties.Name in markup:
    if (returnHString.empty())
    {
        if (auto tvi = Owner().try_as<winrt::TabViewItem>())
        {
            returnHString = SharedHelpers::TryGetStringRepresentationFromObject(tvi.Header());
        }
    }

    return returnHString;
}

bool TabViewItemAutomationPeer::IsSelected()
{
    if (auto tvi = Owner().try_as<TabViewItem>())
    {
        return tvi->IsSelected();
    }
    return false;
}

winrt::IRawElementProviderSimple TabViewItemAutomationPeer::SelectionContainer()
{
    if (const auto parent = GetParentTabView())
    {
        if (const auto peer = winrt::FrameworkElementAutomationPeer::CreatePeerForElement(parent))
        {
            return ProviderFromPeer(peer);
        }
    }
    return nullptr;
}

void TabViewItemAutomationPeer::AddToSelection()
{
    Select();
}

void TabViewItemAutomationPeer::RemoveFromSelection()
{
    // Can't unselect in a TabView without knowing next selection
}

void TabViewItemAutomationPeer::Select()
{
    if (const auto* owner = Owner().try_as<TabViewItem>().get())
    {
        owner->IsSelected(true);
    }
}

winrt::TabView TabViewItemAutomationPeer::GetParentTabView()
{
    winrt::TabView parentTabView{ nullptr };

    winrt::TabViewItem tabViewItem = Owner().try_as<winrt::TabViewItem>();
    if (tabViewItem)
    {
        parentTabView = winrt::get_self<TabViewItem>(tabViewItem)->GetParentTabView();
    }
    return parentTabView;
}
