// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorTableEntry.g.h"
#include "ColorSchemes.g.h"
#include "ColorSchemeViewModel.h"
#include "ColorSchemesPageViewModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemes : public HasScrollViewer<ColorSchemes>, ColorSchemesT<ColorSchemes>
    {
    public:
        ColorSchemes();

        void OnNavigatedTo(const WUX::Navigation::NavigationEventArgs& e);

        void AddNew_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void ListView_PreviewKeyDown(const WF::IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);
        void ListView_SelectionChanged(const WF::IInspectable& sender, const WUXC::SelectionChangedEventArgs& e);

        WINRT_PROPERTY(MTSM::ColorScheme, CurrentColorScheme, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorSchemesPageViewModel, ViewModel, _PropertyChangedHandlers, nullptr);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);

    private:
        WUX::FrameworkElement::LayoutUpdated_revoker _layoutUpdatedRevoker;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemes);
}
