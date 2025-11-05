// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EditAction.g.h"
#include "ActionsViewModel.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct EditAction : public HasScrollViewer<EditAction>, EditActionT<EditAction>
    {
    public:
        EditAction();
        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        void OnNavigatedFrom(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(Editor::CommandViewModel, ViewModel, PropertyChanged.raise, nullptr);

    private:
        friend struct EditActionT<EditAction>; // for Xaml to bind events
        winrt::Windows::UI::Xaml::FrameworkElement::LayoutUpdated_revoker _layoutUpdatedRevoker;
        winrt::event_token _propagateWindowRootToken{};
        winrt::event_token _focusContainerToken{};
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(EditAction);
}
