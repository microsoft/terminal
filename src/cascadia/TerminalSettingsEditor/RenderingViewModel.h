// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "RenderingViewModel.g.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct RenderingViewModel : RenderingViewModelT<RenderingViewModel>, ViewModelHelper<RenderingViewModel>
    {
        explicit RenderingViewModel(Model::CascadiaSettings settings) noexcept;

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_settings.ProfileDefaults(), UseAtlasEngine);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_settings.GlobalSettings(), ForceFullRepaintRendering);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_settings.GlobalSettings(), SoftwareRendering);

    private:
        Model::CascadiaSettings _settings{ nullptr };
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(RenderingViewModel);
}
