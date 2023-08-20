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
    }

    void ColorSchemes::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::ColorSchemesPageViewModel>();
        _ViewModel.CurrentPage(ColorSchemesSubPage::Base);

        _layoutUpdatedRevoker = LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // Only let this succeed once.
            _layoutUpdatedRevoker.revoke();

            ColorSchemeListView().Focus(FocusState::Programmatic);
        });
    }

    void ColorSchemes::AddNew_Click(const IInspectable& /*sender*/, const RoutedEventArgs& /*e*/)
    {
        if (const auto newSchemeVM{ _ViewModel.RequestAddNew() })
        {
            ColorSchemeListView().SelectedItem(newSchemeVM);
            _ViewModel.RequestEditSelectedScheme();
        }
    }

    void ColorSchemes::ListView_PreviewKeyDown(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        if (e.OriginalKey() == winrt::Windows::System::VirtualKey::Enter)
        {
            // Treat this as if 'edit' was clicked
            _ViewModel.RequestEditSelectedScheme();
            e.Handled(true);
        }
    }
}
