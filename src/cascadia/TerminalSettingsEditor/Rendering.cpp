// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Rendering.h"
#include "Rendering.g.cpp"
#include <ObjectModel\GlobalSettings.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Rendering::Rendering()
    {
        m_globalSettingsModel = winrt::make<Model::implementation::GlobalSettingsModel>();
        InitializeComponent();
    }

    Model::GlobalSettingsModel Rendering::GlobalSettingsModel()
    {
        return m_globalSettingsModel;
    }
}
