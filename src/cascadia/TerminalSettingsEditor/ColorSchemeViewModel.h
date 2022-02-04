// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorSchemeViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "ColorSchemes.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemeViewModel : ColorSchemeViewModelT<ColorSchemeViewModel>, ViewModelHelper<ColorSchemeViewModel>
    {
    public:
        ColorSchemeViewModel(Model::ColorScheme scheme);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, CurrentNonBrightColorTable, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, CurrentBrightColorTable, nullptr);

        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentForegroundColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentBackgroundColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentCursorColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentSelectionBackgroundColor, _PropertyChangedHandlers, nullptr);
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemeViewModel);
}
