// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ArgsTemplateSelectors.h"
#include "ArgsTemplateSelectors.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Windows::UI::Xaml::DataTemplate ArgsTemplateSelectors::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& /*container*/)
    {
        return SelectTemplateCore(item);
    }

    // Method Description:
    // - This method is called once command palette decides how to render a filtered command.
    //   Currently we support two ways to render command, that depend on its palette item type:
    //   - For TabPalette item we render an icon, a title, and some tab-related indicators like progress bar (as defined by TabItemTemplate)
    //   - All other items are currently rendered with icon, title and optional key-chord (as defined by GeneralItemTemplate)
    // Arguments:
    // - item - an instance of filtered command to render
    // Return Value:
    // - data template to use for rendering
    Windows::UI::Xaml::DataTemplate ArgsTemplateSelectors::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item)
    {
        static constexpr std::pair<std::wstring_view, Windows::UI::Xaml::DataTemplate ArgsTemplateSelectors::*> lut[] = {
            { L"int32_t", &ArgsTemplateSelectors::_Int32Template },
            { L"uint32_t", &ArgsTemplateSelectors::_UInt32Template },
            { L"float", &ArgsTemplateSelectors::_FloatTemplate },
            { L"bool", &ArgsTemplateSelectors::_BoolTemplate },
            { L"Windows::Foundation::IReference<bool>", &ArgsTemplateSelectors::_BoolOptionalTemplate },
            { L"Windows::Foundation::IReference<int32_t>", &ArgsTemplateSelectors::_Int32OptionalTemplate },
            { L"Windows::Foundation::IReference<uint32_t>", &ArgsTemplateSelectors::_UInt32OptionalTemplate },
            { L"SuggestionsSource", &ArgsTemplateSelectors::_FlagTemplate },
            { L"Windows::Foundation::IReference<Control::CopyFormat>", &ArgsTemplateSelectors::_FlagTemplate },
            { L"Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>", &ArgsTemplateSelectors::_TerminalCoreColorOptionalTemplate },
            { L"Windows::Foundation::IReference<Windows::UI::Color>", &ArgsTemplateSelectors::_WindowsUIColorOptionalTemplate },
            { L"Model::ResizeDirection", &ArgsTemplateSelectors::_EnumTemplate },
            { L"Model::FocusDirection", &ArgsTemplateSelectors::_EnumTemplate },
            { L"SettingsTarget", &ArgsTemplateSelectors::_EnumTemplate },
            { L"MoveTabDirection", &ArgsTemplateSelectors::_EnumTemplate },
            { L"Microsoft::Terminal::Control::ScrollToMarkDirection", &ArgsTemplateSelectors::_EnumTemplate },
            { L"CommandPaletteLaunchMode", &ArgsTemplateSelectors::_EnumTemplate },
            { L"FindMatchDirection", &ArgsTemplateSelectors::_EnumTemplate },
            { L"Model::DesktopBehavior", &ArgsTemplateSelectors::_EnumTemplate },
            { L"Model::MonitorBehavior", &ArgsTemplateSelectors::_EnumTemplate },
            { L"winrt::Microsoft::Terminal::Control::ClearBufferType", &ArgsTemplateSelectors::_EnumTemplate },
            { L"SelectOutputDirection", &ArgsTemplateSelectors::_EnumTemplate },
            { L"Windows::Foundation::IReference<TabSwitcherMode>", &ArgsTemplateSelectors::_EnumTemplate },
            { L"Model::SplitDirection", &ArgsTemplateSelectors::_EnumTemplate },
            { L"SplitType", &ArgsTemplateSelectors::_EnumTemplate },
        };

        if (const auto argWrapper{ item.try_as<Microsoft::Terminal::Settings::Editor::ArgWrapper>() })
        {
            const auto argType = argWrapper.Type();
            if (argType == L"winrt::hstring")
            {
                // string has some special cases - check the tag
                const auto argTag = argWrapper.TypeHint();
                switch (argTag)
                {
                case Model::ArgTypeHint::ColorScheme:
                    return ColorSchemeTemplate();
                case Model::ArgTypeHint::FilePath:
                    return FilePickerTemplate();
                case Model::ArgTypeHint::FolderPath:
                    return FolderPickerTemplate();
                default:
                    // no special handling required, just return the normal string template
                    return StringTemplate();
                }
            }

            for (const auto& [type, member] : lut)
            {
                if (type == argType)
                {
                    return this->*member;
                }
            }
        }
        return nullptr;
    }
}
