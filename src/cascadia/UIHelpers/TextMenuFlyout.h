#pragma once

#include "TextMenuFlyout.g.h"

namespace winrt::Microsoft::Terminal::UI::implementation
{
    struct TextMenuFlyout : TextMenuFlyoutT<TextMenuFlyout>
    {
        TextMenuFlyout();

        void MenuFlyout_Opening(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::Foundation::IInspectable const& e);
        void Cut_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Copy_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Paste_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void SelectAll_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

    private:
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItemBase _createMenuItem(
            winrt::Windows::UI::Xaml::Controls::Symbol symbol,
            winrt::hstring text,
            winrt::Windows::UI::Xaml::RoutedEventHandler click,
            winrt::Windows::System::VirtualKeyModifiers modifiers,
            winrt::Windows::System::VirtualKey key);

        // These are always present.
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItemBase _copy{ nullptr };
        // These are only set for writable controls.
        winrt::Windows::UI::Xaml::Controls::MenuFlyoutItemBase _cut{ nullptr };
    };
}

namespace winrt::Microsoft::Terminal::UI::factory_implementation
{
    BASIC_FACTORY(TextMenuFlyout);
}
