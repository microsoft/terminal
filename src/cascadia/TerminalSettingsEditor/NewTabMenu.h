// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "NewTabMenu.g.h"
#include "NewTabMenuEntryTemplateSelector.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NewTabMenu : public HasScrollViewer<NewTabMenu>, NewTabMenuT<NewTabMenu>
    {
    public:
        NewTabMenu();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        void FolderNameTextBox_TextChanged(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Controls::TextChangedEventArgs& e);

        void TreeView_DragItemsStarting(const winrt::Microsoft::UI::Xaml::Controls::TreeView& sender, const winrt::Microsoft::UI::Xaml::Controls::TreeViewDragItemsStartingEventArgs& e);
        void TreeView_DragOver(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);
        void TreeView_Drop(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);
        void TreeView_DragItemsCompleted(const winrt::Microsoft::UI::Xaml::Controls::TreeView& sender, const winrt::Microsoft::UI::Xaml::Controls::TreeViewDragItemsCompletedEventArgs& e);

        void TreeViewItem_DragOver(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);
        void TreeViewItem_Drop(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::DragEventArgs& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(Editor::NewTabMenuViewModel, ViewModel, _PropertyChangedHandlers, nullptr);

    private:
        Editor::NewTabMenuEntryTemplateSelector _entryTemplateSelector{ nullptr };

        Editor::NewTabMenuEntryViewModel _draggedEntry{ nullptr };
    };

    struct NewTabMenuEntryTemplateSelector : public NewTabMenuEntryTemplateSelectorT<NewTabMenuEntryTemplateSelector>
    {
    public:
        NewTabMenuEntryTemplateSelector() = default;

        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const Windows::Foundation::IInspectable& item, const Windows::UI::Xaml::DependencyObject& container);
        Windows::UI::Xaml::DataTemplate SelectTemplateCore(const Windows::Foundation::IInspectable& item);

        WINRT_PROPERTY(Windows::UI::Xaml::DataTemplate, ProfileEntryTemplate, nullptr);
        WINRT_PROPERTY(Windows::UI::Xaml::DataTemplate, SeparatorEntryTemplate, nullptr);
        WINRT_PROPERTY(Windows::UI::Xaml::DataTemplate, FolderEntryTemplate, nullptr);
        WINRT_PROPERTY(Windows::UI::Xaml::DataTemplate, MatchProfilesEntryTemplate, nullptr);
        WINRT_PROPERTY(Windows::UI::Xaml::DataTemplate, RemainingProfilesEntryTemplate, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(NewTabMenu);
    BASIC_FACTORY(NewTabMenuEntryTemplateSelector);
}
