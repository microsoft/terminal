// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChordListener.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct KeyChordListener : KeyChordListenerT<KeyChordListener>
    {
    public:
        KeyChordListener();

        void KeyChordTextBox_KeyDown(const WF::IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);

        DEPENDENCY_PROPERTY(Control::KeyChord, Keys);

    private:
        static void _InitializeProperties();
        static void _OnKeysChanged(const WUX::DependencyObject& d, const WUX::DependencyPropertyChangedEventArgs& e);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(KeyChordListener);
}
