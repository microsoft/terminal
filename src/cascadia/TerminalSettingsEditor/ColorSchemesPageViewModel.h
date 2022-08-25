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

        void CurrentScheme(const Editor::ColorSchemeViewModel& newSelectedScheme);
        Editor::ColorSchemeViewModel CurrentScheme();

        void AddNew_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        void RequestEnterRename();
        bool RequestExitRename(bool saveChanges, winrt::hstring newName);
        void RequestDeleteCurrentScheme();

        bool CanDeleteCurrentScheme() const;

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<ColorSchemesPageViewModel>::PropertyChanged;

        WINRT_OBSERVABLE_PROPERTY(bool, InRenameMode, _propertyChangedHandlers, false);
        WINRT_OBSERVABLE_PROPERTY(Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel>, AllColorSchemes, _propertyChangedHandlers, nullptr);

    private:
        Editor::ColorSchemeViewModel _CurrentScheme{ nullptr };
        Model::CascadiaSettings _settings;
        Windows::Foundation::Collections::IMap<Editor::ColorSchemeViewModel, Model::ColorScheme> _viewModelToSchemeMap;

        void _MakeColorSchemeVMsHelper();
        Editor::ColorSchemeViewModel _AddNewScheme();
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemesPageViewModel);
}
