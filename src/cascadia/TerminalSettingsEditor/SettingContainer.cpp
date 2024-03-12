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
    DependencyProperty SettingContainer::_CurrentValueProperty{ nullptr };
    DependencyProperty SettingContainer::_HasSettingValueProperty{ nullptr };
    DependencyProperty SettingContainer::_SettingOverrideSourceProperty{ nullptr };
    DependencyProperty SettingContainer::_StartExpandedProperty{ nullptr };

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
        if (!_CurrentValueProperty)
        {
            _CurrentValueProperty =
                DependencyProperty::Register(
                    L"CurrentValue",
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
        if (!_SettingOverrideSourceProperty)
        {
            _SettingOverrideSourceProperty =
                DependencyProperty::Register(
                    L"SettingOverrideSource",
                    xaml_typename<IInspectable>(),
                    xaml_typename<Editor::SettingContainer>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &SettingContainer::_OnHasSettingValueChanged } });
        }
        if (!_StartExpandedProperty)
        {
            _StartExpandedProperty =
                DependencyProperty::Register(
                    L"StartExpanded",
                    xaml_typename<bool>(),
                    xaml_typename<Editor::SettingContainer>(),
                    PropertyMetadata{ box_value(false) });
        }
    }

    void SettingContainer::_OnHasSettingValueChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*args*/)
    {
        // update visibility for override message and reset button
        const auto& obj{ d.try_as<Editor::SettingContainer>() };
        get_self<SettingContainer>(obj)->_UpdateOverrideSystem();
    }

    void SettingContainer::OnApplyTemplate()
    {
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

                // apply name (automation property)
                Automation::AutomationProperties::SetName(child, RS_(L"SettingContainer_OverrideMessageBaseLayer"));
            }
        }

        _UpdateOverrideSystem();

        // Get the correct base to apply automation properties to
        std::vector<DependencyObject> base;
        base.reserve(2);
        if (const auto& child{ GetTemplateChild(L"Expander") })
        {
            if (const auto& expander{ child.try_as<Microsoft::UI::Xaml::Controls::Expander>() })
            {
                base.push_back(child);
            }
        }
        if (const auto& content{ Content() })
        {
            const auto& panel{ content.try_as<Controls::Panel>() };
            const auto& obj{ content.try_as<DependencyObject>() };
            if (!panel && obj)
            {
                base.push_back(obj);
            }
        }

        for (const auto& obj : base)
        {
            // apply header as name (automation property)
            if (const auto& header{ Header() })
            {
                if (const auto headerText{ header.try_as<hstring>() })
                {
                    Automation::AutomationProperties::SetName(obj, *headerText);
                }
            }

            // apply help text as tooltip and full description (automation property)
            if (const auto& helpText{ HelpText() }; !helpText.empty())
            {
                Automation::AutomationProperties::SetFullDescription(obj, helpText);
            }
            else
            {
                Controls::ToolTipService::SetToolTip(obj, nullptr);
                Automation::AutomationProperties::SetFullDescription(obj, L"");
            }
        }

        const auto textBlockHidden = HelpText().empty();
        if (const auto& child{ GetTemplateChild(L"HelpTextBlock") })
        {
            if (const auto& textBlock{ child.try_as<Controls::TextBlock>() })
            {
                textBlock.Visibility(textBlockHidden ? Visibility::Collapsed : Visibility::Visible);
            }
        }
    }

    void SettingContainer::SetExpanded(bool expanded)
    {
        if (const auto& child{ GetTemplateChild(L"Expander") })
        {
            if (const auto& expander{ child.try_as<Microsoft::UI::Xaml::Controls::Expander>() })
            {
                expander.IsExpanded(expanded);
            }
        }
    }

    // Method Description:
    // - Updates the override system visibility and text
    // Arguments:
    // - <none>
    void SettingContainer::_UpdateOverrideSystem()
    {
        if (const auto& child{ GetTemplateChild(L"ResetButton") })
        {
            if (const auto& button{ child.try_as<Controls::Button>() })
            {
                if (HasSettingValue())
                {
                    // We want to be smart about showing the override system.
                    // Don't just show it if the user explicitly set the setting.
                    // If the tooltip is empty, we'll hide the entire override system.

                    const auto& settingSrc{ SettingOverrideSource() };
                    const auto tooltip{ _GenerateOverrideMessage(settingSrc) };

                    Controls::ToolTipService::SetToolTip(button, box_value(tooltip));
                    button.Visibility(tooltip.empty() ? Visibility::Collapsed : Visibility::Visible);
                }
                else
                {
                    // a value is not being overridden; hide the override system
                    button.Visibility(Visibility::Collapsed);
                }
            }
        }
    }

    // Method Description:
    // - Helper function for generating the override message
    // Arguments:
    // - profile: the profile that defines the setting (aka SettingOverrideSource)
    // Return Value:
    // - text specifying where the setting was defined. If empty, we don't want to show the system.
    hstring SettingContainer::_GenerateOverrideMessage(const IInspectable& settingOrigin)
    {
        // We only get here if the user had an override in place.
        auto originTag{ Model::OriginTag::None };
        winrt::hstring source;

        if (const auto& profile{ settingOrigin.try_as<Model::Profile>() })
        {
            source = profile.Source();
            originTag = profile.Origin();
        }
        else if (const auto& appearanceConfig{ settingOrigin.try_as<Model::AppearanceConfig>() })
        {
            const auto profile = appearanceConfig.SourceProfile();
            source = profile.Source();
            originTag = profile.Origin();
        }

        // We will display arrows for all origins, and informative tooltips for Fragments and Generated
        if (originTag == Model::OriginTag::Fragment || originTag == Model::OriginTag::Generated)
        {
            // from a fragment extension or generated profile
            return hstring{ fmt::format(std::wstring_view{ RS_(L"SettingContainer_OverrideMessageFragmentExtension") }, source) };
        }
        return RS_(L"SettingContainer_OverrideMessageBaseLayer");
    }
}
