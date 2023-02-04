// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EditColorScheme.g.h"
#include "ColorSchemeViewModel.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct EditColorScheme : public HasScrollViewer<EditColorScheme>, EditColorSchemeT<EditColorScheme>
    {
        EditColorScheme();

        void OnNavigatedTo(const WUX::Navigation::NavigationEventArgs& e);

        void ColorPickerChanged(const WF::IInspectable& sender, const MUXC::ColorChangedEventArgs& args);
        void RenameAccept_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void RenameCancel_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void NameBox_PreviewKeyDown(const WF::IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);

        WINRT_OBSERVABLE_PROPERTY(Editor::ColorSchemeViewModel, ViewModel, _PropertyChangedHandlers, nullptr);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);

    private:
        void _RenameCurrentScheme(hstring newName);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(EditColorScheme);
}
