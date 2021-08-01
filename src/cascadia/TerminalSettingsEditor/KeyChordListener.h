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

        void KeyChordTextBox_KeyDown(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        DEPENDENCY_PROPERTY(Control::KeyChord, Keys);

    private:
        static void _InitializeProperties();
        static void _OnKeysChanged(Windows::UI::Xaml::DependencyObject const& d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(KeyChordListener);
}
