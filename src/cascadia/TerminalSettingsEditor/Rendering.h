// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Rendering.g.h"
#include "ObjectModel/GlobalSettingsModel.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Rendering : RenderingT<Rendering>
    {
        Rendering();

        Model::GlobalSettingsModel GlobalSettingsModel();

    private:
        Model::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Rendering);
}
