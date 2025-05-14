// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "NewActions.g.h"
#include "ActionsViewModel.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NewActions : public HasScrollViewer<NewActions>, NewActionsT<NewActions>
    {
    public:
        NewActions();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

        void AddNew_Click(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::ActionsViewModel, ViewModel, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(NewActions);
}
