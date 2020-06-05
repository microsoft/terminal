// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"

#include "TerminalPage.h"
#include "Utils.h"

using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace ::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{
    void TerminalPage::_HandleOpenNewTabDropdown(const IInspectable& /*sender*/,
                                                 const TerminalApp::ActionEventArgs& args)
    {
        _OpenNewTabDropdown();
        args.Handled(true);
    }

    void TerminalPage::_HandleDuplicateTab(const IInspectable& /*sender*/,
                                           const TerminalApp::ActionEventArgs& args)
    {
        _DuplicateTabViewItem();
        args.Handled(true);
    }

    void TerminalPage::_HandleCloseTab(const IInspectable& /*sender*/,
                                       const TerminalApp::ActionEventArgs& args)
    {
        _CloseFocusedTab();
        args.Handled(true);
    }

    void TerminalPage::_HandleClosePane(const IInspectable& /*sender*/,
                                        const TerminalApp::ActionEventArgs& args)
    {
        _CloseFocusedPane();
        args.Handled(true);
    }

    void TerminalPage::_HandleCloseWindow(const IInspectable& /*sender*/,
                                          const TerminalApp::ActionEventArgs& args)
    {
        CloseWindow();
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollUp(const IInspectable& /*sender*/,
                                       const TerminalApp::ActionEventArgs& args)
    {
        _Scroll(-1);
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollDown(const IInspectable& /*sender*/,
                                         const TerminalApp::ActionEventArgs& args)
    {
        _Scroll(1);
        args.Handled(true);
    }

    void TerminalPage::_HandleNextTab(const IInspectable& /*sender*/,
                                      const TerminalApp::ActionEventArgs& args)
    {
        _SelectNextTab(true);
        args.Handled(true);
    }

    void TerminalPage::_HandlePrevTab(const IInspectable& /*sender*/,
                                      const TerminalApp::ActionEventArgs& args)
    {
        _SelectNextTab(false);
        args.Handled(true);
    }

    void TerminalPage::_HandleSplitPane(const IInspectable& /*sender*/,
                                        const TerminalApp::ActionEventArgs& args)
    {
        if (args == nullptr)
        {
            args.Handled(false);
        }
        else if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::SplitPaneArgs>())
        {
            _SplitPane(realArgs.SplitStyle(), realArgs.SplitMode(), realArgs.TerminalArgs());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleScrollUpPage(const IInspectable& /*sender*/,
                                           const TerminalApp::ActionEventArgs& args)
    {
        _ScrollPage(-1);
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollDownPage(const IInspectable& /*sender*/,
                                             const TerminalApp::ActionEventArgs& args)
    {
        _ScrollPage(1);
        args.Handled(true);
    }

    void TerminalPage::_HandleOpenSettings(const IInspectable& /*sender*/,
                                           const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::OpenSettingsArgs>())
        {
            _LaunchSettings(realArgs.Target());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandlePasteText(const IInspectable& /*sender*/,
                                        const TerminalApp::ActionEventArgs& args)
    {
        _PasteText();
        args.Handled(true);
    }

    void TerminalPage::_HandleNewTab(const IInspectable& /*sender*/,
                                     const TerminalApp::ActionEventArgs& args)
    {
        if (args == nullptr)
        {
            _OpenNewTab(nullptr);
            args.Handled(true);
        }
        else if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::NewTabArgs>())
        {
            _OpenNewTab(realArgs.TerminalArgs());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSwitchToTab(const IInspectable& /*sender*/,
                                          const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::SwitchToTabArgs>())
        {
            const auto handled = _SelectTab({ realArgs.TabIndex() });
            args.Handled(handled);
        }
    }

    void TerminalPage::_HandleResizePane(const IInspectable& /*sender*/,
                                         const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::ResizePaneArgs>())
        {
            if (realArgs.Direction() == TerminalApp::Direction::None)
            {
                // Do nothing
                args.Handled(false);
            }
            else
            {
                _ResizePane(realArgs.Direction());
                args.Handled(true);
            }
        }
    }

    void TerminalPage::_HandleMoveFocus(const IInspectable& /*sender*/,
                                        const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::MoveFocusArgs>())
        {
            if (realArgs.Direction() == TerminalApp::Direction::None)
            {
                // Do nothing
                args.Handled(false);
            }
            else
            {
                _MoveFocus(realArgs.Direction());
                args.Handled(true);
            }
        }
    }

    void TerminalPage::_HandleCopyText(const IInspectable& /*sender*/,
                                       const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::CopyTextArgs>())
        {
            const auto handled = _CopyText(realArgs.SingleLine());
            args.Handled(handled);
        }
    }

    void TerminalPage::_HandleAdjustFontSize(const IInspectable& /*sender*/,
                                             const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::AdjustFontSizeArgs>())
        {
            const auto termControl = _GetActiveControl();
            termControl.AdjustFontSize(realArgs.Delta());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleFind(const IInspectable& /*sender*/,
                                   const TerminalApp::ActionEventArgs& args)
    {
        _Find();
        args.Handled(true);
    }

    void TerminalPage::_HandleResetFontSize(const IInspectable& /*sender*/,
                                            const TerminalApp::ActionEventArgs& args)
    {
        const auto termControl = _GetActiveControl();
        termControl.ResetFontSize();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleFullscreen(const IInspectable& /*sender*/,
                                               const TerminalApp::ActionEventArgs& args)
    {
        ToggleFullscreen();
        args.Handled(true);
    }

    void TerminalPage::_HandleExecuteCommandline(const IInspectable& /*sender*/,
                                                 const TerminalApp::ActionEventArgs& actionArgs)
    {
        if (const auto& realArgs = actionArgs.ActionArgs().try_as<TerminalApp::ExecuteCommandlineArgs>())
        {
            realArgs;
            ::TerminalApp::AppCommandlineArgs appArgs;

            // copied from AppLogic::_ParseArgs
            auto parseArgs = [this, &appArgs](auto& args) -> int {
                // std::vector<winrt::hstring> argv;
                // argv.push_back(L"wt.exe"); // { realArgs.Commandline() };
                // argv.push_back(realArgs.Commandline()); // { realArgs.Commandline() };
                winrt::array_view<const winrt::hstring> actions{ args };
                auto commands = ::TerminalApp::AppCommandlineArgs::BuildCommands(actions);
                for (auto& cmdBlob : commands)
                {
                    const auto result = appArgs.ParseCommand(cmdBlob);

                    // If this succeeded, result will be 0. Otherwise, the caller should
                    // exit(result), to exit the program.
                    if (result != 0)
                    {
                        return result;
                    }
                }

                // If all the args were successfully parsed, we'll have some commands
                // built in appArgs, which we'll use when the application starts up.
                return 0;
            };

            std::wstring fullCmdline = std::wstring{ L"wt.exe " } + realArgs.Commandline().c_str();
            int argc = 0;
            wil::unique_any<LPWSTR*, decltype(&::LocalFree), ::LocalFree> argv{ CommandLineToArgvW(fullCmdline.c_str(), &argc) };
            if (argv)
            {
                std::vector<winrt::hstring> args;
                for (auto& elem : wil::make_range(argv.get(), argc))
                {
                    args.emplace_back(elem);
                }

                bool handled = parseArgs(args) == 0;

                if (handled)
                {
                    _startupActions = appArgs.GetStartupActions();
                    _ProcessStartupActions(false);
                }

                actionArgs.Handled(handled);
            }
        }
    }

}
