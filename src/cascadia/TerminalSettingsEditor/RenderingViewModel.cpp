// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "RenderingViewModel.h"

#include "EnumEntry.h"

#include "RenderingViewModel.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    RenderingViewModel::RenderingViewModel(CascadiaSettings settings) noexcept :
        _settings{ std::move(settings) }
    {
        INITIALIZE_BINDABLE_ENUM_SETTING(GraphicsAPI, GraphicsAPI, winrt::Microsoft::Terminal::Control::GraphicsAPI, L"Globals_GraphicsAPI_", L"Text");
    }
}
