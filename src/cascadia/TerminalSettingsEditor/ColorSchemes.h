// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorTableEntry.g.h"
#include "ColorSchemes.g.h"
#include "NavigateToColorSchemesArgs.g.h"
#include "ColorSchemeViewModel.h"
#include "ColorSchemesPageViewModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NavigateToColorSchemesArgs : NavigateToColorSchemesArgsT<NavigateToColorSchemesArgs>
    {
        NavigateToColorSchemesArgs(const Editor::ColorSchemesPageViewModel& vm, const hstring& elementToFocus = {}) :
            _ViewModel(vm),
            _ElementToFocus(elementToFocus) {}

        Editor::ColorSchemesPageViewModel ViewModel() const noexcept { return _ViewModel; }
        hstring ElementToFocus() const noexcept { return _ElementToFocus; }

    private:
        Editor::ColorSchemesPageViewModel _ViewModel{ nullptr };
        hstring _ElementToFocus{};
    };

    struct ColorSchemes : public HasScrollViewer<ColorSchemes>, ColorSchemesT<ColorSchemes>
    {
    public:
        ColorSchemes();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void AddNew_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void ListView_PreviewKeyDown(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);

        til::property_changed_event PropertyChanged;

        WINRT_PROPERTY(Model::ColorScheme, CurrentColorScheme, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorSchemesPageViewModel, ViewModel, PropertyChanged.raise, nullptr);

    private:
        winrt::Windows::UI::Xaml::FrameworkElement::LayoutUpdated_revoker _layoutUpdatedRevoker;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemes);
}
