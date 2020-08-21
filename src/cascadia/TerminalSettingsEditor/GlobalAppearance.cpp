// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"
#include <ObjectModel\GlobalSettings.h>

using namespace winrt;
using namespace Windows::UI::Xaml;

namespace winrt::SettingsControl::implementation
{
    GlobalAppearance::GlobalAppearance()
    {
        m_globalSettingsModel = winrt::make<ObjectModel::implementation::GlobalSettingsModel>();
        InitializeComponent();
    }

    ObjectModel::GlobalSettingsModel GlobalAppearance::GlobalSettingsModel()
    {
        return m_globalSettingsModel;
    }
}
