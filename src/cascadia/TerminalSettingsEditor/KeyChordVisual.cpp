// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "KeyChordVisual.h"
#include "KeyChordVisual.g.cpp"

using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty KeyChordVisual::_KeyChordProperty{ nullptr };

    KeyChordVisual::KeyChordVisual()
    {
        InitializeComponent();
        _InitializeProperties();
    }

    void KeyChordVisual::_InitializeProperties()
    {
        if (!_KeyChordProperty)
        {
            _KeyChordProperty =
                DependencyProperty::Register(
                    L"KeyChord",
                    xaml_typename<Control::KeyChord>(),
                    xaml_typename<Editor::KeyChordVisual>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &KeyChordVisual::_OnKeyChordChanged } });
        }
    }

    void KeyChordVisual::_OnKeyChordChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        if (const auto control{ d.try_as<Editor::KeyChordVisual>() })
        {
            const auto controlImpl{ get_self<KeyChordVisual>(control) };
            controlImpl->_UpdateKeyVisuals();
        }
    }

    // Capitalizes the first character of the provided string.
    // Examples: "enter" -> "Enter", "f1" -> "F1", "v" -> "V"
    static winrt::hstring _formatMainKeyName(std::wstring_view part)
    {
        if (part.empty())
        {
            return {};
        }

        std::wstring buffer{ part };
        buffer[0] = til::toupper_ascii(buffer[0]);
        return winrt::hstring{ buffer };
    }

    void KeyChordVisual::_UpdateKeyVisuals()
    {
        auto panel{ KeysPanel() };
        if (!panel)
        {
            return;
        }
        panel.Children().Clear();

        const auto kc{ KeyChord() };
        if (!kc)
        {
            return;
        }

        // Reuse the canonical serialization so the key naming stays in sync with the
        // rest of the app. Then split on '+' (no key name in the table contains a literal
        // '+'; VK_OEM_PLUS serializes as "plus") and render each part as its own visual.
        const auto serialized{ Model::KeyChordSerialization::ToString(kc) };
        if (serialized.empty())
        {
            return;
        }

        const std::wstring_view full{ serialized };
        for (const auto part : til::split_iterator{ full, L'+' })
        {
            if (til::equals_insensitive_ascii(part, L"win"))
            {
                _AddGlyphKey();
            }
            else if (til::equals_insensitive_ascii(part, L"ctrl"))
            {
                _AddTextKey(L"Ctrl");
            }
            else if (til::equals_insensitive_ascii(part, L"alt"))
            {
                _AddTextKey(L"Alt");
            }
            else if (til::equals_insensitive_ascii(part, L"shift"))
            {
                _AddTextKey(L"Shift");
            }
            else
            {
                _AddTextKey(_formatMainKeyName(part));
            }
        }
    }

    void KeyChordVisual::_AddTextKey(const winrt::hstring& text)
    {
        const auto tmpl{ Resources().Lookup(box_value(L"KeyChordVisualTextKeyTemplate")).as<DataTemplate>() };
        const auto border{ tmpl.LoadContent().as<Border>() };
        if (const auto tb{ border.Child().try_as<TextBlock>() })
        {
            tb.Text(text);
        }
        KeysPanel().Children().Append(border);
    }

    void KeyChordVisual::_AddGlyphKey()
    {
        const auto tmpl{ Resources().Lookup(box_value(L"KeyChordVisualWindowsKeyTemplate")).as<DataTemplate>() };
        const auto border{ tmpl.LoadContent().as<Border>() };

        // Provide an accessible name for the glyph since it has no text fallback.
        if (const auto path{ border.Child() })
        {
            Automation::AutomationProperties::SetName(path, L"Win");
        }
        KeysPanel().Children().Append(border);
    }
}
