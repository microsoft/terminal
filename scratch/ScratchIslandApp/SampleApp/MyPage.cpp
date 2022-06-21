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
        auto settings = winrt::make_self<MySettings>();

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

        // Once the control loads (and not before that), write some text for debugging:
        control.Initialized([conn](auto&&, auto&&) {
            conn.WriteInput(L"This TermControl is hosted in-proc...");
        });
    }

    static wil::unique_process_information _createHostClassProcess(const winrt::guid& g)
    {
        auto guidStr{ ::Microsoft::Console::Utils::GuidToString(g) };

        // Create an event that the content process will use to signal it is
        // ready to go. We won't need the event after this function, so the
        // unique_event will clean up our handle when we leave this scope. The
        // ContentProcess is responsible for cleaning up its own handle.
        wil::unique_event ev{ CreateEvent(nullptr, true, false, nullptr) };
        // Make sure to mark this handle as inheritable! Even with
        // bInheritHandles=true, this is only inherited when it's explicitly
        // allowed to be.
        SetHandleInformation(ev.get(), HANDLE_FLAG_INHERIT, 1);

        // god bless, fmt::format will format a HANDLE like `0xa80`
        std::wstring commandline{
            fmt::format(L"WindowsTerminal.exe --content {} --signal {}", guidStr, ev.get())
        };

        STARTUPINFO siOne{ 0 };
        siOne.cb = sizeof(STARTUPINFOW);
        wil::unique_process_information piOne;
        auto succeeded = CreateProcessW(
            nullptr,
            commandline.data(),
            nullptr, // lpProcessAttributes
            nullptr, // lpThreadAttributes
            true, // bInheritHandles
            CREATE_UNICODE_ENVIRONMENT, // dwCreationFlags
            nullptr, // lpEnvironment
            nullptr, // startingDirectory
            &siOne, // lpStartupInfo
            &piOne // lpProcessInformation
        );
        THROW_IF_WIN32_BOOL_FALSE(succeeded);

        // Wait for the child process to signal that they're ready.
        WaitForSingleObject(ev.get(), INFINITE);

        return piOne;
    }

    winrt::fire_and_forget MyPage::_writeToLog(std::wstring_view str)
    {
        winrt::hstring copy{ str };
        // Switch back to the UI thread.
        co_await resume_foreground(Dispatcher());
        winrt::WUX::Controls::TextBlock block;
        block.Text(copy);
        Log().Children().Append(block);
    }

    winrt::fire_and_forget MyPage::CreateClicked(const IInspectable& sender,
                                                 const WUX::Input::TappedRoutedEventArgs& eventArgs)
    {
        auto guidString = GuidInput().Text();

        // Capture calling context.
        winrt::apartment_context ui_thread;

        auto canConvert = guidString.size() == 38 &&
                          guidString.front() == '{' &&
                          guidString.back() == '}';
        bool tryingToAttach = false;
        winrt::guid contentGuid{ ::Microsoft::Console::Utils::CreateGuid() };

        if (canConvert)
        {
            GUID result{};
            if (SUCCEEDED(IIDFromString(guidString.c_str(), &result)))
            {
                contentGuid = result;
                tryingToAttach = true;
            }
        }
        _writeToLog(tryingToAttach ? L"Attaching to existing content process" : L"Creating new content process");

        co_await winrt::resume_background();
        if (!tryingToAttach)
        {
            // Spawn a wt.exe, with the guid on the commandline
            piContentProcess = std::move(_createHostClassProcess(contentGuid));
        }

        // THIS MUST TAKE PLACE AFTER _createHostClassProcess.
        // * If we're creating a new OOP control, _createHostClassProcess will
        //   spawn the process that will actually host the ContentProcess
        //   object.
        // * If we're attaching, then that process already exists.
        Control::ContentProcess content{ nullptr };
        try
        {
            content = create_instance<Control::ContentProcess>(contentGuid, CLSCTX_LOCAL_SERVER);
        }
        catch (winrt::hresult_error hr)
        {
            _writeToLog(L"CreateInstance the ContentProcess object");
            _writeToLog(fmt::format(L"    HR ({}): {}", hr.code(), hr.message().c_str()));
            co_return; // be sure to co_return or we'll fall through to the part where we clear the log
        }

        if (content == nullptr)
        {
            _writeToLog(L"Failed to connect to the ContentProcess object. It may not have been started fast enough.");
            co_return; // be sure to co_return or we'll fall through to the part where we clear the log
        }

        TerminalConnection::ConnectionInformation connectInfo{ nullptr };
        Control::IControlSettings settings{ *winrt::make_self<implementation::MySettings>() };

        // When creating a terminal for the first time, pass it a connection
        // info
        //
        // otherwise, when attaching to an existing one, just pass null, because
        // we don't need the connection info.
        if (!tryingToAttach)
        {
            auto connectionSettings{ TerminalConnection::ConptyConnection::CreateSettings(L"cmd.exe /k echo This TermControl is hosted out-of-proc...",
                                                                                          winrt::hstring{},
                                                                                          L"",
                                                                                          nullptr,
                                                                                          32,
                                                                                          80,
                                                                                          winrt::guid()) };

            // "Microsoft.Terminal.TerminalConnection.ConptyConnection"
            winrt::hstring myClass{ winrt::name_of<TerminalConnection::ConptyConnection>() };
            connectInfo = TerminalConnection::ConnectionInformation(myClass, connectionSettings);

            if (!content.Initialize(settings, settings, connectInfo))
            {
                _writeToLog(L"Failed to Initialize the ContentProcess object.");
                co_return; // be sure to co_return or we'll fall through to the part where we clear the log
            }
        }
        else
        {
            // If we're attaching, we don't really need to do anything special.
        }

        // Switch back to the UI thread.
        co_await ui_thread;

        // Create the XAML control that will be attached to the content process.
        // We're not passing in a connection, because the contentGuid will be used instead.
        Control::TermControl control{ contentGuid, settings, settings, nullptr };
        auto weakControl = winrt::make_weak(control);
        control.RaiseNotice([this](auto&&, auto& args) {
            _writeToLog(L"Content process died, probably.");
            _writeToLog(args.Message());
            OutOfProcContent().Children().Clear();
            GuidInput().Text(L"");
            if (piContentProcess.hProcess)
            {
                piContentProcess.reset();
            }
        });
        control.ConnectionStateChanged([this, weakControl](auto&&, auto&) {
            if (auto strongControl{ weakControl.get() })
            {
                const auto newConnectionState = strongControl.ConnectionState();
                if (newConnectionState == TerminalConnection::ConnectionState::Closed)
                {
                    _writeToLog(L"Connection was closed");
                    OutOfProcContent().Children().Clear();
                    GuidInput().Text(L"");
                    if (piContentProcess.hProcess)
                    {
                        piContentProcess.reset();
                    }
                }
            }
        });

        Log().Children().Clear();
        OutOfProcContent().Children().Append(control);

        if (!tryingToAttach)
        {
            auto guidStr{ ::Microsoft::Console::Utils::GuidToString(contentGuid) };
            GuidInput().Text(guidStr);
        }
    }

    void MyPage::CloseClicked(const IInspectable& /*sender*/,
                              const WUX::Input::TappedRoutedEventArgs& /*eventArgs*/)
    {
        OutOfProcContent().Children().Clear();
        GuidInput().Text(L"");
        if (piContentProcess.hProcess)
        {
            piContentProcess.reset();
        }
    }

    void MyPage::KillClicked(const IInspectable& /*sender*/,
                             const WUX::Input::TappedRoutedEventArgs& /*eventArgs*/)
    {
        if (piContentProcess.hProcess)
        {
            TerminateProcess(piContentProcess.hProcess, (UINT)-1);
            piContentProcess.reset();
        }
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
