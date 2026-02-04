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

        void ShortcutActionBox_GotFocus(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& args);
        void ShortcutActionBox_TextChanged(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs& args);
        void ShortcutActionBox_SuggestionChosen(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxSuggestionChosenEventArgs& args);
        void ShortcutActionBox_QuerySubmitted(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const winrt::Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs& args);

    private:
        friend struct EditActionT<EditAction>; // for Xaml to bind events
        winrt::Windows::UI::Xaml::FrameworkElement::LayoutUpdated_revoker _layoutUpdatedRevoker;
        Editor::CommandViewModel::PropagateWindowRootRequested_revoker _propagateWindowRootRevoker;
        Editor::CommandViewModel::FocusContainer_revoker _focusContainerRevoker;
        winrt::Windows::Foundation::Collections::IObservableVector<winrt::hstring> _filteredActions{ nullptr };
        winrt::hstring _lastValidAction;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(EditAction);
}
