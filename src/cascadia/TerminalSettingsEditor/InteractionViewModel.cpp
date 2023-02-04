// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "InteractionViewModel.h"
#include "InteractionViewModel.g.cpp"
#include "EnumEntry.h"

using namespace WUX::Navigation;
using namespace WF;
using namespace MTSM;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    InteractionViewModel::InteractionViewModel(MTSM::GlobalAppSettings globalSettings) :
        _GlobalSettings{ globalSettings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(TabSwitcherMode, TabSwitcherMode, TabSwitcherMode, L"Globals_TabSwitcherMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(CopyFormat, CopyFormat, MTControl::CopyFormat, L"Globals_CopyFormat", L"Content");
    }
}
