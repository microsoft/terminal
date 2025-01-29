// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "EditAction.h"
#include "EditAction.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/AllShortcutActions.h"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    EditAction::EditAction()
    {
        InitializeComponent();
        _itemTemplateSelector = Resources().Lookup(winrt::box_value(L"ArgsTemplateSelector")).try_as<ArgsTemplateSelectors>();
        _listItemTemplate = Resources().Lookup(winrt::box_value(L"ListItemTemplate")).try_as<DataTemplate>();
    }

    void EditAction::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::CommandViewModel>();
    }

    void EditAction::_choosingItemContainer(
        const Windows::UI::Xaml::Controls::ListViewBase& /*sender*/,
        const Windows::UI::Xaml::Controls::ChoosingItemContainerEventArgs& args)
    {
        const auto dataTemplate = _itemTemplateSelector.SelectTemplate(args.Item());
        const auto itemContainer = args.ItemContainer();

        // todo: do we need the same caching logic as in command palette?
        if (!itemContainer || itemContainer.ContentTemplate() != dataTemplate)
        {
            ElementFactoryGetArgs factoryArgs{};
            const auto listViewItem = _listItemTemplate.GetElement(factoryArgs).try_as<Controls::ListViewItem>();
            listViewItem.ContentTemplate(dataTemplate);

            args.ItemContainer(listViewItem);
        }

        args.IsContainerPrepared(true);
    }
}
