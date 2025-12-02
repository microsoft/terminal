// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SnippetsPaneContent.h"
#include "SnippetsPaneContent.g.cpp"
#include "FilteredTask.g.cpp"
#include "Utils.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
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
    SnippetsPaneContent::SnippetsPaneContent() :
        _allTasks{ winrt::single_threaded_observable_vector<TerminalApp::FilteredTask>() }
    {
        InitializeComponent();

        WUX::Automation::AutomationProperties::SetName(*this, RS_(L"SnippetPaneTitle/Text"));
    }

    void SnippetsPaneContent::_updateFilteredCommands()
    {
        const auto& queryString = _filterBox().Text();
        auto pattern = std::make_shared<fzf::matcher::Pattern>(fzf::matcher::ParsePattern(queryString));

        // DON'T replace the itemSource here. If you do, it'll un-expand all the
        // nested items the user has expanded. Instead, just update the filter.
        // That'll also trigger a PropertyChanged for the Visibility property.
        for (const auto& t : _allTasks)
        {
            auto impl = winrt::get_self<implementation::FilteredTask>(t);
            impl->UpdateFilter(pattern);
        }
    }

    safe_void_coroutine SnippetsPaneContent::UpdateSettings(const CascadiaSettings& settings)
    {
        _settings = settings;

        // You'd think that `FilterToSendInput(queryString)` would work. It
        // doesn't! That uses the queryString as the current command the user
        // has typed, then relies on the suggestions UI to _also_ filter with that
        // string.

        const auto tasks = co_await _settings.GlobalSettings().ActionMap().FilterToSnippets(winrt::hstring{}, winrt::hstring{}); // IVector<Model::Command>
        co_await wil::resume_foreground(Dispatcher());

        _allTasks.Clear();
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
        return { 200, 200 };
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

    winrt::WUX::Media::Brush SnippetsPaneContent::BackgroundBrush()
    {
        static const auto key = winrt::box_value(L"SettingsUiTabBrush");
        return ThemeLookup(WUX::Application::Current().Resources(),
                           _settings.GlobalSettings().CurrentTheme().RequestedTheme(),
                           key)
            .try_as<winrt::WUX::Media::Brush>();
    }

    void SnippetsPaneContent::SetLastActiveControl(const Microsoft::Terminal::Control::TermControl& control)
    {
        _control = control;
    }

    void SnippetsPaneContent::_runCommand(const Microsoft::Terminal::Settings::Model::Command& command)
    {
        if (const auto& strongControl{ _control.get() })
        {
            // By using the last active control as the sender here, the
            // action dispatch will send this to the active control,
            // thinking that it is the control that requested this event.
            strongControl.Focus(winrt::WUX::FocusState::Programmatic);
            DispatchCommandRequested.raise(strongControl, command);
        }
    }

    void SnippetsPaneContent::_runCommandButtonClicked(const Windows::Foundation::IInspectable& sender,
                                                       const Windows::UI::Xaml::RoutedEventArgs&)
    {
        if (const auto& taskVM{ sender.try_as<WUX::Controls::Button>().DataContext().try_as<FilteredTask>() })
        {
            _runCommand(taskVM->Command());
        }
    }
    void SnippetsPaneContent::_closePaneClick(const Windows::Foundation::IInspectable& /*sender*/,
                                              const Windows::UI::Xaml::RoutedEventArgs&)
    {
        Close();
    }

    // Called when one of the items in the list is tapped, or enter/space is
    // pressed on it while focused. Notably, this isn't the Tapped event - it
    // isn't called when the user clicks the dropdown arrow (that does usually
    // also trigger a Tapped).
    //
    // We'll use this to toggle the expanded state of nested items, since the
    // tree view arrow is so little
    void SnippetsPaneContent::_treeItemInvokedHandler(const IInspectable& /*sender*/,
                                                      const MUX::Controls::TreeViewItemInvokedEventArgs& e)
    {
        // The InvokedItem here is the item in the data collection that was
        // bound itself.
        if (const auto& taskVM{ e.InvokedItem().try_as<FilteredTask>() })
        {
            if (taskVM->HasChildren())
            {
                // We then need to find the actual TreeViewItem for that
                // FilteredTask.
                if (const auto& item{ _treeView().ContainerFromItem(*taskVM).try_as<MUX::Controls::TreeViewItem>() })
                {
                    item.IsExpanded(!item.IsExpanded());
                }
            }
        }
    }

    // Raised on individual TreeViewItems. We'll use this event to send the
    // input on an Enter/Space keypress, when a leaf item is selected.
    void SnippetsPaneContent::_treeItemKeyUpHandler(const IInspectable& sender,
                                                    const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto& item{ sender.try_as<MUX::Controls::TreeViewItem>() };
        if (!item)
        {
            return;
        }
        const auto& taskVM{ item.DataContext().try_as<FilteredTask>() };
        if (!taskVM || taskVM->HasChildren())
        {
            return;
        }

        const auto& key = e.OriginalKey();
        if (key == VirtualKey::Enter || key == VirtualKey::Space)
        {
            if (const auto& button = e.OriginalSource().try_as<WUX::Controls::Button>())
            {
                // Let the button handle the Enter key so an eventually attached click handler will be called
                e.Handled(false);
                return;
            }

            _runCommand(taskVM->Command());
            e.Handled(true);
        }
    }

}
