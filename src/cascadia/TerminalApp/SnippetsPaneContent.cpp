// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SnippetsPaneContent.h"
#include "SnippetsPaneContent.g.cpp"
#include "FilteredTask.g.cpp"
#include "SnippetsItemTemplateSelector.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt
{
    namespace WUX = Windows::UI::Xaml;
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    SnippetsPaneContent::SnippetsPaneContent()
    {
        InitializeComponent();

        // _itemTemplateSelector = Resources().Lookup(winrt::box_value(L"SnippetsItemTemplateSelector")).try_as<SnippetsItemTemplateSelector>();

        auto bg = Resources().Lookup(winrt::box_value(L"PageBackground"));
        Background(bg.try_as<WUX::Media::Brush>());
    }

    void SnippetsPaneContent::_updateFilteredCommands()
    {
        const auto& queryString = _filterBox().Text();

        // DON'T replace the itemSource here. If you do, it'll un-expand all the
        // nested items the user has expanded. Instead, just update the filter.
        // That'll also trigger a PropertyChanged for the Visibility property.
        for (const auto& t : _allTasks)
        {
            t.UpdateFilter(queryString);
        }
    }

    void SnippetsPaneContent::UpdateSettings(const CascadiaSettings& settings)
    {
        _settings = settings;

        // You'd think that `FilterToSendInput(queryString)` would work. It
        // doesn't! That uses the queryString as the current command the user
        // has typed, then relies on the suggestions UI to _also_ filter with that
        // string.

        const auto tasks = _settings.GlobalSettings().ActionMap().FilterToSendInput(L""); // IVector<Model::Command>
        _allTasks = winrt::single_threaded_observable_vector<TerminalApp::FilteredTask>();
        for (const auto& t : tasks)
        {
            const auto& filtered{ winrt::make<FilteredTask>(t) };
            _allTasks.Append(filtered);
        }
        _treeView().ItemsSource(_allTasks);

        _updateFilteredCommands();

        PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"HasSnippets" });
    }

    bool SnippetsPaneContent::HasSnippets() const
    {
        return _allTasks.Size() != 0;
    }

    void SnippetsPaneContent::_filterTextChanged(const IInspectable& /*sender*/,
                                                 const Windows::UI::Xaml::RoutedEventArgs& /*args*/)
    {
        _updateFilteredCommands();
    }

    winrt::Windows::UI::Xaml::FrameworkElement SnippetsPaneContent::GetRoot()
    {
        return *this;
    }
    winrt::Windows::Foundation::Size SnippetsPaneContent::MinimumSize()
    {
        return { 1, 1 };
    }
    void SnippetsPaneContent::Focus(winrt::Windows::UI::Xaml::FocusState reason)
    {
        _filterBox().Focus(reason);
    }
    void SnippetsPaneContent::Close()
    {
        CloseRequested.raise(*this, nullptr);
    }

    INewContentArgs SnippetsPaneContent::GetNewTerminalArgs(BuildStartupKind /*kind*/) const
    {
        return BaseContentArgs(L"snippets");
    }

    winrt::hstring SnippetsPaneContent::Icon() const
    {
        static constexpr std::wstring_view glyph{ L"\xe70b" }; // QuickNote
        return winrt::hstring{ glyph };
    }

    winrt::Windows::UI::Xaml::Media::Brush SnippetsPaneContent::BackgroundBrush()
    {
        return Background();
    }

    void SnippetsPaneContent::SetLastActiveControl(const Microsoft::Terminal::Control::TermControl& control)
    {
        _control = control;
    }

    void SnippetsPaneContent::_runCommandButtonClicked(const Windows::Foundation::IInspectable& sender,
                                                       const Windows::UI::Xaml::RoutedEventArgs&)
    {
        if (const auto& taskVM{ sender.try_as<WUX::Controls::Button>().DataContext().try_as<FilteredTask>() })
        {
            if (const auto& strongControl{ _control.get() })
            {
                // By using the last active control as the sender here, the
                // action dispatch will send this to the active control,
                // thinking that it is the control that requested this event.
                strongControl.Focus(winrt::WUX::FocusState::Programmatic);
                DispatchCommandRequested.raise(strongControl, taskVM->Command());
            }
        }
    }

    // // Method Description:
    // // - This event is triggered when filteredActionView is looking for item container (ListViewItem)
    // // to use to present the filtered actions.
    // // GH#9288: unfortunately the default lookup seems to choose items with wrong data templates,
    // // e.g., using DataTemplate rendering actions for tab palette items.
    // // We handle this event by manually selecting an item from the cache.
    // // If no item is found we allocate a new one.
    // // Arguments:
    // // - args: the ChoosingItemContainerEventArgs allowing to get container candidate suggested by the
    // // system and replace it with another candidate if required.
    // // Return Value:
    // // - <none>
    // void SnippetsPaneContent::_choosingItemContainer(
    //     const Windows::UI::Xaml::Controls::ListViewBase& /*sender*/,
    //     const Windows::UI::Xaml::Controls::ChoosingItemContainerEventArgs& args)
    // {
    //     const auto dataTemplate = _itemTemplateSelector.SelectTemplate(args.Item());
    //     const auto itemContainer = args.ItemContainer();
    //     if (itemContainer && itemContainer.ContentTemplate() == dataTemplate)
    //     {
    //         // If the suggested candidate is OK simply remove it from the cache
    //         // (so we won't allocate it until it is released) and return
    //         _listViewItemsCache[dataTemplate].erase(itemContainer);
    //     }
    //     else
    //     {
    //         // We need another candidate, let's look it up inside the cache
    //         auto& containersByTemplate = _listViewItemsCache[dataTemplate];
    //         if (!containersByTemplate.empty())
    //         {
    //             // There cache contains available items for required DataTemplate
    //             // Let's return one of them (and remove it from the cache)
    //             auto firstItem = containersByTemplate.begin();
    //             args.ItemContainer(*firstItem);
    //             containersByTemplate.erase(firstItem);
    //         }
    //         else
    //         {
    //             ElementFactoryGetArgs factoryArgs{};
    //             const auto listViewItem = _listItemTemplate.GetElement(factoryArgs).try_as<Controls::ListViewItem>();
    //             listViewItem.ContentTemplate(dataTemplate);

    //             if (dataTemplate == _itemTemplateSelector.NestedItemTemplate())
    //             {
    //                 const auto helpText = winrt::box_value(RS_(L"CommandPalette_MoreOptions/[using:Windows.UI.Xaml.Automation]AutomationProperties/HelpText"));
    //                 listViewItem.SetValue(Automation::AutomationProperties::HelpTextProperty(), helpText);
    //             }

    //             args.ItemContainer(listViewItem);
    //         }
    //     }
    //     args.IsContainerPrepared(true);
    // }

    // // Method Description:
    // // - This event is triggered when the data item associate with filteredActionView list item is changing.
    // //   We check if the item is being recycled. In this case we return it to the cache
    // // Arguments:
    // // - args: the ContainerContentChangingEventArgs describing the container change
    // // Return Value:
    // // - <none>
    // void SnippetsPaneContent::_containerContentChanging(
    //     const Windows::UI::Xaml::Controls::ListViewBase& /*sender*/,
    //     const Windows::UI::Xaml::Controls::ContainerContentChangingEventArgs& args)
    // {
    //     const auto itemContainer = args.ItemContainer();
    //     if (args.InRecycleQueue() && itemContainer && itemContainer.ContentTemplate())
    //     {
    //         _listViewItemsCache[itemContainer.ContentTemplate()].insert(itemContainer);
    //         itemContainer.DataContext(nullptr);
    //     }
    //     else
    //     {
    //         itemContainer.DataContext(args.Item());
    //     }
    // }

#pragma region(SnippetsItemTemplateSelector)

    WUX::DataTemplate SnippetsItemTemplateSelector::SelectTemplateCore(const winrt::IInspectable& item, const winrt::WUX::DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    // Method Description:
    // - This method is called once command palette decides how to render a filtered command.
    //   Currently we support two ways to render command, that depend on its palette item type:
    //   - For TabPalette item we render an icon, a title, and some tab-related indicators like progress bar (as defined by TabItemTemplate)
    //   - All other items are currently rendered with icon, title and optional key-chord (as defined by GeneralItemTemplate)
    // Arguments:
    // - item - an instance of filtered command to render
    // Return Value:
    // - data template to use for rendering
    WUX::DataTemplate SnippetsItemTemplateSelector::SelectTemplateCore(const winrt::IInspectable& item)
    {
        if (const auto filteredTask{ item.try_as<winrt::TerminalApp::FilteredTask>() })
        {
            return filteredTask.HasChildren() ? NestedItemTemplate() : GeneralItemTemplate();
        }

        return GeneralItemTemplate();
    }
#pragma endregion

}
