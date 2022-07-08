// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "RenderingViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct RenderingViewModel : RenderingViewModelT<RenderingViewModel>, ViewModelHelper<RenderingViewModel>
    {
    public:
        RenderingViewModel(Model::GlobalAppSettings globalSettings);

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, ForceFullRepaintRendering);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, SoftwareRendering);

    private:
        Model::GlobalAppSettings _GlobalSettings;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(RenderingViewModel);
}
