// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingResetButton.h"
#include "SettingResetButton.g.cpp"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Automation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty SettingResetButton::_TargetProperty{ nullptr };
    DependencyProperty SettingResetButton::_SettingNameProperty{ nullptr };

    static constexpr std::wstring_view EraserGlyph{ L"\uE75C" };

    SettingResetButton::SettingResetButton()
    {
        _InitializeProperties();

        // Build the eraser icon content. We rely on the default Button template
        // (via the implicit style's BasedOn) for the rest of the visuals.
        FontIcon icon{};
        icon.Glyph(winrt::hstring{ EraserGlyph });
        icon.FontSize(14.0);
        Content(icon);

        // Self-owned Click handler. Use a weak ref to avoid a reference cycle.
        Click([weakThis{ get_weak() }](const IInspectable& sender, const RoutedEventArgs& args) {
            if (const auto self{ weakThis.get() })
            {
                self->_OnClick(sender, args);
            }
        });

        _Update();
    }

    void SettingResetButton::_InitializeProperties()
    {
        if (!_TargetProperty)
        {
            _TargetProperty = DependencyProperty::Register(
                L"Target",
                xaml_typename<Editor::IInheritableViewModel>(),
                xaml_typename<Editor::SettingResetButton>(),
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &SettingResetButton::_OnTargetChanged } });
        }
        if (!_SettingNameProperty)
        {
            _SettingNameProperty = DependencyProperty::Register(
                L"SettingName",
                xaml_typename<hstring>(),
                xaml_typename<Editor::SettingResetButton>(),
                PropertyMetadata{ box_value(L""), PropertyChangedCallback{ &SettingResetButton::_OnSettingNameChanged } });
        }
    }

    void SettingResetButton::_OnTargetChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto& obj{ d.try_as<Editor::SettingResetButton>() };
        const auto self{ get_self<SettingResetButton>(obj) };
        self->_ResubscribeToTarget();
        self->_Update();
    }

    void SettingResetButton::_OnSettingNameChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        const auto& obj{ d.try_as<Editor::SettingResetButton>() };
        get_self<SettingResetButton>(obj)->_Update();
    }

    // Subscribe to the Target's PropertyChanged so we can keep our enabled state and
    // tooltip in sync when the underlying setting (or its inheritance) changes.
    void SettingResetButton::_ResubscribeToTarget()
    {
        if (_subscribedTarget)
        {
            _subscribedTarget.PropertyChanged(_targetPropertyChangedToken);
            _subscribedTarget = nullptr;
            _targetPropertyChangedToken = {};
        }

        if (const auto target{ Target() })
        {
            if (const auto inpc{ target.try_as<INotifyPropertyChanged>() })
            {
                _subscribedTarget = inpc;
                _targetPropertyChangedToken = inpc.PropertyChanged({ get_weak(), &SettingResetButton::_OnTargetPropertyChanged });
            }
        }
    }

    void SettingResetButton::_OnTargetPropertyChanged(const IInspectable& /*sender*/, const PropertyChangedEventArgs& args)
    {
        const auto changed{ args.PropertyName() };
        const auto name{ SettingName() };
        const auto hasName{ hstring{ L"Has" } + name };
        // A bulk-refresh (empty name) or a change to either the value or its
        // "Has<Setting>" flag should refresh our state.
        if (changed.empty() || changed == name || changed == hasName)
        {
            _Update();
        }
    }

    void SettingResetButton::_Update()
    {
        const auto target{ Target() };
        const auto name{ SettingName() };
        if (!target || name.empty())
        {
            IsEnabled(false);
            return;
        }

        // The button is enabled only when the setting has a value at this layer.
        IsEnabled(target.HasSetting(name));

        // Tooltip + accessible name describe what resetting will do.
        const auto message{ _GenerateOverrideMessage(target.SettingOverrideSource(name)) };
        ToolTipService::SetToolTip(*this, box_value(message));
        AutomationProperties::SetName(*this, message);
    }

    void SettingResetButton::_OnClick(const IInspectable& /*sender*/, const RoutedEventArgs& /*args*/)
    {
        const auto target{ Target() };
        const auto name{ SettingName() };
        if (target && !name.empty())
        {
            target.ClearSetting(name);
        }
    }

    // Mirrors the legacy SettingContainer override message: base-layer profiles get a
    // generic "reset to inherited value" message, while fragment/generated sources get a
    // message naming the source they would revert to.
    hstring SettingResetButton::_GenerateOverrideMessage(const IInspectable& settingOrigin)
    {
        auto originTag{ OriginTag::None };
        winrt::hstring source;

        if (const auto& profile{ settingOrigin.try_as<Profile>() })
        {
            source = profile.Source();
            originTag = profile.Origin();
        }
        else if (const auto& appearanceConfig{ settingOrigin.try_as<AppearanceConfig>() })
        {
            const auto profile{ appearanceConfig.SourceProfile() };
            source = profile.Source();
            originTag = profile.Origin();
        }

        if (originTag == OriginTag::Fragment || originTag == OriginTag::Generated)
        {
            return hstring{ RS_fmt(L"SettingContainer_OverrideMessageFragmentExtension", source) };
        }
        return RS_(L"SettingContainer_OverrideMessageBaseLayer");
    }
}
