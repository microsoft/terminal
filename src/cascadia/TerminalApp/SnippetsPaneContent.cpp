// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SnippetsPaneContent.h"
#include "SnippetsPaneContent.g.cpp"
#include "FilteredTask.g.cpp"

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

        // auto res = Windows::UI::Xaml::Application::Current().Resources();
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
        reason;
        // _box.Focus(reason);
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
                DispatchCommandRequested.raise(strongControl, taskVM->Command());
            }
        }
    }

}
