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
