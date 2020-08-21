// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Rendering.h"
#include "Rendering.g.cpp"
#include <ObjectModel\GlobalSettings.h>

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    Rendering::Rendering()
    {
        m_globalSettingsModel = winrt::make<ObjectModel::implementation::GlobalSettingsModel>();
        InitializeComponent();
    }

    ObjectModel::GlobalSettingsModel Rendering::GlobalSettingsModel()
    {
        return m_globalSettingsModel;
    }
}
