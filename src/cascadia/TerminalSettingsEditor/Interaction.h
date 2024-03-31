// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Interaction.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Interaction : public HasScrollViewer<Interaction>, InteractionT<Interaction>
    {
        Interaction();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::InteractionViewModel, ViewModel, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Interaction);
}
