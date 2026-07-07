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
    InteractionViewModel::InteractionViewModel(Model::GlobalAppSettings globalSettings) :
        _GlobalSettings{ globalSettings }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(TabSwitcherMode, TabSwitcherMode, TabSwitcherMode, L"Globals_TabSwitcherMode", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(CopyFormat, CopyFormat, winrt::Microsoft::Terminal::Control::CopyFormat, L"Globals_CopyFormat", L"Content");
        INITIALIZE_BINDABLE_ENUM_SETTING(ConfirmOnClose, ConfirmOnClose, Model::ConfirmOnClose, L"Globals_ConfirmOnClose", L"Content");
    }

    // The enableTabDragDrop setting only has an effect when the OS will actually
    // let us drag/drop tabs. When running elevated or as a different user, the
    // drag/drop broker denies us and would crash the window, so we force it off
    // regardless (see Utils::CanUwpDragDrop, GH#15689) - reflect that by
    // disabling the control here.
    bool InteractionViewModel::CanEnableTabDragDrop() const noexcept
    {
        return ::Microsoft::Console::Utils::CanUwpDragDrop();
    }

    // When drag/drop is unavailable, replace the normal help text with an
    // explanation of why the toggle is disabled (it's otherwise not obvious).
    winrt::hstring InteractionViewModel::TabDragDropStatefulHelpText() const
    {
        if (!::Microsoft::Console::Utils::CanUwpDragDrop())
        {
            return RS_(L"Globals_EnableTabDragDrop_Unavailable");
        }
        return RS_(L"Globals_EnableTabDragDrop/HelpText");
    }
}
