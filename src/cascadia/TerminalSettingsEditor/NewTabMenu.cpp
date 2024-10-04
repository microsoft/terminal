// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "NewTabMenu.h"
#include "NewTabMenu.g.cpp"
#include "NewTabMenuEntryTemplateSelector.g.cpp"
#include "EnumEntry.h"

#include "NewTabMenuViewModel.h"

#include <LibraryResources.h>

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    NewTabMenu::NewTabMenu()
    {
        InitializeComponent();

        _entryTemplateSelector = Resources().Lookup(box_value(L"NewTabMenuEntryTemplateSelector")).as<Editor::NewTabMenuEntryTemplateSelector>();
    }

    void NewTabMenu::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::NewTabMenuViewModel>();
    }

    void NewTabMenu::FolderNameTextBox_TextChanged(const IInspectable& sender, const Controls::TextChangedEventArgs& /*e*/)
    {
        const auto isTextEmpty = sender.as<Controls::TextBox>().Text().empty();
        AddFolderButton().IsEnabled(!isTextEmpty);
    }

    void NewTabMenu::ReorderEntry_Clicked(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        const auto btn = sender.as<Controls::Button>();
        const auto entry = btn.DataContext().as<Editor::NewTabMenuEntryViewModel>();
        const auto direction = unbox_value<hstring>(btn.Tag());

        _ViewModel.RequestReorderEntry(entry, direction == L"Up");
    }

    void NewTabMenu::DeleteEntry_Clicked(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        const auto entry = sender.as<Controls::Button>().DataContext().as<Editor::NewTabMenuEntryViewModel>();
        _ViewModel.RequestDeleteEntry(entry);
    }

    void NewTabMenu::EditEntry_Clicked(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        auto entryVM = sender.as<FrameworkElement>().DataContext().as<Editor::FolderEntryViewModel>();
        _ViewModel.CurrentFolderEntry(entryVM);
        _ViewModel.CurrentPage(NTMSubPage::Folder);
    }

    DataTemplate NewTabMenuEntryTemplateSelector::SelectTemplateCore(const IInspectable& item, const DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    DataTemplate NewTabMenuEntryTemplateSelector::SelectTemplateCore(const IInspectable& item)
    {
        if (const auto entryVM = item.try_as<Editor::NewTabMenuEntryViewModel>())
        {
            switch (entryVM.Type())
            {
            case Model::NewTabMenuEntryType::Profile:
                return ProfileEntryTemplate();
            case Model::NewTabMenuEntryType::Separator:
                return SeparatorEntryTemplate();
            case Model::NewTabMenuEntryType::Folder:
                return FolderEntryTemplate();
            case Model::NewTabMenuEntryType::MatchProfiles:
                return MatchProfilesEntryTemplate();
            case Model::NewTabMenuEntryType::RemainingProfiles:
                return RemainingProfilesEntryTemplate();
            case Model::NewTabMenuEntryType::Invalid:
            default:
                assert(false);
                return nullptr;
            }
        }
        assert(false);
        return nullptr;
    }
}
