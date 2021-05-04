// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Font.g.h"
#include "Appearances.g.h"
#include "AppearancePageNavigationState.g.h"
#include "AppearanceViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct AppearanceViewModel : AppearanceViewModelT<AppearanceViewModel>, ViewModelHelper<AppearanceViewModel>
    {
    public:
        AppearanceViewModel(const Model::AppearanceConfig& appearance);
        bool CanDeleteAppearance() const;

        OBSERVABLE_PROJECTED_SETTING(_appearance, ColorSchemeName);

    private:
        Model::AppearanceConfig _appearance;
    };

    struct AppearancePageNavigationState : AppearancePageNavigationStateT<AppearancePageNavigationState>
    {
    public:
        AppearancePageNavigationState(const Editor::AppearanceViewModel& viewModel,
                                      const Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme>& schemes,
                                      const Editor::AppearancePageNavigationState& lastState,
                                      const IHostedInWindow& windowRoot) :
            _Appearance{ viewModel },
            _Schemes{ schemes },
            _WindowRoot{ windowRoot }
        {
            if (lastState)
            {
            }
        }

        WINRT_PROPERTY(IHostedInWindow, WindowRoot, nullptr);
        WINRT_PROPERTY(Editor::AppearanceViewModel, Appearance, nullptr);

    private:
        Windows::Foundation::Collections::IMapView<hstring, Model::ColorScheme> _Schemes;
    };

    struct Appearances : AppearancesT<Appearances>
    {
    public:
        Appearances();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_PROPERTY(Editor::AppearancePageNavigationState, State, nullptr);

    private:

    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Appearances);
}
