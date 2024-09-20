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

        // TODO CARLOS: set auto props, if necessary
        //Automation::AutomationProperties::SetName(NewTabMenuModeComboBox(), RS_(L"Globals_NewTabMenuModeSetting/Text"));
        //Automation::AutomationProperties::SetHelpText(NewTabMenuModeComboBox(), RS_(L"Globals_NewTabMenuModeSetting/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        //Automation::AutomationProperties::SetHelpText(PosXBox(), RS_(L"Globals_InitialPosXBox/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        //Automation::AutomationProperties::SetHelpText(PosYBox(), RS_(L"Globals_InitialPosYBox/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        //Automation::AutomationProperties::SetHelpText(UseDefaultNewTabMenuPositionCheckbox(), RS_(L"Globals_DefaultNewTabMenuPositionCheckbox/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
        //Automation::AutomationProperties::SetName(CenterOnNewTabMenuToggle(), RS_(L"Globals_CenterOnNewTabMenu/Text"));
        //Automation::AutomationProperties::SetHelpText(CenterOnNewTabMenuToggle(), RS_(L"Globals_CenterOnNewTabMenu/[using:Windows.UI.Xaml.Controls]ToolTipService/ToolTip"));
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

    void NewTabMenu::TreeView_DragItemsStarting(const winrt::Microsoft::UI::Xaml::Controls::TreeView& /*sender*/, const winrt::Microsoft::UI::Xaml::Controls::TreeViewDragItemsStartingEventArgs& e)
    {
        _draggedEntry = e.Items().GetAt(0).as<Editor::NewTabMenuEntryViewModel>();
    }

    void NewTabMenu::TreeView_DragOver(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::DragEventArgs& e)
    {
        e.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::Move);
        OutputDebugString(L"TreeView_DragOver\n");
    }

    void NewTabMenu::TreeView_Drop(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::DragEventArgs& /*e*/)
    {
        OutputDebugString(L"TreeView_Drop\n");
    }

    void NewTabMenu::TreeView_DragItemsCompleted(const winrt::Microsoft::UI::Xaml::Controls::TreeView& /*sender*/, const winrt::Microsoft::UI::Xaml::Controls::TreeViewDragItemsCompletedEventArgs& /*e*/)
    {
        _draggedEntry = nullptr;
    }

    void NewTabMenu::TreeViewItem_DragOver(const IInspectable& /*sender*/, const DragEventArgs& e)
    {
        e.AcceptedOperation(Windows::ApplicationModel::DataTransfer::DataPackageOperation::Move);
    }

    void NewTabMenu::TreeViewItem_Drop(const IInspectable& sender, const DragEventArgs& /*e*/)
    {
        auto element = sender.as<FrameworkElement>();
        auto entry = element.DataContext().as<Editor::NewTabMenuEntryViewModel>();
        if (entry.Type() == NewTabMenuEntryType::Folder)
        {
            // add to the current folder
            auto folderEntry = entry.as<Editor::FolderEntryViewModel>();
            folderEntry.Entries().Append(_draggedEntry);
        }
        else
        {
            // create a parent folder and add both entries to it
            // TODO CARLOS: localize
            auto folderEntry = winrt::make<FolderEntryViewModel>(FolderEntry{ L"New Folder" });
            folderEntry.Entries().Append(entry);
            folderEntry.Entries().Append(_draggedEntry);

            // TODO CARLOS: this is wrong, we should be placing the folder in the same place as before, but we're testing this stuff out
            _ViewModel.Entries().Append(folderEntry);
        }
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
