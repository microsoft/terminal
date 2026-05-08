// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license information.

#pragma once

#include "TabView.h"
#include "TabViewAutomationPeer.g.h"

class TabViewAutomationPeer :
    public ReferenceTracker<TabViewAutomationPeer,
    winrt::implementation::TabViewAutomationPeerT,
    winrt::ISelectionProvider>
{

public:
    TabViewAutomationPeer(winrt::TabView const& owner);

    // IAutomationPeerOverrides
    winrt::IInspectable GetPatternCore(winrt::PatternInterface const& patternInterface);
    hstring GetClassNameCore();
    winrt::AutomationControlType GetAutomationControlTypeCore();

    // ISelectionProvider
    bool CanSelectMultiple();
    bool IsSelectionRequired();
    winrt::com_array<winrt::Windows::UI::Xaml::Automation::Provider::IRawElementProviderSimple> GetSelection();
};
