// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionsViewModel.h"
#include "ActionsViewModel.g.cpp"
#include "LibraryResources.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ActionsViewModel::ActionsViewModel()
    {

    }

    winrt::hstring ActionsViewModel::Name()
    {
        return L"ayy";
    }
}
