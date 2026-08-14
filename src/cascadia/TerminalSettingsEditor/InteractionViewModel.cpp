// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "InteractionViewModel.h"
#include "InteractionViewModel.g.cpp"
#include "EnumEntry.h"

#include "../../types/inc/utils.hpp"

using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    InteractionViewModel::InteractionViewModel(Model::GlobalAppSettings globalSettings, Model::WindowSettings windowSettings) :
        _GlobalSettings{ globalSettings },
        _WindowSettings{ windowSettings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(TabSwitcherMode, TabSwitcherMode, TabSwitcherMode, L"Globals_TabSwitcherMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(CopyFormat, CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, L"Globals_CopyFormat", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ConfirmOnClose, ConfirmOnClose, Model::ConfirmOnClose, L"Globals_ConfirmOnClose", L"Content");
    }

    // Grey out the toggle when drag/drop is forced off regardless of the setting
    // (elevated / different user); see Utils::CanUwpDragDrop. GH#15689.
    bool InteractionViewModel::CanEnableTabDragDrop() const noexcept
    {
        return ::Microsoft::Console::Utils::CanUwpDragDrop();
    }

    // Explain why the toggle is greyed out when drag/drop is unavailable.
    winrt::hstring InteractionViewModel::TabDragDropStatefulHelpText() const
    {
        if (!::Microsoft::Console::Utils::CanUwpDragDrop())
        {
            return RS_(L"Globals_EnableTabDragDrop_Unavailable");
        }
        return RS_(L"Globals_EnableTabDragDrop/HelpText");
    }
}
