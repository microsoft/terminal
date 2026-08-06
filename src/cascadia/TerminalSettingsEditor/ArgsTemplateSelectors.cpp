// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ArgsTemplateSelectors.h"
#include "ArgsTemplateSelectors.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    // Method Description:
    // - This method is called once command palette decides how to render a filtered command.
    //   Currently we support two ways to render command, that depend on its palette item type:
    //   - For TabPalette item we render an icon, a title, and some tab-related indicators like progress bar (as defined by TabItemTemplate)
    //   - All other items are currently rendered with icon, title and optional key-chord (as defined by GeneralItemTemplate)
    // Arguments:
    // - item - an instance of filtered command to render
    // Return Value:
    // - data template to use for rendering
    Windows::UI::Xaml::DataTemplate ArgsTemplateSelectors::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& /*container*/)
    {
        static constexpr std::pair<
            std::wstring_view,
            til::property<Windows::UI::Xaml::DataTemplate> ArgsTemplateSelectors::*>
            lut[] = {
                { L"int32_t", &ArgsTemplateSelectors::Int32Template },
                { L"uint32_t", &ArgsTemplateSelectors::UInt32Template },
                { L"bool", &ArgsTemplateSelectors::BoolTemplate },
                { L"Windows::Foundation::IReference<bool>", &ArgsTemplateSelectors::BoolOptionalTemplate },
                { L"Windows::Foundation::IReference<int32_t>", &ArgsTemplateSelectors::Int32OptionalTemplate },
                { L"Windows::Foundation::IReference<uint32_t>", &ArgsTemplateSelectors::UInt32OptionalTemplate },
                { L"SuggestionsSource", &ArgsTemplateSelectors::FlagTemplate },
                { L"Windows::Foundation::IReference<Control::CopyFormat>", &ArgsTemplateSelectors::FlagTemplate },
                { L"Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>", &ArgsTemplateSelectors::TerminalCoreColorOptionalTemplate },
                { L"Windows::Foundation::IReference<Windows::UI::Color>", &ArgsTemplateSelectors::WindowsUIColorOptionalTemplate },
                { L"Model::ResizeDirection", &ArgsTemplateSelectors::EnumTemplate },
                { L"Model::FocusDirection", &ArgsTemplateSelectors::EnumTemplate },
                { L"SettingsTarget", &ArgsTemplateSelectors::EnumTemplate },
                { L"MoveTabDirection", &ArgsTemplateSelectors::EnumTemplate },
                { L"Microsoft::Terminal::Control::ScrollToMarkDirection", &ArgsTemplateSelectors::EnumTemplate },
                { L"CommandPaletteLaunchMode", &ArgsTemplateSelectors::EnumTemplate },
                { L"FindMatchDirection", &ArgsTemplateSelectors::EnumTemplate },
                { L"Model::DesktopBehavior", &ArgsTemplateSelectors::EnumTemplate },
                { L"Model::MonitorBehavior", &ArgsTemplateSelectors::EnumTemplate },
                { L"winrt::Microsoft::Terminal::Control::ClearBufferType", &ArgsTemplateSelectors::EnumTemplate },
                { L"SelectOutputDirection", &ArgsTemplateSelectors::EnumTemplate },
                { L"Windows::Foundation::IReference<TabSwitcherMode>", &ArgsTemplateSelectors::EnumTemplate },
                { L"Model::SplitDirection", &ArgsTemplateSelectors::EnumTemplate },
                { L"SplitType", &ArgsTemplateSelectors::EnumTemplate },
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
            else if (argType == L"float")
            {
                const auto argTag = argWrapper.TypeHint();
                switch (argTag)
                {
                case Model::ArgTypeHint::SplitSize:
                    return SplitSizeTemplate();
                default:
                    return FloatTemplate();
                }
            }

            for (const auto& [type, member] : lut)
            {
                if (type == argType)
                {
                    return (this->*member)();
                }
            }
        }
        return nullptr;
    }
}
