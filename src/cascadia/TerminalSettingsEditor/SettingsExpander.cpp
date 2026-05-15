// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsExpander.h"
#include "SettingsExpander.g.cpp"
#include "SettingsExpanderAutomationPeer.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Automation;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;
using namespace winrt::Windows::UI::Xaml::Controls;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty SettingsExpander::_HeaderProperty{ nullptr };
    DependencyProperty SettingsExpander::_DescriptionProperty{ nullptr };
    DependencyProperty SettingsExpander::_HeaderIconProperty{ nullptr };
    DependencyProperty SettingsExpander::_ContentProperty{ nullptr };
    DependencyProperty SettingsExpander::_IsExpandedProperty{ nullptr };
    DependencyProperty SettingsExpander::_ItemsHeaderProperty{ nullptr };
    DependencyProperty SettingsExpander::_ItemsFooterProperty{ nullptr };

    SettingsExpander::SettingsExpander()
    {
        _InitializeProperties();
    }

    void SettingsExpander::_InitializeProperties()
    {
        if (!_HeaderProperty)
        {
            _HeaderProperty = DependencyProperty::Register(
                L"Header",
                xaml_typename<IInspectable>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr });
        }
        if (!_DescriptionProperty)
        {
            _DescriptionProperty = DependencyProperty::Register(
                L"Description",
                xaml_typename<IInspectable>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr });
        }
        if (!_HeaderIconProperty)
        {
            _HeaderIconProperty = DependencyProperty::Register(
                L"HeaderIcon",
                xaml_typename<IconElement>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr });
        }
        if (!_ContentProperty)
        {
            _ContentProperty = DependencyProperty::Register(
                L"Content",
                xaml_typename<IInspectable>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr });
        }
        if (!_IsExpandedProperty)
        {
            _IsExpandedProperty = DependencyProperty::Register(
                L"IsExpanded",
                xaml_typename<bool>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ box_value(false), PropertyChangedCallback{ &SettingsExpander::_OnIsExpandedChanged } });
        }
        if (!_ItemsHeaderProperty)
        {
            _ItemsHeaderProperty = DependencyProperty::Register(
                L"ItemsHeader",
                xaml_typename<UIElement>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr });
        }
        if (!_ItemsFooterProperty)
        {
            _ItemsFooterProperty = DependencyProperty::Register(
                L"ItemsFooter",
                xaml_typename<UIElement>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr });
        }
    }

    AutomationPeer SettingsExpander::OnCreateAutomationPeer()
    {
        return winrt::make<implementation::SettingsExpanderAutomationPeer>(*this);
    }

    void SettingsExpander::OnApplyTemplate()
    {
        // No template-part lookups required: our template uses regular bindings
        // through TemplateBinding / x:Bind. The DPs do their own work.
    }

    void SettingsExpander::_OnIsExpandedChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& e)
    {
        const auto obj{ d.try_as<Editor::SettingsExpander>() };
        if (!obj)
        {
            return;
        }
        const auto self = get_self<SettingsExpander>(obj);
        const auto newValue = unbox_value_or<bool>(e.NewValue(), false);
        if (newValue)
        {
            self->Expanded.raise(obj, nullptr);
        }
        else
        {
            self->Collapsed.raise(obj, nullptr);
        }
    }

    SettingsExpanderAutomationPeer::SettingsExpanderAutomationPeer(const Editor::SettingsExpander& owner) :
        SettingsExpanderAutomationPeerT<SettingsExpanderAutomationPeer>(owner)
    {
    }

    AutomationControlType SettingsExpanderAutomationPeer::GetAutomationControlTypeCore() const
    {
        return AutomationControlType::Group;
    }

    hstring SettingsExpanderAutomationPeer::GetClassNameCore() const
    {
        return hstring{ L"SettingsExpander" };
    }

    hstring SettingsExpanderAutomationPeer::GetNameCore() const
    {
        if (const auto expander{ Owner().try_as<Editor::SettingsExpander>() })
        {
            if (const auto manualName{ AutomationProperties::GetName(expander) }; !manualName.empty())
            {
                return manualName;
            }
            if (const auto headerString{ unbox_value_or<hstring>(expander.Header(), hstring{}) }; !headerString.empty())
            {
                return headerString;
            }
        }
        return {};
    }
}
