// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ActionsTemplateSelectors.h"
#include "ActionsTemplateSelectors.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Windows::UI::Xaml::DataTemplate ActionsTemplateSelectors::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item, const winrt::Windows::UI::Xaml::DependencyObject& /*container*/)
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
    Windows::UI::Xaml::DataTemplate ActionsTemplateSelectors::SelectTemplateCore(const winrt::Windows::Foundation::IInspectable& item)
    {
        if (const auto actionArgsVM{ item.try_as<winrt::Microsoft::Terminal::Settings::Editor::ActionArgsViewModel>() })
        {
            const auto shortcutAction = actionArgsVM.ShortcutActionType();
            switch (shortcutAction)
            {
            case Microsoft::Terminal::Settings::Model::ShortcutAction::SendInput:
                return SendInputTemplate();
            case Microsoft::Terminal::Settings::Model::ShortcutAction::CloseTab:
                return CloseTabTemplate();
            }
        }
        return nullptr;
    }
}
