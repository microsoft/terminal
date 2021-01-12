// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Toast.g.h"
#include "../../cascadia/inc/cppwinrt_utils.h"

namespace winrt::TerminalApp::implementation
{
    struct Toast : ToastT<Toast>
    {
        Toast();
        void Show();

        Windows::UI::Xaml::Controls::ContentControl Root();

        // WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        // OBSERVABLE_GETSET_PROPERTY(Microsoft::UI::Xaml::Controls::TeachingTip, Root, _PropertyChangedHandlers, nullptr);

    private:
        Windows::UI::Xaml::DispatcherTimer _dismissTimer{ nullptr };
        Microsoft::UI::Xaml::Controls::TeachingTip _root{ nullptr };
        void _onDismiss(Windows::Foundation::IInspectable const&,
                        Windows::Foundation::IInspectable const&);
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(Toast);
}
