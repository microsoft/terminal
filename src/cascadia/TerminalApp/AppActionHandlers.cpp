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
using namespace winrt::Windows::Foundation::Collections;
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

    void TerminalPage::_HandleToggleRetroEffect(const IInspectable& /*sender*/,
                                                const TerminalApp::ActionEventArgs& args)
    {
        const auto termControl = _GetActiveControl();
        termControl.ToggleRetroEffect();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleFocusMode(const IInspectable& /*sender*/,
                                              const TerminalApp::ActionEventArgs& args)
    {
        ToggleFocusMode();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleFullscreen(const IInspectable& /*sender*/,
                                               const TerminalApp::ActionEventArgs& args)
    {
        ToggleFullscreen();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleAlwaysOnTop(const IInspectable& /*sender*/,
                                                const TerminalApp::ActionEventArgs& args)
    {
        ToggleAlwaysOnTop();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleCommandPalette(const IInspectable& /*sender*/,
                                                   const TerminalApp::ActionEventArgs& args)
    {
        // TODO GH#6677: When we add support for commandline mode, first set the
        // mode that the command palette should be in, before making it visible.
        CommandPalette().Visibility(CommandPalette().Visibility() == Visibility::Visible ?
                                        Visibility::Collapsed :
                                        Visibility::Visible);
        args.Handled(true);
    }

    void TerminalPage::_HandleSetTabColor(const IInspectable& /*sender*/,
                                          const TerminalApp::ActionEventArgs& args)
    {
        std::optional<til::color> tabColor;

        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::SetTabColorArgs>())
        {
            if (realArgs.TabColor() != nullptr)
            {
                tabColor = realArgs.TabColor().Value();
            }
        }

        auto activeTab = _GetFocusedTab();
        if (activeTab)
        {
            if (tabColor.has_value())
            {
                activeTab->SetTabColor(tabColor.value());
            }
            else
            {
                activeTab->ResetTabColor();
            }
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleOpenTabColorPicker(const IInspectable& /*sender*/,
                                                 const TerminalApp::ActionEventArgs& args)
    {
        auto activeTab = _GetFocusedTab();
        if (activeTab)
        {
            activeTab->ActivateColorPicker();
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleRenameTab(const IInspectable& /*sender*/,
                                        const TerminalApp::ActionEventArgs& args)
    {
        std::optional<winrt::hstring> title;

        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::RenameTabArgs>())
        {
            title = realArgs.Title();
        }

        auto activeTab = _GetFocusedTab();
        if (activeTab)
        {
            if (title.has_value())
            {
                activeTab->SetTabText(title.value());
            }
            else
            {
                activeTab->ResetTabText();
            }
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleExecuteCommandline(const IInspectable& /*sender*/,
                                                 const TerminalApp::ActionEventArgs& actionArgs)
    {
        if (const auto& realArgs = actionArgs.ActionArgs().try_as<TerminalApp::ExecuteCommandlineArgs>())
        {
            auto actions = winrt::single_threaded_vector<winrt::TerminalApp::ActionAndArgs>(std::move(
                TerminalPage::ConvertExecuteCommandlineToActions(realArgs)));

            if (_startupActions.Size() != 0)
            {
                actionArgs.Handled(true);
                _ProcessStartupActions(actions, false);
            }
        }
    }
}
