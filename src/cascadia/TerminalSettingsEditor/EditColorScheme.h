// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "NavigateToEditColorSchemeArgs.g.h"
#include "EditColorScheme.g.h"
#include "ColorSchemeViewModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    // TODO CARLOS: We have a bunch of NavigateToXArgs that have the same pattern.
    // Leonard suggested replacing ViewModel with IInspectable so that we can just reuse this for everything.
    struct NavigateToEditColorSchemeArgs : NavigateToEditColorSchemeArgsT<NavigateToEditColorSchemeArgs>
    {
        NavigateToEditColorSchemeArgs(const Editor::ColorSchemeViewModel& vm, const hstring& elementToFocus = {}) :
            _ViewModel(vm),
            _ElementToFocus(elementToFocus) {}

        Editor::ColorSchemeViewModel ViewModel() const noexcept { return _ViewModel; }
        hstring ElementToFocus() const noexcept { return _ElementToFocus; }

    private:
        Editor::ColorSchemeViewModel _ViewModel{ nullptr };
        hstring _ElementToFocus{};
    };

    struct EditColorScheme : public HasScrollViewer<EditColorScheme>, EditColorSchemeT<EditColorScheme>
    {
        EditColorScheme();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void ColorPickerChanged(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Microsoft::UI::Xaml::Controls::ColorChangedEventArgs& args);
        void RenameAccept_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void RenameCancel_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void NameBox_PreviewKeyDown(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);

        til::property_changed_event PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(Editor::ColorSchemeViewModel, ViewModel, PropertyChanged.raise, nullptr);

    private:
        void _RenameCurrentScheme(hstring newName);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(EditColorScheme);
}
