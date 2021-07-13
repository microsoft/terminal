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

        void KeyChordTextBox_LosingFocus(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::LosingFocusEventArgs const& e);
        void KeyChordTextBox_KeyDown(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        DEPENDENCY_PROPERTY(Control::KeyChord, CurrentKeys);
        WINRT_OBSERVABLE_PROPERTY(Control::KeyChord, ProposedKeys, _PropertyChangedHandlers, nullptr);

    private:
        static void _InitializeProperties();
        static void _OnCurrentKeysChanged(Windows::UI::Xaml::DependencyObject const& d, Windows::UI::Xaml::DependencyPropertyChangedEventArgs const& e);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(KeyChordListener);
}
