// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Actions.g.h"
#include "NavigateToActionsArgs.g.h"
#include "ActionsViewModel.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NavigateToActionsArgs : NavigateToActionsArgsT<NavigateToActionsArgs>
    {
        NavigateToActionsArgs(Editor::ActionsViewModel vm, hstring elementToFocus = {}) :
            _ViewModel(vm),
            _ElementToFocus(elementToFocus) {}

        Editor::ActionsViewModel ViewModel() const noexcept { return _ViewModel; }
        hstring ElementToFocus() const noexcept { return _ElementToFocus; }

    private:
        Editor::ActionsViewModel _ViewModel{ nullptr };
        hstring _ElementToFocus{};
    };

    struct Actions : public HasScrollViewer<Actions>, ActionsT<Actions>
    {
    public:
        Actions();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

        void AddNew_Click(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::ActionsViewModel, ViewModel, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Actions);
}
