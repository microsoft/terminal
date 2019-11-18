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

    void TerminalPage::_HandleSplitVertical(const IInspectable& /*sender*/,
                                            const TerminalApp::ActionEventArgs& args)
    {
        _SplitVertical(std::nullopt);
        args.Handled(true);
    }

    void TerminalPage::_HandleSplitHorizontal(const IInspectable& /*sender*/,
                                              const TerminalApp::ActionEventArgs& args)
    {
        _SplitHorizontal(std::nullopt);
        args.Handled(true);
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
        // TODO:GH#2557 Add an optional arg for opening the defaults here
        _LaunchSettings(false);
        args.Handled(true);
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
            _OpenNewTab(std::nullopt);
            args.Handled(true);
        }
        else if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::NewTabArgs>())
        {
            if (realArgs.ProfileIndex() == nullptr)
            {
                _OpenNewTab(std::nullopt);
            }
            else
            {
                _OpenNewTab(realArgs.ProfileIndex().Value());
            }
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
            const auto handled = _CopyText(realArgs.TrimWhitespace());
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

    void TerminalPage::_HandleToggleFullscreen(const IInspectable& /*sender*/,
                                               const TerminalApp::ActionEventArgs& args)
    {
        _ToggleFullscreen();
        args.Handled(true);
    }

}
