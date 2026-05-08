// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "TabViewItem.h"
#include "TabViewItemAutomationPeer.g.h"

class TabViewItemAutomationPeer :
    public ReferenceTracker<TabViewItemAutomationPeer,
                            winrt::implementation::TabViewItemAutomationPeerT,
                            winrt::ISelectionItemProvider>
{
public:
    TabViewItemAutomationPeer(winrt::TabViewItem const& owner);

    // IAutomationPeerOverrides
    winrt::IInspectable GetPatternCore(winrt::PatternInterface const& patternInterface);
    winrt::hstring GetNameCore();
    hstring GetClassNameCore();
    winrt::AutomationControlType GetAutomationControlTypeCore();

    // ISelectionItemProvider
    bool IsSelected();
    winrt::IRawElementProviderSimple SelectionContainer();
    void AddToSelection();
    void RemoveFromSelection();
    void Select();

private:
    winrt::TabView GetParentTabView();
};
