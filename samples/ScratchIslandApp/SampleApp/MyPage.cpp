// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include "..\..\..\src\cascadia\UnitTests_Control\MockControlSettings.h"

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
        TerminalConnection::EchoConnection conn{};
        auto settings = winrt::make_self<ControlUnitTests::MockControlSettings>();

        // winrt::com_ptr<TerminalConnection::ITerminalConnection> conn2{ nullptr };
        // // auto f = conn2.get();
        // ::IInspectable* i{ nullptr };
        // winrt::hstring h{ L"Microsoft.Terminal.TerminalConnection.EchoConnection" };
        // auto a = h;
        // HSTRING hs;
        // winrt::copy_to_abi(h, hs);
        // HRESULT hr = RoActivateInstance(hs, &i);
        // winrt::Windows::Foundation::IInspectable ii{ i };
        // TerminalConnection::ITerminalConnection conn3 = ;

        winrt::hstring myClass{ L"Microsoft.Terminal.TerminalConnection.EchoConnection" };
        // winrt::hstring myClass{ L"Microsoft.Terminal.TerminalConnection.ConptyConnection" };
        winrt::IInspectable coolInspectable{};
        auto name = static_cast<HSTRING>(winrt::get_abi(myClass));
        auto foo = winrt::put_abi(coolInspectable);
        ::IInspectable** bar = reinterpret_cast<::IInspectable**>(foo);
        RoActivateInstance(name, bar);
        auto conn2 = coolInspectable.try_as<TerminalConnection::ITerminalConnection>();

        Control::TermControl control{ *settings, conn2 };

        InProcContent().Children().Append(control);

        // Once the control loads (and not before that), write some text for debugging:
        control.Initialized([conn](auto&&, auto&&) {
            conn.WriteInput(L"This TermControl is hosted in-proc...");
        });
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
