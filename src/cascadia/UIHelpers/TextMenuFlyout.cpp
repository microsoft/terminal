// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TextMenuFlyout.h"

#include <LibraryResources.h>

#include "TextMenuFlyout.g.cpp"

using namespace ::winrt::Windows::UI::Xaml;
using namespace ::winrt::Windows::UI::Xaml::Controls;
using namespace ::winrt::Windows::ApplicationModel::Resources::Core;
using namespace ::winrt::Microsoft::UI::Xaml::Controls;
using namespace ::winrt::Windows::System;
using namespace ::winrt::Windows::UI::Xaml::Input;

using MenuFlyoutItemClick = void (*)(IInspectable const&, RoutedEventArgs const&);

constexpr auto NullSymbol = static_cast<Symbol>(0);

namespace winrt::Microsoft::Terminal::UI::implementation
{
#pragma warning(suppress : 26455) // Default constructor should not throw. Declare it 'noexcept' (f.6).
    TextMenuFlyout::TextMenuFlyout()
    {
        // Most of the initialization is delayed until the first call to MenuFlyout_Opening.
        Opening({ this, &TextMenuFlyout::MenuFlyout_Opening });
    }

    void TextMenuFlyout::MenuFlyout_Opening(IInspectable const&, IInspectable const&)
    {
        auto target = Target();
        if (!target)
        {
            return;
        }
        hstring selection;
        bool writable = false;

        // > Common group of selectable controls with common actions
        // > The I in MIDL stands for...
        // No common interface.
        if (const auto box = target.try_as<NumberBox>())
        {
            // Accessing template children from outside the class is unspecified; GetTemplateChild is
            // a protected member. It does work, though.
            target = box.as<IControlProtected>().GetTemplateChild(L"InputBox").as<TextBox>();
        }
        if (const auto control = target.try_as<TextBlock>())
        {
            selection = control.SelectedText();
        }
        else if (const auto control = target.try_as<TextBox>())
        {
            selection = control.SelectedText();
            writable = true;
        }
        else if (const auto control = target.try_as<RichTextBlock>())
        {
            selection = control.SelectedText();
        }

        if (!_copy)
        {
            til::small_vector<MenuFlyoutItemBase, 4> items;

            if (writable)
            {
                _cut = items.emplace_back(_createMenuItem(Symbol::Cut, RS_(L"Cut"), { this, &TextMenuFlyout::Cut_Click }, VirtualKeyModifiers::Control, VirtualKey::X));
            }
            _copy = items.emplace_back(_createMenuItem(Symbol::Copy, RS_(L"Copy"), { this, &TextMenuFlyout::Copy_Click }, VirtualKeyModifiers::Control, VirtualKey::C));
            if (writable)
            {
                items.emplace_back(_createMenuItem(Symbol::Paste, RS_(L"Paste"), { this, &TextMenuFlyout::Paste_Click }, VirtualKeyModifiers::Control, VirtualKey::V));
            }
            items.emplace_back(_createMenuItem(NullSymbol, RS_(L"SelectAll"), { this, &TextMenuFlyout::SelectAll_Click }, VirtualKeyModifiers::Control, VirtualKey::A));

            Items().ReplaceAll({ items.data(), gsl::narrow_cast<uint32_t>(items.size()) });
        }

        const auto visibilityOfItemsRequiringSelection = selection.empty() ? Visibility::Collapsed : Visibility::Visible;
        if (_cut)
        {
            _cut.Visibility(visibilityOfItemsRequiringSelection);
        }
        _copy.Visibility(visibilityOfItemsRequiringSelection);
    }

    void TextMenuFlyout::Cut_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // NOTE: When the flyout closes, WinUI doesn't disconnect the accelerator keys.
        // Since that means we'll get Ctrl+X/C/V callbacks forever, just ignore them.
        // The TextBox will still handle those events...
        auto target = Target();
        if (!target)
        {
            return;
        }

        if (const auto box = target.try_as<NumberBox>())
        {
            target = box.as<IControlProtected>().GetTemplateChild(L"InputBox").as<TextBox>();
        }
        if (const auto control = target.try_as<TextBox>())
        {
            control.CutSelectionToClipboard();
        }
    }

    void TextMenuFlyout::Copy_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto target = Target();
        if (!target)
        {
            return;
        }

        if (const auto box = target.try_as<NumberBox>())
        {
            target = box.as<IControlProtected>().GetTemplateChild(L"InputBox").as<TextBox>();
        }
        if (const auto control = target.try_as<TextBlock>())
        {
            control.CopySelectionToClipboard();
        }
        else if (const auto control = target.try_as<TextBox>())
        {
            control.CopySelectionToClipboard();
        }
        else if (const auto control = target.try_as<RichTextBlock>())
        {
            control.CopySelectionToClipboard();
        }
    }

    void TextMenuFlyout::Paste_Click(IInspectable const&, RoutedEventArgs const&)
    {
        auto target = Target();
        if (!target)
        {
            return;
        }

        if (const auto box = target.try_as<NumberBox>())
        {
            target = box.as<IControlProtected>().GetTemplateChild(L"InputBox").as<TextBox>();
        }
        if (const auto control = target.try_as<TextBox>())
        {
            control.PasteFromClipboard();
        }
    }

    void TextMenuFlyout::SelectAll_Click(IInspectable const&, RoutedEventArgs const&)
    {
        // BODGY:
        // Once the flyout was open once, we'll get Ctrl+A events and the TextBox will
        // ignore them. As such, we have to dig out the focused element as a fallback,
        // because otherwise Ctrl+A will be permanently broken. Put differently,
        // this is bodgy because WinUI 2.8 is buggy. There's no other solution here.
        IInspectable target = Target();
        if (!target)
        {
            target = FocusManager::GetFocusedElement(XamlRoot());
            if (!target)
            {
                return;
            }
        }

        if (const auto box = target.try_as<NumberBox>())
        {
            target = box.as<IControlProtected>().GetTemplateChild(L"InputBox").as<TextBox>();
        }
        if (const auto control = target.try_as<TextBlock>())
        {
            control.SelectAll();
        }
        else if (const auto control = target.try_as<TextBox>())
        {
            control.SelectAll();
        }
        else if (const auto control = target.try_as<RichTextBlock>())
        {
            control.SelectAll();
        }
    }

    MenuFlyoutItemBase TextMenuFlyout::_createMenuItem(Symbol symbol, hstring text, RoutedEventHandler click, VirtualKeyModifiers modifiers, VirtualKey key)
    {
        KeyboardAccelerator accel;
        accel.Modifiers(modifiers);
        accel.Key(key);

        MenuFlyoutItem item;
        if (symbol != NullSymbol)
        {
            item.Icon(SymbolIcon{ std::move(symbol) });
        }
        item.Text(std::move(text));
        item.Click(std::move(click));
        item.KeyboardAccelerators().Append(std::move(accel));
        return item;
    }
}
