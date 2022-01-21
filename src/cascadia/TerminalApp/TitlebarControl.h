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
        void Root_SizeChanged(const IInspectable& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e);

        void Minimize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Maximize_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void Close_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void DragBar_DoubleTapped(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::DoubleTappedRoutedEventArgs const& e);

    private:
        void _OnMaximizeOrRestore(byte flag);
        HWND _window{ nullptr }; // non-owning handle; should not be freed in the dtor.
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TitlebarControl);
}
