// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorSchemesPageViewModel.g.h"
#include "ColorSchemeViewModel.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "ColorSchemes.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemesPageViewModel : ColorSchemesPageViewModelT<ColorSchemesPageViewModel>, ViewModelHelper<ColorSchemesPageViewModel>
    {
    public:
        ColorSchemesPageViewModel(const Model::CascadiaSettings& settings);
        void UpdateSettings(const Model::CascadiaSettings& settings);

        void RequestSetCurrentScheme(Editor::ColorSchemeViewModel scheme);
        void RequestEnterRename();
        bool RequestExitRename(bool saveChanges, winrt::hstring newName);
        void RequestDeleteCurrentScheme();
        Editor::ColorSchemeViewModel RequestAddNew();

        bool CanDeleteCurrentScheme() const;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_OBSERVABLE_PROPERTY(bool, InRenameMode, _PropertyChangedHandlers, false);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorSchemeViewModel, CurrentScheme, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel>, AllColorSchemes, _PropertyChangedHandlers, nullptr);

    private:
        Model::CascadiaSettings _settings;
        Windows::Foundation::Collections::IMap<Editor::ColorSchemeViewModel, Model::ColorScheme> _viewModelToSchemeMap;

        void _MakeColorSchemeVMsHelper();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemesPageViewModel);
}
