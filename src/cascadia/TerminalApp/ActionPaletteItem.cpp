// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionPaletteItem.h"
#include <LibraryResources.h>

#include "ActionPaletteItem.g.cpp"

#include <regex>
#include <vector>

using namespace winrt;
using namespace winrt::TerminalApp;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::TerminalApp::implementation
{
    static std::wstring scan_code_to_name(const std::wstring& str)
    {
        std::size_t idx = 0;
        auto scan_code = std::stoul(str, &idx);

        if (idx != str.length())
            throw std::runtime_error{ "cannot convert scan_code from str" };

        std::vector<wchar_t> buffer;
        buffer.resize(64);
        if (!GetKeyNameTextW(scan_code << 16, buffer.data(), static_cast<int>(buffer.size())))
        {
            throw std::runtime_error{ "cannot resolve scan_code to name" };
        }

        return { buffer.data() };
    }

    static winrt::hstring replace_scan_codes(const std::wstring_view& input)
    {
        static std::wregex regex{ L"sc\\(([0-9]+)\\)" };

        std::wstring key_chord_text{ input.begin(), input.end() };
        
        std::wstring result;

        std::wsregex_token_iterator end;
        std::wsregex_token_iterator iter{ key_chord_text.begin(), key_chord_text.end(), regex, 1 };
        std::wstring::const_iterator last = key_chord_text.begin();
        for (; iter != end; ++iter)
        {
            if (iter->matched)
            {
                constexpr auto length_of_scan_code_prefix = 3;
                result.append(last, iter->first - length_of_scan_code_prefix);

                try
                {
                    result.append(scan_code_to_name(iter->str()));
                }
                catch (const std::runtime_error&)
                {
                    // LOG?
                    result.append(L"sc(" + iter->str() + L")");
                }

                constexpr auto length_of_scan_code_suffix = 1;
                last = iter->second + length_of_scan_code_suffix;
            }
        }

        result.append(last, key_chord_text.cend());

        return winrt::hstring{ result.c_str() };
    }

    ActionPaletteItem::ActionPaletteItem(Microsoft::Terminal::Settings::Model::Command const& command) :
        _Command(command)
    {
        Name(command.Name());
        KeyChordText(replace_scan_codes(command.KeyChordText()));
        Icon(command.IconPath());

        _commandChangedRevoker = command.PropertyChanged(winrt::auto_revoke, [weakThis{ get_weak() }](auto& sender, auto& e) {
            auto item{ weakThis.get() };
            auto senderCommand{ sender.try_as<Microsoft::Terminal::Settings::Model::Command>() };

            if (item && senderCommand)
            {
                auto changedProperty = e.PropertyName();
                if (changedProperty == L"Name")
                {
                    item->Name(senderCommand.Name());
                }
                else if (changedProperty == L"KeyChordText")
                {
                    item->KeyChordText(replace_scan_codes(senderCommand.KeyChordText()));
                }
                else if (changedProperty == L"IconPath")
                {
                    item->Icon(senderCommand.IconPath());
                }
            }
        });
    }
}
