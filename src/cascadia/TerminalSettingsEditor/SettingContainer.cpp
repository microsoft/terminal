// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingContainer.h"
#include "SettingContainer.g.cpp"
#include "LibraryResources.h"
#include "../TerminalSettingsModel/LegacyProfileGeneratorNamespaces.h"

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

                // apply tooltip
                const auto& name{ RS_(L"SettingContainer_ResetButtonHelpText") };
                Controls::ToolTipService::SetToolTip(child, box_value(name));
                Automation::AutomationProperties::SetName(child, name);

                // apply name (automation property)
                // NOTE: this can only be set _once_. Automation clients do not detect any changes.
                //       As a result, we're using the more generic version of the override message.
                hstring overrideMsg{ fmt::format(std::wstring_view{ RS_(L"SettingContainer_OverrideIntro") }, RS_(L"SettingContainer_OverrideTarget")) };
                Automation::AutomationProperties::SetHelpText(child, overrideMsg);

                // initialize visibility for reset button
                button.Visibility(HasSettingValue() ? Visibility::Visible : Visibility::Collapsed);
            }
        }

        _UpdateOverrideMessage();

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

    void SettingContainer::_UpdateOverrideMessage()
    {
        if (const auto& overrideMsg{ GetTemplateChild(L"OverrideMessage") })
        {
            if (const auto& tb{ overrideMsg.try_as<Controls::TextBlock>() })
            {
                if (HasSettingValue())
                {
                    const std::wstring templateText{ RS_(L"SettingContainer_OverrideIntro") };
                    const auto pos{ templateText.find(L"{}") };
                    if (pos != std::wstring::npos)
                    {
                        // Append Message Prefix
                        {
                            Documents::Run msgPrefix{};
                            msgPrefix.Text(templateText.substr(0, pos));
                            tb.Inlines().Append(msgPrefix);
                        }

                        // Append Message Target
                        {
                            Documents::Run msgTarget{};
                            const auto& settingSrc{ SettingOverrideSource() };
                            if (!settingSrc)
                            {
                                // no source; provide system-default view
                                msgTarget.Text(RS_(L"SettingContainer_OverrideTargetSystemDefaults"));
                                tb.Inlines().Append(msgTarget);
                            }
                            else if (const auto& profile{ settingSrc.try_as<Model::Profile>() })
                            {
                                const auto originTag{ profile.Origin() };
                                if (originTag == Model::OriginTag::InBox)
                                {
                                    // in-box profile
                                    msgTarget.Text(profile.Name());
                                    tb.Inlines().Append(msgTarget);
                                }
                                else if (originTag == Model::OriginTag::Generated)
                                {
                                    // from a dynamic profile generator
                                    const auto& profileSource{ profile.Source() };
                                    if (profileSource == WslGeneratorNamespace)
                                    {
                                        msgTarget.Text(RS_(L"SettingContainer_OverrideTargetWSLGenerator"));
                                    }
                                    else if (profileSource == AzureGeneratorNamespace)
                                    {
                                        msgTarget.Text(RS_(L"SettingContainer_OverrideTargetPowershellGenerator"));
                                    }
                                    else if (profileSource == PowershellCoreGeneratorNamespace)
                                    {
                                        msgTarget.Text(RS_(L"SettingContainer_OverrideTargetAzureCloudShellGenerator"));
                                    }
                                    else
                                    {
                                        // TODO CARLOS: we should probably have some special handling for proto-extensions here
                                        // unknown generator; use fallback message instead
                                        msgTarget.Text(RS_(L"SettingContainer_OverrideTarget"));
                                    }
                                    tb.Inlines().Append(msgTarget);
                                }
                                else
                                {
                                    // base layer
                                    // TODO GH#3818: When we add profile inheritance as a setting,
                                    //               we'll need an extra conditional check to see if this
                                    //               is the base layer or some other profile
                                    msgTarget.Text(RS_(L"Nav_ProfileDefaults/Content"));

                                    Documents::Hyperlink hyperlink{};
                                    hyperlink.Click([=](auto&, auto&) {
                                        _NavigateHandlers(*this, settingSrc);
                                    });
                                    hyperlink.Inlines().Append(msgTarget);
                                    tb.Inlines().Append(hyperlink);
                                }
                            }
                            else
                            {
                                // unknown source; use fallback message instead
                                msgTarget.Text(RS_(L"SettingContainer_OverrideTarget"));
                                tb.Inlines().Append(msgTarget);
                            }
                        }

                        // Append Message Suffix
                        {
                            Documents::Run msgSuffix{};
                            msgSuffix.Text(templateText.substr(pos + 2, templateText.npos));
                            tb.Inlines().Append(msgSuffix);
                        }
                    }
                    else
                    {
                        // '{}' was not found
                        // fallback to more ambiguous version
                        hstring overrideMsg{ fmt::format(std::wstring_view{ RS_(L"SettingContainer_OverrideIntro") }, RS_(L"SettingContainer_OverrideTarget")) };
                        tb.Text(overrideMsg);
                    }
                    tb.Visibility(Visibility::Visible);
                }
                else
                {
                    tb.Visibility(Visibility::Collapsed);
                }
            }
        }
    }
}
