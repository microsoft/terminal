// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Rendering.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Rendering : RenderingT<Rendering>
    {
        Rendering();

        void OnNavigatedTo(winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs e);

        GETSET_PROPERTY(Model::GlobalAppSettings, GlobalSettings, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Rendering);
}
