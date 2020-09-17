// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "Utils.h"

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings
{
    hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable)
    {
        Controls::ComboBox comboBox = comboBoxAsInspectable.as<Controls::ComboBox>();
        Controls::ComboBoxItem selectedOption = comboBox.SelectedItem().as<Controls::ComboBoxItem>();

        return unbox_value<hstring>(selectedOption.Tag());
    }

    // This can be used to populate a map<VirtualKey, hstring> to perform conversions from key lists to hstring and vice-versa
    hstring KeyToString(VirtualKey key)
    {
        hstring generatedString;

        if (key >= VirtualKey::F1 && key <= VirtualKey::F24)
        {
            return L"F" + hstring(std::to_wstring((int)key - (int)VirtualKey::F1 + 1));
        }

        if (key >= VirtualKey::A && key <= VirtualKey::Z)
        {
            return hstring(std::wstring(1, (wchar_t)key));
        }

        if (key >= VirtualKey::Number0 && key <= VirtualKey::Number9)
        {
            return hstring(std::to_wstring((int)key - (int)VirtualKey::Number0));
        }

        if (key >= VirtualKey::NumberPad0 && key <= VirtualKey::NumberPad9)
        {
            return L"numpad_" + hstring(std::to_wstring((int)key - (int)VirtualKey::NumberPad0));
        }

        switch (key)
        {
        case VirtualKey::Control:
            return L"ctrl+";
        case VirtualKey::Shift:
            return L"shift+";
        case VirtualKey::Menu:
            return L"alt+";
        case VirtualKey::Add:
            return L"plus";
        case VirtualKey::Subtract:
            return L"-";
        case VirtualKey::Divide:
            return L"/";
        case VirtualKey::Decimal:
            return L".";
        case VirtualKey::Left:
            return L"left";
        case VirtualKey::Down:
            return L"down";
        case VirtualKey::Right:
            return L"right";
        case VirtualKey::Up:
            return L"up";
        case VirtualKey::PageDown:
            return L"pagedown";
        case VirtualKey::PageUp:
            return L"pageup";
        case VirtualKey::End:
            return L"end";
        case VirtualKey::Home:
            return L"home";
        case VirtualKey::Tab:
            return L"tab";
        case VirtualKey::Enter:
            return L"enter";
        case VirtualKey::Escape:
            return L"esc";
        case VirtualKey::Space:
            return L"space";
        case VirtualKey::Back:
            return L"backspace";
        case VirtualKey::Delete:
            return L"delete";
        case VirtualKey::Insert:
            return L"insert";
        default:
            return L"";
        }

        return L"";
    }
}
