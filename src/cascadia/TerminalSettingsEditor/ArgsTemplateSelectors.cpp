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
        if (const auto argWrapper{ item.try_as<Microsoft::Terminal::Settings::Editor::ArgWrapper>() })
        {
            const auto argType = argWrapper.Type();
            if (argType == L"winrt::hstring")
            {
                return StringTemplate();
            }
            else if (argType == L"int32_t")
            {
                return Int32Template();
            }
            else if (argType == L"uint32_t")
            {
                return UInt32Template();
            }
            else if (argType == L"float")
            {
                return FloatTemplate();
            }
            else if (argType == L"bool")
            {
                // we don't have any bool args that are required, so just use the optional template for all of them
                return BoolOptionalTemplate();
            }
            else if (argType == L"Windows::Foundation::IReference<uint32_t>")
            {
                return UInt32OptionalTemplate();
            }
            else if (argType == L"Model::ResizeDirection" ||
                     argType == L"Model::FocusDirection" ||
                     argType == L"SettingsTarget" ||
                     argType == L"MoveTabDirection" ||
                     argType == L"Microsoft::Terminal::Control::ScrollToMarkDirection" ||
                     argType == L"CommandPaletteLaunchMode" ||
                     argType == L"FindMatchDirection" ||
                     argType == L"Model::DesktopBehavior" ||
                     argType == L"Model::MonitorBehavior" ||
                     argType == L"winrt::Microsoft::Terminal::Control::ClearBufferType" ||
                     argType == L"SelectOutputDirection")
            {
                return EnumTemplate();
            }
            else if (argType == L"SuggestionsSource")
            {
                return FlagTemplate();
            }
            else if (argType == L"Windows::Foundation::IReference<Microsoft::Terminal::Core::Color>")
            {
                return TerminalCoreColorOptionalTemplate();
            }
            else if (argType == L"Windows::Foundation::IReference<Windows::UI::Color>")
            {
                return WindowsUIColorOptionalTemplate();
            }
            else if (argType == L"Windows::Foundation::IReference<Control::CopyFormat>")
            {
                return nullptr;
            }
        }
        return NoArgTemplate();
    }
}
