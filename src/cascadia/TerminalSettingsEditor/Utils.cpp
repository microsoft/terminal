// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "Utils.h"
#include <winrt/Microsoft.Terminal.Settings.Editor.h>

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings
{
    static void _appendChildren(const DependencyObject& node, std::vector<DependencyObject>& out)
    {
        const auto add = [&out](const auto& obj) {
            if (const auto child = obj.try_as<DependencyObject>())
            {
                out.push_back(child);
            }
        };

        if (const auto expander = node.try_as<Editor::SettingsExpander>())
        {
            add(expander.Content());
            add(expander.ItemsHeader());
            add(expander.ItemsFooter());
            if (const auto items = expander.Items())
            {
                for (const auto& item : items)
                {
                    add(item);
                }
            }
        }
        else if (const auto presenter = node.try_as<Controls::ContentPresenter>())
        {
            add(presenter.Content());
        }
        else if (const auto border = node.try_as<Controls::Border>())
        {
            add(border.Child());
        }
        else if (const auto panel = node.try_as<Controls::Panel>())
        {
            for (const auto& child : panel.Children())
            {
                add(child);
            }
        }

        const auto visualCount = Media::VisualTreeHelper::GetChildrenCount(node);
        for (int32_t i = 0; i < visualCount; ++i)
        {
            out.push_back(Media::VisualTreeHelper::GetChild(node, i));
        }
    }

    // Depth-first search of the logical tree under 'node' for 'target'. Returns true
    // once 'target' is found in 'node's subtree. On the way back up, any SettingsExpander
    // on the path to the target is expanded, so a target nested inside collapsed
    // expanders becomes visible. Short-circuits as soon as the target is found.
    static bool _expandAncestorsToReveal(const DependencyObject& node, const DependencyObject& target)
    {
        if (node == target)
        {
            return true;
        }

        std::vector<DependencyObject> children;
        _appendChildren(node, children);
        for (const auto& child : children)
        {
            if (_expandAncestorsToReveal(child, target))
            {
                if (const auto expander = node.try_as<Editor::SettingsExpander>())
                {
                    expander.IsExpanded(true);
                }
                return true;
            }
        }
        return false;
    }

    hstring GetSelectedItemTag(const winrt::Windows::Foundation::IInspectable& comboBoxAsInspectable)
    {
        auto comboBox = comboBoxAsInspectable.as<Controls::ComboBox>();
        auto selectedOption = comboBox.SelectedItem().as<Controls::ComboBoxItem>();

        return unbox_value<hstring>(selectedOption.Tag());
    }

    hstring LocalizedNameForEnumName(const std::wstring_view sectionAndEnumType, const std::wstring_view enumValue, const std::wstring_view propertyType)
    {
        // Uppercase the first letter to conform to our current Resource keys
        auto fmtKey = fmt::format(FMT_COMPILE(L"{}{}{}/{}"), sectionAndEnumType, static_cast<wchar_t>(std::towupper(enumValue[0])), enumValue.substr(1), propertyType);
        return GetLibraryResourceString(fmtKey);
    }

    // Returns the control that should actually receive focus for a resolved
    // search-navigation target.
    Controls::Control ResolveFocusTarget(const Controls::Control& element)
    {
        if (!element)
        {
            return element;
        }

        winrt::Windows::Foundation::IInspectable content{ nullptr };
        if (const auto expander = element.try_as<Editor::SettingsExpander>())
        {
            content = expander.Content();
        }
        else if (const auto card = element.try_as<Editor::SettingsCard>())
        {
            content = card.Content();
        }

        if (content)
        {
            if (const auto contentControl = content.try_as<Controls::Control>())
            {
                return contentControl;
            }
        }
        return element;
    }

    void ExpandAncestorsAndBringIntoView(const FrameworkElement& root, const Controls::Control& control)
    {
        if (!control)
        {
            return;
        }

        if (root)
        {
            _expandAncestorsToReveal(root, control);

            // Force a layout pass so the now-visible content is realized/measured
            // before we bring it into view.
            root.UpdateLayout();
        }

        control.StartBringIntoView();
        control.Focus(FocusState::Programmatic);
    }
}
