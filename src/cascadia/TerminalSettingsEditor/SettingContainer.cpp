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
        if (const auto& resetButton{ obj.GetTemplateChild(L"ResetButton") })
        {
            if (const auto& elem{ resetButton.try_as<UIElement>() })
            {
                elem.Visibility(newVal ? Visibility::Visible : Visibility::Collapsed);
            }
        }

        // update visibility for override message
        if (const auto& overrideMsg{ obj.GetTemplateChild(L"OverrideMessage") })
        {
            if (const auto& elem{ overrideMsg.try_as<UIElement>() })
            {
                elem.Visibility(newVal ? Visibility::Visible : Visibility::Collapsed);
            }
        }
    }

    void SettingContainer::OnApplyTemplate()
    {
        // This message is only populated if `HasSettingValue` is true.
        const auto& overrideMsg{ _GenerateOverrideMessageText() };

        if (const auto& child{ GetTemplateChild(L"ResetButton") })
        {
            if (const auto& button{ child.try_as<Controls::Button>() })
            {
                // Apply click handler for the reset button.
                // When clicked, we dispatch the bound ClearSettingValue event,
                // resulting in inheriting the setting value from the parent.
                button.Click([=](auto&&, auto&&) {
                    _ClearSettingValueHandlers(*this, nullptr);

                    // move the focus to the child control
                    if (const auto& content{ Content() })
                    {
                        if (const auto& control{ content.try_as<Controls::Control>() })
                        {
                            control.Focus(FocusState::Programmatic);
                            return;
                        }
                        else if (const auto& panel{ content.try_as<Controls::Panel>() })
                        {
                            for (const auto& panelChild : panel.Children())
                            {
                                if (const auto& panelControl{ panelChild.try_as<Controls::Control>() })
                                {
                                    panelControl.Focus(FocusState::Programmatic);
                                    return;
                                }
                            }
                        }
                        // if we get here, we didn't find something to reasonably focus to.
                    }
                });

                // apply tooltip and name (automation property)
                const auto& name{ RS_(L"SettingContainer_ResetButtonHelpText") };
                Controls::ToolTipService::SetToolTip(child, box_value(name));
                Automation::AutomationProperties::SetName(child, name);
                Automation::AutomationProperties::SetHelpText(child, overrideMsg);

                // initialize visibility for reset button
                button.Visibility(HasSettingValue() ? Visibility::Visible : Visibility::Collapsed);
            }
        }

        if (const auto& child{ GetTemplateChild(L"OverrideMessage") })
        {
            if (const auto& tb{ child.try_as<Controls::TextBlock>() })
            {
                if (!overrideMsg.empty())
                {
                    // Create the override message
                    // TODO GH#6800: the override target will be replaced with hyperlink/text directing the user to another profile.
                    tb.Text(overrideMsg);

                    // initialize visibility for reset button
                    tb.Visibility(Visibility::Visible);
                }
                else
                {
                    // we have no message to render
                    tb.Visibility(Visibility::Collapsed);
                }
            }
        }

        if (const auto& content{ Content() })
        {
            if (const auto& obj{ content.try_as<DependencyObject>() })
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

                // apply help text as tooltip and full description (automation property)
                const auto& helpText{ HelpText() };
                if (!helpText.empty())
                {
                    Controls::ToolTipService::SetToolTip(obj, box_value(helpText));
                    Automation::AutomationProperties::SetFullDescription(obj, helpText);
                }
            }
        }
    }

    hstring SettingContainer::_GenerateOverrideMessageText()
    {
        if (HasSettingValue())
        {
            return hstring{ fmt::format(std::wstring_view{ RS_(L"SettingContainer_OverrideIntro") }, RS_(L"SettingContainer_OverrideTarget")) };
        }
        return {};
    }
}
