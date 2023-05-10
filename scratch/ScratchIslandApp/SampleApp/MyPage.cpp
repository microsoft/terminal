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

        // auto connectionSettings{ TerminalConnection::ConptyConnection::CreateSettings(L"cmd.exe /k echo This TermControl is hosted in-proc...",
        //                                                                               winrt::hstring{},
        //                                                                               L"",
        //                                                                               nullptr,
        //                                                                               32,
        //                                                                               80,
        //                                                                               winrt::guid(),
        //                                                                               winrt::guid()) };

        // "Microsoft.Terminal.TerminalConnection.ConptyConnection"
        winrt::hstring myClass{ winrt::name_of<TerminalConnection::EchoConnection>() };
        TerminalConnection::ConnectionInformation connectInfo{ myClass, nullptr };

        _connection = TerminalConnection::ConnectionInformation::CreateConnection(connectInfo);
        Control::TermControl control{ *settings, *settings, _connection };

        // _connection.WriteInput(L"This is an in-proc echo connection!\r\n");

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

    winrt::Windows::Foundation::IAsyncAction MyPage::_lookupCatalog() noexcept
    {
        co_await winrt::resume_background();
        try
        {
            auto cat = winrt::Windows::ApplicationModel::AppExtensions::AppExtensionCatalog::Open(winrt::hstring{ L"com.terminal.scratch" });
            auto findOperation{ cat.FindAllAsync() };
            auto extnList = co_await findOperation;
            for (const auto& extn : extnList)
            {
                DynamicDependency dynDep{};
                auto hr = dynDep.Create(extn);

                LOG_IF_FAILED(hr);
                if (FAILED(hr))
                {
                    _connection.WriteInput(L"Failed to create extension dependency\r\n");
                    continue;
                }

                auto f = dynDep.ResolveProperties();
                bool result = co_await f;
                if (result)
                {
                    _connection.WriteInput(L"Successfully added package dependency to ");
                    _connection.WriteInput(dynDep._pfn);
                    _connection.WriteInput(L"\r\n");

                    this->_extensions.emplace_back(std::move(dynDep), nullptr);
                }
                else
                {
                    _connection.WriteInput(L"Didnt find impelentation for ");
                    _connection.WriteInput(dynDep._pfn);
                    _connection.WriteInput(L"\r\n");
                }
            }
        }
        catch (...)
        {
            // LOG_CAUGHT_EXCEPTION();
            co_return;
        }

        co_return;
    }

    void MyPage::ClickHandler(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&)
    {
        _lookupCatalog();
    }

    winrt::fire_and_forget MyPage::SendInputHandler(Windows::Foundation::IInspectable const&, winrt::SampleExtensions::SendInputArgs args)
    {
        co_await winrt::resume_foreground(Dispatcher());
        _connection.WriteInput(args.Input());
        _connection.WriteInput(L"\r\n");
    }

    winrt::fire_and_forget MyPage::ActivateInstanceButtonHandler(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&)
    {
        if (_extensions.size() == 0)
        {
            co_await _lookupCatalog();
        }
        co_await winrt::resume_foreground(Dispatcher());

        auto hr = S_OK;
        Windows::Foundation::IInspectable foo{ nullptr };

        auto& extension = _extensions.at(0);

        // auto className = winrt::hstring{ L"ExtensionComponent.Class" };
        auto className = extension.app._implementationClassName;
        const auto nameForAbi = static_cast<HSTRING>(winrt::get_abi(className));
        hr = RoActivateInstance(nameForAbi, (::IInspectable**)winrt::put_abi(foo));

        if (foo)
        {
            if (const auto& ext{ foo.try_as<winrt::SampleExtensions::IExtension>() })
            {
                extension.instance = ext;

                auto oneOhOne = extension.instance.DoTheThing();
                oneOhOne++;

                auto fwe = extension.instance.PaneContent();
                OutOfProcContent().Children().Append(fwe);

                extension.instance.SendInputRequested({ get_weak(), &MyPage::SendInputHandler });
            }
        }
        else
        {
            _connection.WriteInput(L"Failed to activate instance \r\n");
        }
    }
}
