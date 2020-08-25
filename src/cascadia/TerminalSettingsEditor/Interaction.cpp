// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Interaction.h"
#include "Interaction.g.cpp"
#include <ObjectModel\GlobalSettings.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Interaction::Interaction()
    {
        m_globalSettingsModel = winrt::make<Model::implementation::GlobalSettingsModel>();
        InitializeComponent();
    }

    Model::GlobalSettingsModel Interaction::GlobalSettingsModel()
    {
        return m_globalSettingsModel;
    }
}
