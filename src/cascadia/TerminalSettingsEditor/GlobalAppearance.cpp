// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"
#include <ObjectModel\GlobalSettings.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalAppearance::GlobalAppearance()
    {
        m_globalSettingsModel = winrt::make<Model::implementation::GlobalSettingsModel>();
        InitializeComponent();
    }

    Model::GlobalSettingsModel GlobalAppearance::GlobalSettingsModel()
    {
        return m_globalSettingsModel;
    }
}
