// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "Utils.h"
#include <winrt/Microsoft.Terminal.Settings.Editor.h>
#include <unordered_set>

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings
{
    // Method Description:
    // - Depth-first search of the logical+visual tree under "node" for "target". We must walk
    //   both because logical properties (i.e. SettingsExpander.Items) may hold the target before its
    //   template/visual tree is realized, so VisualTreeHelper alone can miss it. On the way
    //   back up, any SettingsExpander on the path to the target is expanded, so a target
    //   nested inside collapsed expanders becomes visible. Short-circuits as soon as the
    //   target is found.
    // - "visited" dedupes nodes reachable through both the logical tree (i.e. SettingsExpander/
    //   ContentPresenter/Border/Panel children) and the visual tree (VisualTreeHelper)
    //   so we don't re-scan the same subtree more than once.
    // Arguments:
    // - node - the root of the subtree to search
    // - target - the control we're looking for
    // - visited - set of visited nodes, shared across the whole recursive search
    // - expandedAny - set to true if an ancestor SettingsExpander was flipped from collapsed to
    //                 expanded, so callers know whether to wait for its expand animation to finish
    // Return Value:
    // - true if "target" was found in node's subtree, false otherwise
    static bool _expandAncestorsToReveal(const DependencyObject& node, const DependencyObject& target, std::unordered_set<::IInspectable*>& visited, bool& expandedAny)
    {
        if (node == target)
        {
            return true;
        }

        // Use winrt::get_abi() over `.as<IInspectable>()` cast to avoid addref/release
        const auto identity = static_cast<::IInspectable*>(winrt::get_abi(node));
        if (!visited.insert(identity).second)
        {
            // Already processed via another path
            return false;
        }

        const auto recurse = [&](const auto& obj) {
            if (const auto child = obj.try_as<DependencyObject>())
            {
                return _expandAncestorsToReveal(child, target, visited, expandedAny);
            }
            return false;
        };

        auto found = false;
        if (const auto expander = node.try_as<Editor::SettingsExpander>())
        {
            found = recurse(expander.Content()) || recurse(expander.ItemsHeader()) || recurse(expander.ItemsFooter());
            if (!found)
            {
                if (const auto items = expander.Items())
                {
                    for (const auto& item : items)
                    {
                        if (recurse(item))
                        {
                            found = true;
                            break;
                        }
                    }
                }
            }
        }
        else if (const auto presenter = node.try_as<Controls::ContentPresenter>())
        {
            found = recurse(presenter.Content());
        }
        else if (const auto border = node.try_as<Controls::Border>())
        {
            found = recurse(border.Child());
        }
        else if (const auto panel = node.try_as<Controls::Panel>())
        {
            for (const auto& child : panel.Children())
            {
                if (recurse(child))
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            const auto visualCount = Media::VisualTreeHelper::GetChildrenCount(node);
            for (int32_t i = 0; i < visualCount; ++i)
            {
                if (recurse(Media::VisualTreeHelper::GetChild(node, i)))
                {
                    found = true;
                    break;
                }
            }
        }

        if (found)
        {
            if (const auto expander = node.try_as<Editor::SettingsExpander>())
            {
                if (!expander.IsExpanded())
                {
                    expander.IsExpanded(true);
                    expandedAny = true;
                }
            }
        }
        return found;
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

    safe_void_coroutine ExpandAncestorsAndBringIntoView(FrameworkElement root, Controls::Control control)
    {
        if (!control)
        {
            co_return;
        }

        if (root)
        {
            std::unordered_set<::IInspectable*> visited;
            auto expandedAny = false;
            _expandAncestorsToReveal(root, control, visited, expandedAny);

            // Force a layout pass so the now-visible content is realized/measured
            // before we bring it into view.
            root.UpdateLayout();

            if (expandedAny)
            {
                // SettingsExpander's expand animation slides its content in via a RenderTransform
                // over 0:0:0.333 (see SettingsControlsStyle.xaml). UpdateLayout()
                // above already reflects the final layout size, but StartBringIntoView() below
                // measures the current (still-animating) visual position, so calling it too early
                // undershoots the scroll target. Wait out the animation first.
                const auto dispatcher = control.Dispatcher();
                co_await winrt::resume_after(std::chrono::milliseconds{ 333 });
                co_await wil::resume_foreground(dispatcher);
            }
        }

        control.StartBringIntoView();
        control.Focus(FocusState::Programmatic);
    }

    // Depth-first search of the visual tree under 'root' for the first KeyChordListener.
    Editor::KeyChordListener FindKeyChordListener(const DependencyObject& root)
    {
        if (!root)
        {
            return nullptr;
        }
        if (const auto listener = root.try_as<Editor::KeyChordListener>())
        {
            return listener;
        }
        const auto count = Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            const auto child = Media::VisualTreeHelper::GetChild(root, i);
            if (const auto found = FindKeyChordListener(child))
            {
                return found;
            }
        }
        return nullptr;
    }

    // Depth-first search of the visual tree under 'root' for the first focusable, visible
    // control (e.g. a key chord row's edit pencil), used to restore focus to a row after it
    // leaves edit mode.
    Controls::Control FindFirstFocusable(const DependencyObject& root)
    {
        if (!root)
        {
            return nullptr;
        }
        if (const auto control = root.try_as<Controls::Control>())
        {
            if (control.IsTabStop() && control.IsEnabled() && control.Visibility() == Visibility::Visible)
            {
                return control;
            }
        }
        const auto count = Media::VisualTreeHelper::GetChildrenCount(root);
        for (int32_t i = 0; i < count; ++i)
        {
            const auto child = Media::VisualTreeHelper::GetChild(root, i);
            if (const auto found = FindFirstFocusable(child))
            {
                return found;
            }
        }
        return nullptr;
    }
}
