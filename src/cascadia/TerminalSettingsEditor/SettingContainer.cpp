// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingContainer.h"
#include "SettingContainer.g.cpp"
#include "LibraryResources.h"

using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty SettingContainer::_HeaderTextProperty =
        DependencyProperty::Register(
            L"HeaderText",
            xaml_typename<hstring>(),
            xaml_typename<Editor::SettingContainer>(),
            PropertyMetadata{ nullptr });

    DependencyProperty SettingContainer::_HelpTextProperty =
        DependencyProperty::Register(
            L"HelpText",
            xaml_typename<hstring>(),
            xaml_typename<Editor::SettingContainer>(),
            PropertyMetadata{ box_value(L"") });

    DependencyProperty SettingContainer::_HasSettingValueProperty =
        DependencyProperty::Register(
            L"HasSettingValue",
            xaml_typename<bool>(),
            xaml_typename<Editor::SettingContainer>(),
            PropertyMetadata{ box_value(true), PropertyChangedCallback{ &SettingContainer::_OnHasSettingValueChanged } });

    void SettingContainer::_OnHasSettingValueChanged(DependencyObject const& d, DependencyPropertyChangedEventArgs const& args)
    {
        const auto& obj{ d.try_as<Editor::SettingContainer>() };
        const auto& newVal{ unbox_value<bool>(args.NewValue()) };

        // update visibility for reset button
        if (auto resetBtnChild{ obj.GetTemplateChild(L"ResetButton") })
        {
            if (auto elem{ resetBtnChild.try_as<UIElement>() })
            {
                elem.Visibility(newVal ? Visibility::Visible : Visibility::Collapsed);
            }
        }

        // update visibility for override message
        if (auto overrideMsg{ obj.GetTemplateChild(L"OverrideMessage") })
        {
            if (auto elem{ overrideMsg.try_as<UIElement>() })
            {
                elem.Visibility(newVal ? Visibility::Visible : Visibility::Collapsed);
            }
        }
    }

    void SettingContainer::OnApplyTemplate()
    {
        if (auto child{ GetTemplateChild(L"ResetButton") })
        {
            if (auto btn{ child.try_as<Controls::Button>() })
            {
                // Apply click handler for the reset button.
                // When clicked, we dispatch the bound ClearSettingValue event,
                // resulting in inheriting the setting value from the parent.
                btn.Click([weakThis{ get_weak() }](auto&&, auto&&) {
                    if (auto container{ weakThis.get() })
                    {
                        container->_ClearSettingValueHandlers(*container, nullptr);
                    }
                });

                // apply tooltip and help text (automation property)
                const auto& helpText{ RS_(L"SettingContainer_ResetButtonHelpText") };
                Controls::ToolTipService::SetToolTip(child, box_value(helpText));
                Automation::AutomationProperties::SetName(child, helpText);

                // initialize visibility for reset button
                btn.Visibility(HasSettingValue() ? Visibility::Visible : Visibility::Collapsed);
            }
        }

        if (auto child{ GetTemplateChild(L"OverrideMessage") })
        {
            if (auto tb{ child.try_as<Controls::TextBlock>() })
            {
                // initialize visibility for reset button
                tb.Visibility(HasSettingValue() ? Visibility::Visible : Visibility::Collapsed);
            }
        }

        if (auto child{ GetTemplateChild(L"SettingControl") })
        {
            // apply header text as name (automation property)
            const auto& headerText{ HeaderText() };
            if (!headerText.empty())
            {
                Automation::AutomationProperties::SetName(child, headerText);
            }

            // apply help text as tooltip and help text (automation property)
            const auto& helpText{ HelpText() };
            if (!helpText.empty())
            {
                Controls::ToolTipService::SetToolTip(child, box_value(helpText));
                Automation::AutomationProperties::SetHelpText(child, helpText);
            }
        }
    }
}
