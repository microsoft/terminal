/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- KeyBindingContainer

Abstract:
- This is a XAML data template to represent a key binding as a list view item.

Author(s):
- Carlos Zamora - April 2021

--*/

#pragma once

#include "KeyBindingContainer.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct KeyBindingContainer : KeyBindingContainerT<KeyBindingContainer>
    {
    public:
        KeyBindingContainer() = delete;
        KeyBindingContainer(Model::Command cmd);

        WINRT_PROPERTY(Model::Command, Action, nullptr);
        //TYPED_EVENT(Unbind, Editor::KeyBindingContainer, Windows::Foundation::IInspectable);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(KeyBindingContainer);
}
