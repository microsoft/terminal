// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include "MySettings.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include "..\..\..\src\cascadia\UnitTests_Control\MockControlSettings.h"
#include "..\..\..\src\types\inc\utils.hpp"

using namespace std::chrono_literals;
using namespace winrt::Microsoft::Terminal;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::SampleApp::implementation
{
    MyPage::MyPage()
    {
        InitializeComponent();
    }

    void MyPage::Create()
    {
    }

    winrt::fire_and_forget MyPage::CreateClicked(const IInspectable& sender,
                                                 const WUX::Input::TappedRoutedEventArgs& eventArgs)
    {
        co_await winrt::resume_background();
    }

    void MyPage::CloseClicked(const IInspectable& /*sender*/,
                              const WUX::Input::TappedRoutedEventArgs& /*eventArgs*/)
    {
    }

    void MyPage::KillClicked(const IInspectable& /*sender*/,
                             const WUX::Input::TappedRoutedEventArgs& /*eventArgs*/)
    {
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Windows Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Windows Terminal"
    hstring MyPage::Title()
    {
        return { L"Sample Application" };
    }

}
