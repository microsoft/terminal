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

        winrt::fire_and_forget CreateClicked(const IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& eventArgs);
        void CloseClicked(const IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& eventArgs);
        void KillClicked(const IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& eventArgs);

    private:
        friend struct MyPageT<MyPage>; // for Xaml to bind events

        wil::unique_process_information piContentProcess;

        winrt::fire_and_forget _writeToLog(std::wstring_view str);
    };
}

namespace winrt::SampleApp::factory_implementation
{
    BASIC_FACTORY(MyPage);
}
