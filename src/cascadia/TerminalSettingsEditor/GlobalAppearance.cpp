// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "GlobalAppearance.h"
#include "GlobalAppearance.g.cpp"

#include <LibraryResources.h>
#include <WtExeUtils.h>

using namespace winrt;
using namespace WUX;
using namespace WUX::Navigation;
using namespace WUXC;
using namespace MTSM;
using namespace WFC;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    GlobalAppearance::GlobalAppearance()
    {
        InitializeComponent();
    }

    void GlobalAppearance::OnNavigatedTo(const NavigationEventArgs& e)
    {
        _ViewModel = e.Parameter().as<Editor::GlobalAppearanceViewModel>();
    }
}
