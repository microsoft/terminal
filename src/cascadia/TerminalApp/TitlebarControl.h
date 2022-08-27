// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TitlebarControl.g.h"

namespace winrt::TerminalApp::implementation
{
    struct TitlebarControl : TitlebarControlT<TitlebarControl>
    {
        TitlebarControl(uint64_t handle);

        void HoverButton(CaptionButton button);
        void PressButton(CaptionButton button);
        winrt::fire_and_forget ClickButton(CaptionButton button);
        void ReleaseButtons();
        double CaptionButtonWidth();

        IInspectable Content();
        void Content(IInspectable content);

        void SetWindowVisualState(WindowVisualState visualState);
        void Root_SizeChanged(const IInspectable& sender, const Windows::UI::Xaml::SizeChangedEventArgs& e);

        void Minimize_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void Maximize_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void Close_Click(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::RoutedEventArgs& e);
        void DragBar_DoubleTapped(const winrt::Windows::Foundation::IInspectable& sender, const winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs& e);

    private:
        void _OnMaximizeOrRestore(byte flag);
        HWND _window{ nullptr }; // non-owning handle; should not be freed in the dtor.

        void _backgroundChanged(winrt::Windows::UI::Xaml::Media::Brush brush);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TitlebarControl);
}
