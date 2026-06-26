// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsExpander.h"
#include "SettingsExpander.g.cpp"
#include "SettingsExpanderAutomationPeer.g.cpp"
#include "SettingsExpanderItemStyleSelector.g.cpp"
#include "SettingsControlsHelpers.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
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
    DependencyProperty SettingsExpander::_ItemsProperty{ nullptr };
    DependencyProperty SettingsExpander::_ItemsSourceProperty{ nullptr };
    DependencyProperty SettingsExpander::_ItemTemplateProperty{ nullptr };
    DependencyProperty SettingsExpander::_ItemContainerStyleSelectorProperty{ nullptr };

    static constexpr std::wstring_view PART_ItemsHost{ L"PART_ItemsHost" };

    SettingsExpander::SettingsExpander()
    {
        _InitializeProperties();

        // Items is backed by an observable vector so post-construction mutations
        // (e.g. user code that adds cards after the expander is on screen) also
        // refresh the inner ItemsControl. The XAML parser populates Items via
        // Append before OnApplyTemplate runs, so the eager population path also
        // works.
        Items(single_threaded_observable_vector<IInspectable>());
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
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &SettingsExpander::_OnDescriptionChanged } });
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
        if (!_ItemsProperty)
        {
            _ItemsProperty = DependencyProperty::Register(
                L"Items",
                xaml_typename<IVector<IInspectable>>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &SettingsExpander::_OnItemsConnectedPropertyChanged } });
        }
        if (!_ItemsSourceProperty)
        {
            _ItemsSourceProperty = DependencyProperty::Register(
                L"ItemsSource",
                xaml_typename<IInspectable>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr, PropertyChangedCallback{ &SettingsExpander::_OnItemsConnectedPropertyChanged } });
        }
        if (!_ItemTemplateProperty)
        {
            _ItemTemplateProperty = DependencyProperty::Register(
                L"ItemTemplate",
                xaml_typename<IInspectable>(),
                xaml_typename<Editor::SettingsExpander>(),
                PropertyMetadata{ nullptr });
        }
        if (!_ItemContainerStyleSelectorProperty)
        {
            _ItemContainerStyleSelectorProperty = DependencyProperty::Register(
                L"ItemContainerStyleSelector",
                xaml_typename<StyleSelector>(),
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
        // Same implicit-styles injection as SettingsCard so a ToggleSwitch /
        // Slider / ComboBox / TextBox placed directly as SettingsExpander.Content
        // gets the same Windows 11 defaults.
        StyleExtensions::EnsureImplicitStylesMergedInto(*this);

        _SetAccessibleName();
        _UpdateFullDescription();

        // Drop the prior template's host before locating the new one.
        _itemsHost = nullptr;

        if (const auto child{ GetTemplateChild(hstring{ PART_ItemsHost }) })
        {
            _itemsHost = child.try_as<Controls::ItemsControl>();
        }

        if (_itemsHost)
        {
            // Push our initial ItemsSource through to the host and stamp item-container styles.
            _UpdateItemsSource();
        }
    }

    void SettingsExpander::_SetAccessibleName()
    {
        if (!AutomationProperties::GetName(*this).empty())
        {
            return;
        }
        if (const auto headerString{ unbox_value_or<hstring>(Header(), hstring{}) }; !headerString.empty())
        {
            AutomationProperties::SetName(*this, headerString);
        }
    }

    // DEVIATION FROM WCT: Expose the Description via FullDescription
    void SettingsExpander::_UpdateFullDescription()
    {
        AutomationProperties::SetFullDescription(*this, unbox_value_or<hstring>(Description(), hstring{}));
    }

    void SettingsExpander::_OnDescriptionChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        if (const auto obj{ d.try_as<Editor::SettingsExpander>() })
        {
            get_self<SettingsExpander>(obj)->_UpdateFullDescription();
        }
    }

    void SettingsExpander::_UpdateItemsSource()
    {
        if (!_itemsHost)
        {
            return;
        }
        // ItemsSource wins when set; otherwise fall back to the inline Items collection.
        if (const auto source{ ItemsSource() })
        {
            _itemsHost.ItemsSource(source);
        }
        else
        {
            _itemsHost.ItemsSource(Items());
        }

        _ApplyItemContainerStyles();
        _SubscribeToItemsVectorChanged();
    }

    // Watch our inline Items vector for changes so containers added after
    // OnApplyTemplate also get the proper SettingsCard item style. (When
    // ItemsSource is set, this is a no-op since the parser only touches Items.)
    void SettingsExpander::_SubscribeToItemsVectorChanged()
    {
        _itemsVectorChangedRevoker.revoke();

        if (ItemsSource())
        {
            return;
        }

        if (const auto observable{ Items().try_as<IObservableVector<IInspectable>>() })
        {
            _itemsVectorChangedRevoker = observable.VectorChanged(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
                if (const auto strongThis{ weakThis.get() })
                {
                    strongThis->_ApplyItemContainerStyles();
                }
            });
        }
    }

    // Apply the per-item style produced by ItemContainerStyleSelector. ItemsControl
    // only generates ContentPresenter containers for non-UIElement items, so when
    // SettingsCards are added directly the cards themselves are the "containers"
    // and we have to set Style on them ourselves. Mirrors the ElementPrepared path
    // we used when this was an ItemsRepeater.
    void SettingsExpander::_ApplyItemContainerStyles()
    {
        const auto selector{ ItemContainerStyleSelector() };
        if (!selector)
        {
            return;
        }

        const auto items{ Items() };
        if (!items)
        {
            return;
        }

        for (uint32_t i = 0; i < items.Size(); ++i)
        {
            if (const auto element{ items.GetAt(i).try_as<FrameworkElement>() })
            {
                if (element.ReadLocalValue(FrameworkElement::StyleProperty()) == DependencyProperty::UnsetValue())
                {
                    element.Style(selector.SelectStyle(items.GetAt(i), element));
                }
            }
        }
    }

    void SettingsExpander::_OnItemsConnectedPropertyChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        if (const auto obj{ d.try_as<Editor::SettingsExpander>() })
        {
            get_self<SettingsExpander>(obj)->_UpdateItemsSource();
        }
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

        // Notify the automation peer so screen readers see the expand/collapse state change.
        if (const auto peer{ FrameworkElementAutomationPeer::FromElement(obj).try_as<Editor::SettingsExpanderAutomationPeer>() })
        {
            peer.RaiseExpandedChangedEvent(newValue);
        }

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

    void SettingsExpanderAutomationPeer::RaiseExpandedChangedEvent(bool newValue)
    {
        // Mirrors the toolkit's SettingsExpanderAutomationPeer.RaiseExpandedChangedEvent.
        // Narrator doesn't actually announce this today (microsoft/microsoft-ui-xaml#3469),
        // but other AT can subscribe and we keep parity with the toolkit.
        const auto newState = newValue ? ExpandCollapseState::Expanded : ExpandCollapseState::Collapsed;
        const auto oldState = newValue ? ExpandCollapseState::Collapsed : ExpandCollapseState::Expanded;
        RaisePropertyChangedEvent(ExpandCollapsePatternIdentifiers::ExpandCollapseStateProperty(), box_value(oldState), box_value(newState));
    }

    Windows::UI::Xaml::Style SettingsExpanderItemStyleSelector::SelectStyleCore(const winrt::Windows::Foundation::IInspectable& /*item*/, const Windows::UI::Xaml::DependencyObject& container)
    {
        // When the prepared container is a clickable SettingsCard, give it the
        // clickable item style (which adds the right padding for the chevron).
        // Otherwise fall back to the default item style.
        if (const auto card{ container.try_as<Editor::SettingsCard>() })
        {
            if (card.IsClickEnabled())
            {
                return _ClickableStyle;
            }
        }
        return _DefaultStyle;
    }
}
