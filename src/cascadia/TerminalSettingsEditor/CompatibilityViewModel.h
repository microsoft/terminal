// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "CompatibilityViewModel.g.h"
#include "ViewModelHelpers.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct CompatibilityViewModel : CompatibilityViewModelT<CompatibilityViewModel>, ViewModelHelper<CompatibilityViewModel>
    {
        explicit CompatibilityViewModel(Model::CascadiaSettings settings) noexcept;

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_settings.ProfileDefaults(), UseAtlasEngine);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_settings.GlobalSettings(), ForceFullRepaintRendering);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_settings.GlobalSettings(), SoftwareRendering);

    private:
        Model::CascadiaSettings _settings{ nullptr };
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(CompatibilityViewModel);
}
