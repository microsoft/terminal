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
        _createOutOfProcContent();
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

    void MyPage::_createOutOfProcContent()
    {
        auto settings = winrt::make_self<implementation::MySettings>();

        settings->DefaultBackground(til::color{ 0x25, 0x25, 0x25 });
        settings->AutoMarkPrompts(true);
        auto envMap = winrt::single_threaded_map<winrt::hstring, winrt::hstring>();
        envMap.Insert(L"PROMPT", L"$e]133;D$e\\$e]133;A$e\\$e]9;9;$P$e\\$P$G$e]133;B$e\\");

        auto connectionSettings{ TerminalConnection::ConptyConnection::CreateSettings(L"cmd.exe /k echo This a notebook connection.",
                                                                                      winrt::hstring{},
                                                                                      L"",
                                                                                      false,
                                                                                      L"",
                                                                                      envMap.GetView(),
                                                                                      32,
                                                                                      80,
                                                                                      winrt::guid(),
                                                                                      winrt::guid()) };

        // "Microsoft.Terminal.TerminalConnection.ConptyConnection"
        winrt::hstring myClass{ winrt::name_of<TerminalConnection::ConptyConnection>() };
        TerminalConnection::ConnectionInformation connectInfo{ myClass, connectionSettings };

        TerminalConnection::ITerminalConnection conn{ TerminalConnection::ConnectionInformation::CreateConnection(connectInfo) };

        _notebook = Control::Notebook(*settings, *settings, conn);
        _notebook.NewBlock({ get_weak(), &MyPage::_newBlockHandler });
        _addControl(_notebook.ActiveControl());

        // for (const auto& control : _notebook.Controls())
        // {
        //     _addControl(control);
        // }
    }

    void MyPage::_newBlockHandler(const Control::Notebook& /*sender*/,
                                  const Control::TermControl& control)
    {
        _addControl(control);
    }

    void MyPage::_scrollToElement(const WUX::UIElement& element,
                                  bool isVerticalScrolling,
                                  bool smoothScrolling)
    {
        const auto scrollViewer = _scrollViewer();

        const auto origin = winrt::Windows::Foundation::Point{ 0, 0 };

        const auto transform_scrollContent = element.TransformToVisual(scrollViewer.Content().try_as<WUX::UIElement>());
        const auto position_scrollContent = transform_scrollContent.TransformPoint(origin);

        if (isVerticalScrolling)
        {
            scrollViewer.ChangeView(nullptr, position_scrollContent.Y, nullptr, !smoothScrolling);
        }
        else
        {
            scrollViewer.ChangeView(position_scrollContent.X, nullptr, nullptr, !smoothScrolling);
        }
    }

    void MyPage::_addControl(const Control::TermControl& control)
    {
        control.Height(256);
        control.VerticalAlignment(WUX::VerticalAlignment::Top);
        control.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);

        WUX::Controls::Grid wrapper{};
        wrapper.VerticalAlignment(WUX::VerticalAlignment::Top);
        wrapper.HorizontalAlignment(WUX::HorizontalAlignment::Stretch);
        wrapper.CornerRadius(WUX::CornerRadiusHelper::FromRadii(6, 6, 6, 6));
        wrapper.Margin(WUX::ThicknessHelper::FromLengths(0, 5, 0, 7));
        wrapper.Children().Append(control);

        OutOfProcContent().Children().Append(wrapper);

        control.Focus(WUX::FocusState::Programmatic);

        // Incredibly dumb: move off UI thread, then back on, then scroll to the
        // new control.
        _stupid(wrapper);
    }

    winrt::fire_and_forget MyPage::_stupid(WUX::UIElement elem)
    {
        co_await winrt::resume_after(2ms); // no, resume_background is not enough to make this work.
        co_await winrt::resume_foreground(this->Dispatcher(), winrt::Windows::UI::Core::CoreDispatcherPriority::Low);
        _scrollToElement(elem);
    }
}
