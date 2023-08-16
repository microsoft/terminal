// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "MyPage.g.h"
#include "../../../src/cascadia/inc/cppwinrt_utils.h"

namespace winrt::SampleApp::implementation
{
    struct MyPage : MyPageT<MyPage>
    {
    public:
        MyPage();

        void Create(uint64_t hwnd);

        hstring Title();

        winrt::fire_and_forget OnLoadIconClick(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);

    private:
        friend struct MyPageT<MyPage>; // for Xaml to bind events
        HWND _hwnd{ nullptr };

        void _attemptOne(const winrt::hstring& text);
        winrt::fire_and_forget _attemptTwo(winrt::hstring text);
        void _setTaskbarBadge(HICON hIcon);
        void _setTaskbarIcon(HICON hIcon);
    };
}

namespace winrt::SampleApp::factory_implementation
{
    BASIC_FACTORY(MyPage);
}
