// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "KeyChordVisual.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct KeyChordVisual : KeyChordVisualT<KeyChordVisual>
    {
    public:
        KeyChordVisual();

        DEPENDENCY_PROPERTY(Control::KeyChord, KeyChord);

    private:
        static void _InitializeProperties();
        static void _OnKeyChordChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);

        void _UpdateKeyVisuals();
        void _AddTextKey(const winrt::hstring& text);
        void _AddGlyphKey();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(KeyChordVisual);
}
