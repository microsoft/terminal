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
    DependencyProperty SettingContainer::_SettingOverrideSourceProperty{ nullptr };

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
        if (!_SettingOverrideSourceProperty)
        {
            _SettingOverrideSourceProperty =
                DependencyProperty::Register(
                    L"SettingOverrideSource",
                    xaml_typename<bool>(),
                    xaml_typename<Editor::SettingContainer>(),
                    PropertyMetadata{ nullptr });
        }
    }

    void SettingContainer::_OnHasSettingValueChanged(DependencyObject const& d, DependencyPropertyChangedEventArgs const& /*args*/)
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
                    hstring tooltip{};

                    const auto& settingSrc{ SettingOverrideSource() };
                    if (const auto& profile{ settingSrc.try_as<Model::Profile>() })
                    {
                        tooltip = _GenerateOverrideMessage(profile);
                    }
                    else if (const auto& appearanceConfig{ settingSrc.try_as<Model::AppearanceConfig>() })
                    {
                        tooltip = _GenerateOverrideMessage(appearanceConfig.SourceProfile());
                    }

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
    hstring SettingContainer::_GenerateOverrideMessage(const Model::Profile& profile)
    {
        const auto originTag{ profile.Origin() };
        if (originTag == Model::OriginTag::InBox)
        {
            // in-box profile
            return {};
        }
        else if (originTag == Model::OriginTag::Generated)
        {
            // from a dynamic profile generator
            return {};
        }
        else if (originTag == Model::OriginTag::Fragment)
        {
            // from a fragment extension
            return hstring{ fmt::format(std::wstring_view{ RS_(L"SettingContainer_OverrideMessageFragmentExtension") }, profile.Source()) };
        }
        else
        {
            // base layer
            // TODO GH#3818: When we add profile inheritance as a setting,
            //               we'll need an extra conditional check to see if this
            //               is the base layer or some other profile

            // GH#9539: Base Layer has been removed from the Settings UI.
            //          In the event that the Base Layer comes back,
            //          return RS_(L"SettingContainer_OverrideMessageBaseLayer") instead
            return {};
        }
    }
}
