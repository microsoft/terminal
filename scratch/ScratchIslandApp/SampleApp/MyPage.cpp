// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include "MySettings.h"

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
        auto settings = winrt::make_self<implementation::MySettings>();

        auto connectionSettings{ TerminalConnection::ConptyConnection::CreateSettings(L"cmd.exe /k echo This TermControl is hosted in-proc...",
                                                                                      winrt::hstring{},
                                                                                      L"",
                                                                                      nullptr,
                                                                                      32,
                                                                                      80,
                                                                                      winrt::guid()) };

        // "Microsoft.Terminal.TerminalConnection.ConptyConnection"
        winrt::hstring myClass{ winrt::name_of<TerminalConnection::ConptyConnection>() };
        TerminalConnection::ConnectionInformation connectInfo{ myClass, connectionSettings };

        TerminalConnection::ITerminalConnection conn{ TerminalConnection::ConnectionInformation::CreateConnection(connectInfo) };
        Control::TermControl control{ *settings, *settings, conn };

        InProcContent().Children().Append(control);
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
