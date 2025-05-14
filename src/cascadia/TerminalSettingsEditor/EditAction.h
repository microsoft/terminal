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

        til::property_changed_event PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(Editor::CommandViewModel, ViewModel, PropertyChanged.raise, nullptr);

    private:
        friend struct EditActionT<EditAction>; // for Xaml to bind events
        Editor::IHostedInWindow _windowRoot;
        Windows::UI::Xaml::DataTemplate _listItemTemplate;
        winrt::Microsoft::Terminal::Settings::Editor::ArgsTemplateSelectors _itemTemplateSelector{ nullptr };
        void _choosingItemContainer(const Windows::UI::Xaml::Controls::ListViewBase& sender, const Windows::UI::Xaml::Controls::ChoosingItemContainerEventArgs& args);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(EditAction);
}
