// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "SettingsCard.h"
#include "SettingsCard.g.cpp"
#include "LibraryResources.h"

using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    DependencyProperty SettingsCard::_HeaderProperty{ nullptr };
    DependencyProperty SettingsCard::_DescriptionProperty{ nullptr };
    DependencyProperty SettingsCard::_HeaderIconProperty{ nullptr };
    DependencyProperty SettingsCard::_ActionIconProperty{ nullptr };
    DependencyProperty SettingsCard::_ActionIconToolTipProperty{ nullptr };
    DependencyProperty SettingsCard::_IsClickEnabledProperty{ nullptr };
    DependencyProperty SettingsCard::_ContentAlignmentProperty{ nullptr };
    DependencyProperty SettingsCard::_IsActionIconVisibleProperty{ nullptr };

    SettingsCard::SettingsCard()
    {
        _InitializeProperties();
    }

    void SettingsCard::_InitializeProperties()
    {
        // Initialize any dependency properties here.
        // This performs a lazy load on these properties, instead of
        // initializing them when the DLL loads.
        if (!_HeaderProperty)
        {
            _HeaderProperty =
                DependencyProperty::Register(
                    L"Header",
                    xaml_typename<IInspectable>(),
                    xaml_typename<Editor::SettingsCard>(),
                    PropertyMetadata{ nullptr,
                                      PropertyChangedCallback{ [](auto&& d, auto&&) {
                                          const auto& obj = d.as<Editor::SettingsCard>();
                                          get_self<SettingsCard>(obj)->_OnHeaderChanged();
                                      } } });
        }
        if (!_DescriptionProperty)
        {
            _DescriptionProperty =
                DependencyProperty::Register(
                    L"Description",
                    xaml_typename<IInspectable>(),
                    xaml_typename<Editor::SettingsCard>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ [](auto&& d, auto&&) {
                                          const auto& obj = d.as<Editor::SettingsCard>();
                                          get_self<SettingsCard>(obj)->_OnDescriptionChanged();
                                      } } });
        }
        if (!_HeaderIconProperty)
        {
            _HeaderIconProperty =
                DependencyProperty::Register(
                    L"HeaderIcon",
                    xaml_typename<Windows::UI::Xaml::Controls::IconElement>(),
                    xaml_typename<Editor::SettingsCard>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ [](auto&& d, auto&&) {
                                          const auto& obj = d.as<Editor::SettingsCard>();
                                          get_self<SettingsCard>(obj)->_OnHeaderIconChanged();
                                      } } });
        }
        if (!_ActionIconProperty)
        {
            _ActionIconProperty =
                DependencyProperty::Register(
                    L"ActionIcon",
                    xaml_typename<Windows::UI::Xaml::Controls::IconElement>(),
                    xaml_typename<Editor::SettingsCard>(),
                    PropertyMetadata{ box_value(L"\ue974") });
        }
        if (!_ActionIconToolTipProperty)
        {
            _ActionIconToolTipProperty =
                DependencyProperty::Register(
                    L"ActionIconToolTip",
                    xaml_typename<hstring>(),
                    xaml_typename<Editor::SettingsCard>(),
                    PropertyMetadata{ nullptr });
        }
        if (!_IsClickEnabledProperty)
        {
            _IsClickEnabledProperty =
                DependencyProperty::Register(
                    L"IsClickEnabled",
                    xaml_typename<bool>(),
                    xaml_typename<Editor::SettingsCard>(),
                    PropertyMetadata{ box_value(false), PropertyChangedCallback{ [](auto&& d, auto&&) {
                                          const auto& obj = d.as<Editor::SettingsCard>();
                                          get_self<SettingsCard>(obj)->_OnIsClickEnabledChanged();
                                      } } });
        }
        if (!_ContentAlignmentProperty)
        {
            _ContentAlignmentProperty =
                DependencyProperty::Register(
                    L"ContentAlignment",
                    xaml_typename<Editor::ContentAlignment>(),
                    xaml_typename<Editor::SettingsCard>(),
                    PropertyMetadata{ box_value(Editor::ContentAlignment::Right) });
        }
        if (!_IsActionIconVisibleProperty)
        {
            _IsActionIconVisibleProperty =
                DependencyProperty::Register(
                    L"IsActionIconVisible",
                    xaml_typename<bool>(),
                    xaml_typename<Editor::SettingsCard>(),
                    PropertyMetadata{ box_value(true), PropertyChangedCallback{ [](auto&& d, auto&&) {
                                          const auto& obj = d.as<Editor::SettingsCard>();
                                          get_self<SettingsCard>(obj)->_OnIsActionIconVisibleChanged();
                                      } } });
        }
    }

    // TODO CARLOS: port these functions over
    void SettingsCard::_OnHeaderChanged()
    {
    }

    void SettingsCard::_OnDescriptionChanged()
    {
    }

    void SettingsCard::_OnHeaderIconChanged()
    {
    }

    void SettingsCard::_OnIsClickEnabledChanged()
    {
    }

    void SettingsCard::_OnIsActionIconVisibleChanged()
    {
    }
}
