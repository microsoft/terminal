// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "winrt/Microsoft.UI.Xaml.Controls.h"

#include "TabHeaderControl.g.h"

namespace winrt::Microsoft::Terminal::App::implementation
{
    struct TabHeaderControl : TabHeaderControlT<TabHeaderControl>
    {
        TabHeaderControl();
        void BeginRename();

        void RenameBoxLostFocusHandler(const winrt::Windows::Foundation::IInspectable& sender,
                                       const winrt::Windows::UI::Xaml::RoutedEventArgs& e);

        bool InRename();

        WINRT_CALLBACK(TitleChangeRequested, Microsoft::Terminal::App::TitleChangeRequestedArgs);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Title, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(double, RenamerMaxWidth, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::Microsoft::Terminal::App::TerminalTabStatus, TabStatus, _PropertyChangedHandlers);

        TYPED_EVENT(RenameEnded, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);

    private:
        bool _receivedKeyDown{ false };
        bool _renameCancelled{ false };

        void _CloseRenameBox();
    };
}

namespace winrt::Microsoft::Terminal::App::factory_implementation
{
    BASIC_FACTORY(TabHeaderControl);
}
