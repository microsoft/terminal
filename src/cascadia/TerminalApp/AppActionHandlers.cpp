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
    void App::_HandleNewTab(const IInspectable& /*sender*/,
                            const TerminalApp::ActionEventArgs& args)
    {
        _OpenNewTab(std::nullopt);
        args.Handled(true);
    }
    void App::_HandleOpenNewTabDropdown(const IInspectable& /*sender*/,
                                        const TerminalApp::ActionEventArgs& args)
    {
        _OpenNewTabDropdown();
        args.Handled(true);
    }

    void App::_HandleDuplicateTab(const IInspectable& /*sender*/,
                                  const TerminalApp::ActionEventArgs& args)
    {
        _DuplicateTabViewItem();
        args.Handled(true);
    }

    void App::_HandleCloseTab(const IInspectable& /*sender*/,
                              const TerminalApp::ActionEventArgs& args)
    {
        _CloseFocusedTab();
        args.Handled(true);
    }

    void App::_HandleClosePane(const IInspectable& /*sender*/,
                               const TerminalApp::ActionEventArgs& args)
    {
        _CloseFocusedPane();
        args.Handled(true);
    }

    void App::_HandleScrollUp(const IInspectable& /*sender*/,
                              const TerminalApp::ActionEventArgs& args)
    {
        _Scroll(-1);
        args.Handled(true);
    }

    void App::_HandleScrollDown(const IInspectable& /*sender*/,
                                const TerminalApp::ActionEventArgs& args)
    {
        _Scroll(1);
        args.Handled(true);
    }

    void App::_HandleNextTab(const IInspectable& /*sender*/,
                             const TerminalApp::ActionEventArgs& args)
    {
        _SelectNextTab(true);
        args.Handled(true);
    }

    void App::_HandlePrevTab(const IInspectable& /*sender*/,
                             const TerminalApp::ActionEventArgs& args)
    {
        _SelectNextTab(false);
        args.Handled(true);
    }

    void App::_HandleSplitVertical(const IInspectable& /*sender*/,
                                   const TerminalApp::ActionEventArgs& args)
    {
        _SplitVertical(std::nullopt);
        args.Handled(true);
    }

    void App::_HandleSplitHorizontal(const IInspectable& /*sender*/,
                                     const TerminalApp::ActionEventArgs& args)
    {
        _SplitHorizontal(std::nullopt);
        args.Handled(true);
    }

    void App::_HandleScrollUpPage(const IInspectable& /*sender*/,
                                  const TerminalApp::ActionEventArgs& args)
    {
        _ScrollPage(-1);
        args.Handled(true);
    }

    void App::_HandleScrollDownPage(const IInspectable& /*sender*/,
                                    const TerminalApp::ActionEventArgs& args)
    {
        _ScrollPage(1);
        args.Handled(true);
    }

    void App::_HandleOpenSettings(const IInspectable& /*sender*/,
                                  const TerminalApp::ActionEventArgs& args)
    {
        // TODO:<future> Add an optional arg for opening the defaults here
        _OpenSettings(false);
        args.Handled(true);
    }

    void App::_HandlePasteText(const IInspectable& /*sender*/,
                               const TerminalApp::ActionEventArgs& args)
    {
        _PasteText();
        args.Handled(true);
    }

    void App::_HandleNewTabWithProfile(const IInspectable& /*sender*/,
                                       const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::NewTabWithProfileArgs>())
        {
            _OpenNewTab({ realArgs.ProfileIndex() });
            args.Handled(true);
        }
    }

    void App::_HandleSwitchToTab(const IInspectable& /*sender*/,
                                 const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::SwitchToTabArgs>())
        {
            const auto handled = _SelectTab({ realArgs.TabIndex() });
            args.Handled(handled);
        }
    }

    void App::_HandleResizePane(const IInspectable& /*sender*/,
                                const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::ResizePaneArgs>())
        {
            _ResizePane(realArgs.Direction());
            args.Handled(true);
        }
    }

    void App::_HandleMoveFocus(const IInspectable& /*sender*/,
                               const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::MoveFocusArgs>())
        {
            _MoveFocus(realArgs.Direction());
            args.Handled(true);
        }
    }

    void App::_HandleCopyText(const IInspectable& /*sender*/,
                              const TerminalApp::ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<TerminalApp::CopyTextArgs>())
        {
            const auto handled = _CopyText(realArgs.TrimWhitespace());
            args.Handled(handled);
        }
    }

}
