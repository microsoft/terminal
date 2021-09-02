// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Rendering.g.h"
#include "RenderingPageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct RenderingPageNavigationState : RenderingPageNavigationStateT<RenderingPageNavigationState>
    {
    public:
        RenderingPageNavigationState(const Model::GlobalAppSettings& settings) :
            _Globals{ settings } {}

        WINRT_PROPERTY(Model::GlobalAppSettings, Globals, nullptr)
    };

    struct Rendering : public HasScrollViewer<Rendering>, RenderingT<Rendering>
    {
        Rendering();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        WINRT_PROPERTY(Editor::RenderingPageNavigationState, State, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Rendering);
}
