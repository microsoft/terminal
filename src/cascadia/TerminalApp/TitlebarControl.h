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
        void Root_SizeChanged(const IInspectable& sender, const WUX::SizeChangedEventArgs& e);

        void Minimize_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void Maximize_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void Close_Click(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void DragBar_DoubleTapped(const WF::IInspectable& sender, const WUX::Input::DoubleTappedRoutedEventArgs& e);

    private:
        void _OnMaximizeOrRestore(byte flag);
        HWND _window{ nullptr }; // non-owning handle; should not be freed in the dtor.

        void _backgroundChanged(WUXMedia::Brush brush);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TitlebarControl);
}
