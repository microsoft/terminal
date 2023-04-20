// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include <winrt/SampleExtensions.h>
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
                                                                                      winrt::guid(),
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

    static winrt::fire_and_forget _lookupCatalog() noexcept
    {
        co_await winrt::resume_background();
        try
        {
            auto cat = winrt::Windows::ApplicationModel::AppExtensions::AppExtensionCatalog::Open(winrt::hstring{ L"com.terminal.scratch" });
            auto findOperation{ cat.FindAllAsync() };
            auto extnList = co_await findOperation;
            for (const auto& extn : extnList)
            {
                extn;
                auto name = extn.DisplayName();
                name;
                auto pfn = extn.Package().Id().FamilyName();
                pfn;

                auto hr = S_OK;

                LOG_IF_FAILED(hr);
                if (FAILED(hr))
                    continue;

                PWSTR id;
                hr = TryCreatePackageDependency(nullptr, pfn.c_str(), PACKAGE_VERSION{}, PackageDependencyProcessorArchitectures_None, PackageDependencyLifetimeKind_Process, nullptr, CreatePackageDependencyOptions_None, &id);
                LOG_IF_FAILED(hr);
                if (FAILED(hr))
                    continue;

                PACKAGEDEPENDENCY_CONTEXT ctx;
                PWSTR packageFullName;
                hr = AddPackageDependency(id, 1, AddPackageDependencyOptions_None, &ctx, &packageFullName);
                LOG_IF_FAILED(hr);
                if (FAILED(hr))
                    continue;
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
    void MyPage::ActivateInstanceButtonHandler(Windows::Foundation::IInspectable const&, Windows::UI::Xaml::RoutedEventArgs const&)
    {
        auto hr = S_OK;
        Windows::Foundation::IInspectable foo{ nullptr };
        auto className = winrt::hstring{ L"ExtensionComponent.Class" };
        const auto nameForAbi = static_cast<HSTRING>(winrt::get_abi(className));
        hr = RoActivateInstance(nameForAbi, (::IInspectable**)winrt::put_abi(foo));

        if (foo)
        {
            if (const auto& ext{ foo.try_as<winrt::SampleExtensions::IExtension>() })
            {
                auto oneOhOne = ext.DoTheThing();
                oneOhOne++;
            }
        }
    }
}
