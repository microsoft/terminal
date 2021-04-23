// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyBindingContainer.h"
#include "KeyBindingContainer.g.cpp"
//#include "LibraryResources.h"

using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    KeyBindingContainer::KeyBindingContainer(Command cmd) :
        _Action{ cmd }
    {
        InitializeComponent();
    }

}
