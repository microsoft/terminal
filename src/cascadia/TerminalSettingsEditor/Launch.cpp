// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Launch.h"
#include "Launch.g.cpp"
#include <ObjectModel\GlobalSettings.h>

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Launch::Launch()
    {
        m_globalSettingsModel = winrt::make<Model::implementation::GlobalSettingsModel>();
        InitializeComponent();
    }

    Model::GlobalSettingsModel Launch::GlobalSettingsModel()
    {
        return m_globalSettingsModel;
    }

}
