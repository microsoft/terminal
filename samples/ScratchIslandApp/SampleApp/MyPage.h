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

        void Create();

        hstring Title();
        winrt::fire_and_forget CreateOutOfProcTerminal();

        winrt::fire_and_forget CreateClicked(const IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& eventArgs);
        // winrt::fire_and_forget _attachToContent(winrt::guid contentGuid);

    private:
        friend struct MyPageT<MyPage>; // for Xaml to bind events
    };
}

namespace winrt::SampleApp::factory_implementation
{
    BASIC_FACTORY(MyPage);
}
