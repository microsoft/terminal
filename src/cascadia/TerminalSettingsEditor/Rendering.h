// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Rendering.g.h"
#include "ObjectModel/GlobalSettingsModel.h"

namespace winrt::SettingsControl::implementation
{
    struct Rendering : RenderingT<Rendering>
    {
        Rendering();

        ObjectModel::GlobalSettingsModel GlobalSettingsModel();

    private:
        ObjectModel::GlobalSettingsModel m_globalSettingsModel{ nullptr };
    };
}

namespace winrt::SettingsControl::factory_implementation
{
    BASIC_FACTORY(Rendering);
}
