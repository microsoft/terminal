// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemes.h"
#include "ColorTableEntry.g.cpp"
#include "ColorSchemes.g.cpp"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ColorSchemes::ColorSchemes()
    {
        InitializeComponent();

        Automation::AutomationProperties::SetName(AddNewButton(), RS_(L"ColorScheme_AddNewButton/Text"));
        Automation::AutomationProperties::SetName(DeleteButton(), RS_(L"ColorScheme_DeleteButton/Text"));
    }

    void ColorSchemes::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::ColorSchemesPageViewModel>();
        _ViewModel.CurrentPage(ColorSchemesSubPage::Base);
    }

    void ColorSchemes::DeleteConfirmation_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        _ViewModel.RequestDeleteCurrentScheme();
        DeleteButton().Flyout().Hide();

        // GH#11971, part 2. If we delete a scheme, and the next scheme we've
        // loaded is an inbox one that _can't_ be deleted, then we need to toss
        // focus to something sensible, rather than letting it fall out to the
        // tab item.
        //
        // When deleting a scheme and the next scheme _is_ deletable, this isn't
        // an issue, we'll already correctly focus the Delete button.
        //
        // However, it seems even more useful for focus to ALWAYS land on the
        // scheme list view. This forces Narrator to read the name of the
        // newly selected color scheme, which seemed more useful.

        // For some reason, if we just call ColorSchemeListView().Focus(FocusState::Programmatic),
        // focus always lands on the _first_ item of the list view, regardless of what the currently
        // selected item is. So we need to grab the item container and focus that.
        const auto itemContainer = ColorSchemeListView().ContainerFromIndex(ColorSchemeListView().SelectedIndex());
        itemContainer.as<ContentControl>().Focus(FocusState::Programmatic);
    }

    void ColorSchemes::AddNew_Click(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::RoutedEventArgs& /*e*/)
    {
        if (const auto newSchemeVM{ _ViewModel.RequestAddNew() })
        {
            ColorSchemeListView().SelectedItem(newSchemeVM);
            ColorSchemeListView().ScrollIntoView(newSchemeVM);
        }
    }

    void ColorSchemes::ListView_PreviewKeyDown(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            // Treat this as if 'edit' was clicked
            _ViewModel.Edit_Click(nullptr, nullptr);
            e.Handled(true);
        }
        else if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Delete)
        {
            // Treat this as if 'delete' was clicked
            DeleteConfirmation_Click(nullptr, nullptr);
            e.Handled(true);
        }
    }
}
