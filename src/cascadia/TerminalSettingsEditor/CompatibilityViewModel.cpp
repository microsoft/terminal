// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CompatibilityViewModel.h"
#include "CompatibilityViewModel.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    CompatibilityViewModel::CompatibilityViewModel(Model::CascadiaSettings settings) noexcept :
        _settings{ std::move(settings) }
    {
    }

    bool CompatibilityViewModel::ShellCompletionMenuAvailable() const noexcept
    {
        return Feature_ShellCompletions::IsEnabled();
    }
}
