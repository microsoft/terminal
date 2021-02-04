// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingContainer.h"
#include "SettingContainer.g.cpp"
#include "LibraryResources.h"

using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty SettingContainer::_HeaderProperty{ nullptr };
    DependencyProperty SettingContainer::_HelpTextProperty{ nullptr };
    DependencyProperty SettingContainer::_HasSettingValueProperty{ nullptr };

    SettingContainer::SettingContainer()
    {
        _InitializeProperties();
    }

    void SettingContainer::_InitializeProperties()
    {
        // Initialize any SettingContainer dependency properties here.
        // This performs a lazy load on these properties, instead of
        // initializing them when the DLL loads.
        if (!_HeaderProperty)
        {
            _HeaderProperty =
                DependencyProperty::Register(
                    L"Header",
                    xaml_typename<IInspectable>(),
                    xaml_typename<Editor::SettingContainer>(),
                    PropertyMetadata{ nullptr });
        }
        if (!_HelpTextProperty)
        {
            _HelpTextProperty =
                DependencyProperty::Register(
                    L"HelpText",
                    xaml_typename<hstring>(),
                    xaml_typename<Editor::SettingContainer>(),
                    PropertyMetadata{ box_value(L"") });
        }
        if (!_HasSettingValueProperty)
        {
            _HasSettingValueProperty =
                DependencyProperty::Register(
                    L"HasSettingValue",
                    xaml_typename<bool>(),
                    xaml_typename<Editor::SettingContainer>(),
                    PropertyMetadata{ box_value(false), PropertyChangedCallback{ &SettingContainer::_OnHasSettingValueChanged } });
        }
    }

    void SettingContainer::_OnHasSettingValueChanged(DependencyObject const& d, DependencyPropertyChangedEventArgs const& args)
    {
        const auto& obj{ d.try_as<Editor::SettingContainer>() };
        const auto& newVal{ unbox_value<bool>(args.NewValue()) };

        // update visibility for reset button
        if (auto resetButton{ obj.GetTemplateChild(L"ResetButton") })
        {
            if (auto elem{ resetButton.try_as<UIElement>() })
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
            if (auto button{ child.try_as<Controls::Button>() })
            {
                // Apply click handler for the reset button.
                // When clicked, we dispatch the bound ClearSettingValue event,
                // resulting in inheriting the setting value from the parent.
                button.Click([=](auto&&, auto&&) {
                    _ClearSettingValueHandlers(*this, nullptr);
                });

                // apply tooltip and help text (automation property)
                const auto& helpText{ RS_(L"SettingContainer_ResetButtonHelpText") };
                Controls::ToolTipService::SetToolTip(child, box_value(helpText));
                Automation::AutomationProperties::SetName(child, helpText);

                // initialize visibility for reset button
                button.Visibility(HasSettingValue() ? Visibility::Visible : Visibility::Collapsed);
            }
        }

        if (auto child{ GetTemplateChild(L"OverrideMessage") })
        {
            if (auto tb{ child.try_as<Controls::TextBlock>() })
            {
                // TODO GH#6800: 'OverrideTarget' will be replaced with hyperlink/text directing the user to another profile.
                // Create the override message
                const auto overrideMsg{ fmt::format(std::wstring{ RS_(L"SettingContainer_OverrideIntro") }, RS_(L"SettingContainer_OverrideTarget")) };
                tb.Text(overrideMsg);

                // initialize visibility for reset button
                tb.Visibility(HasSettingValue() ? Visibility::Visible : Visibility::Collapsed);
            }
        }

        if (auto content{ Content() })
        {
            if (auto obj{ content.try_as<DependencyObject>() })
            {
                // apply header text as name (automation property)
                if (const auto& header{ Header() })
                {
                    const auto headerText{ header.try_as<hstring>() };
                    if (headerText && !headerText->empty())
                    {
                        Automation::AutomationProperties::SetName(obj, *headerText);
                    }
                }

                // apply help text as tooltip and help text (automation property)
                const auto& helpText{ HelpText() };
                if (!helpText.empty())
                {
                    Controls::ToolTipService::SetToolTip(obj, box_value(helpText));
                    Automation::AutomationProperties::SetHelpText(obj, helpText);
                }
            }
        }
    }
}
