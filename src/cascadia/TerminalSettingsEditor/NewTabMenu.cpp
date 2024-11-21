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

        // Ideally, we'd bind IsEnabled to something like mtu:Converters.isEmpty(NewTabMenuListView.SelectedItems.Size) in the XAML,
        //   but the XAML compiler can't find NewTabMenuListView when we try that. Rather than copying the list of selected items over
        //   to the view model, we'll just do this instead (much simpler).
        NewTabMenuListView().SelectionChanged([this](auto&&, auto&&) {
            const auto list = NewTabMenuListView();
            MoveToFolderButton().IsEnabled(list.SelectedItems().Size() > 0);
            DeleteMultipleButton().IsEnabled(list.SelectedItems().Size() > 0);
        });

        Automation::AutomationProperties::SetName(MoveToFolderButton(), RS_(L"NewTabMenu_MoveToFolderTextBlock/Text"));
        Automation::AutomationProperties::SetName(DeleteMultipleButton(), RS_(L"NewTabMenu_DeleteMultipleTextBlock/Text"));
        Automation::AutomationProperties::SetName(AddProfileComboBox(), RS_(L"NewTabMenu_AddProfile/Header"));
        Automation::AutomationProperties::SetName(AddProfileButton(), RS_(L"NewTabMenu_AddProfileButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetName(AddSeparatorButton(), RS_(L"NewTabMenu_AddSeparatorButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetName(AddFolderButton(), RS_(L"NewTabMenu_AddFolderButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        Automation::AutomationProperties::SetName(AddMatchProfilesButton(), RS_(L"NewTabMenu_AddMatchProfilesTextBlock/Text"));
        Automation::AutomationProperties::SetName(AddRemainingProfilesButton(), RS_(L"NewTabMenu_AddRemainingProfilesButton/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
    }

    void NewTabMenu::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::NewTabMenuViewModel>();
    }

    void NewTabMenu::FolderPickerDialog_Opened(const IInspectable& /*sender*/, const Controls::ContentDialogOpenedEventArgs& /*e*/)
    {
        // Ideally, we'd bind IsPrimaryButtonEnabled to something like mtu:Converters.isEmpty(FolderTree.SelectedItems.Size) in the XAML.
        // Similar to above, the XAML compiler can't find FolderTree when we try that.
        // To make matters worse, SelectionChanged doesn't exist for WinUI 2's TreeView.
        // Let's just select the first item and call it a day.
        _ViewModel.GenerateFolderTree();
        _ViewModel.CurrentFolderTreeViewSelectedItem(_ViewModel.FolderTree().First().Current());
    }

    void NewTabMenu::FolderPickerDialog_PrimaryButtonClick(const IInspectable& /*sender*/, const Controls::ContentDialogButtonClickEventArgs& /*e*/)
    {
        // copy selected items first (it updates as we move entries)
        std::vector<Editor::NewTabMenuEntryViewModel> entries;
        for (const auto& item : NewTabMenuListView().SelectedItems())
        {
            entries.push_back(item.as<Editor::NewTabMenuEntryViewModel>());
        }

        // now actually move them
        _ViewModel.RequestMoveEntriesToFolder(single_threaded_vector<Editor::NewTabMenuEntryViewModel>(std::move(entries)), FolderTreeView().SelectedItem().as<Editor::FolderTreeViewEntry>().FolderEntryVM());
    }

    void NewTabMenu::EditEntry_Clicked(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        const auto folderVM = sender.as<FrameworkElement>().DataContext().as<Editor::FolderEntryViewModel>();
        _ViewModel.CurrentFolder(folderVM);
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

    safe_void_coroutine NewTabMenu::MoveMultiple_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        co_await FindName(L"FolderPickerDialog").as<Controls::ContentDialog>().ShowAsync();
    }

    void NewTabMenu::DeleteMultiple_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        // copy selected items first (it updates as we delete entries)
        std::vector<Editor::NewTabMenuEntryViewModel> entries;
        for (const auto& item : NewTabMenuListView().SelectedItems())
        {
            entries.push_back(item.as<Editor::NewTabMenuEntryViewModel>());
        }

        // now actually delete them
        for (const auto& vm : entries)
        {
            _ViewModel.RequestDeleteEntry(vm);
        }
    }

    void NewTabMenu::AddProfileButton_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ScrollToEntry(_ViewModel.RequestAddSelectedProfileEntry());
    }

    void NewTabMenu::AddSeparatorButton_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ScrollToEntry(_ViewModel.RequestAddSeparatorEntry());
    }

    void NewTabMenu::AddFolderButton_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ScrollToEntry(_ViewModel.RequestAddFolderEntry());
    }

    void NewTabMenu::AddMatchProfilesButton_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ScrollToEntry(_ViewModel.RequestAddProfileMatcherEntry());
    }

    void NewTabMenu::AddRemainingProfilesButton_Clicked(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ScrollToEntry(_ViewModel.RequestAddRemainingProfilesEntry());
    }

    // As a QOL improvement, we scroll to the newly added entry.
    // Calling ScrollIntoView() on its own causes the items to briefly disappear.
    // Calling UpdateLayout() beforehand remedies this issue.
    void NewTabMenu::_ScrollToEntry(const Editor::NewTabMenuEntryViewModel& entry)
    {
        const auto& listView = NewTabMenuListView();
        listView.UpdateLayout();
        listView.ScrollIntoView(entry);
    }

    void NewTabMenu::AddFolderNameTextBox_KeyDown(const IInspectable& /*sender*/, const Input::KeyRoutedEventArgs& e)
    {
        if (e.Key() == Windows::System::VirtualKey::Enter)
        {
            // We need to manually set the FolderName here because the TextBox's TextChanged event hasn't fired yet.
            if (const auto folderName = FolderNameTextBox().Text(); !folderName.empty())
            {
                _ViewModel.AddFolderName(folderName);
                const auto entry = _ViewModel.RequestAddFolderEntry();
                NewTabMenuListView().ScrollIntoView(entry);
            }
        }
    }

    void NewTabMenu::AddFolderNameTextBox_TextChanged(const IInspectable& sender, const Controls::TextChangedEventArgs& /*e*/)
    {
        const auto isTextEmpty = sender.as<Controls::TextBox>().Text().empty();
        AddFolderButton().IsEnabled(!isTextEmpty);
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
            case Model::NewTabMenuEntryType::Action:
                return ActionEntryTemplate();
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
