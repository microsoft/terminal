// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"

#include "TerminalPage.h"
#include "../WinRTUtils/inc/WtExeUtils.h"
#include "../../types/inc/utils.hpp"
#include "Utils.h"

using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::Control;
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
                                                 const ActionEventArgs& args)
    {
        _OpenNewTabDropdown();
        args.Handled(true);
    }

    void TerminalPage::_HandleDuplicateTab(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        _DuplicateFocusedTab();
        args.Handled(true);
    }

    void TerminalPage::_HandleCloseTab(const IInspectable& /*sender*/,
                                       const ActionEventArgs& args)
    {
        _CloseFocusedTab();
        args.Handled(true);
    }

    void TerminalPage::_HandleClosePane(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        _CloseFocusedPane();
        args.Handled(true);
    }

    void TerminalPage::_HandleCloseWindow(const IInspectable& /*sender*/,
                                          const ActionEventArgs& args)
    {
        CloseWindow();
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollUp(const IInspectable& /*sender*/,
                                       const ActionEventArgs& args)
    {
        const auto& realArgs = args.ActionArgs().try_as<ScrollUpArgs>();
        if (realArgs)
        {
            _Scroll(ScrollUp, realArgs.RowsToScroll());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleScrollDown(const IInspectable& /*sender*/,
                                         const ActionEventArgs& args)
    {
        const auto& realArgs = args.ActionArgs().try_as<ScrollDownArgs>();
        if (realArgs)
        {
            _Scroll(ScrollDown, realArgs.RowsToScroll());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleNextTab(const IInspectable& /*sender*/,
                                      const ActionEventArgs& args)
    {
        const auto& realArgs = args.ActionArgs().try_as<NextTabArgs>();
        if (realArgs)
        {
            _SelectNextTab(true, realArgs.SwitcherMode());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandlePrevTab(const IInspectable& /*sender*/,
                                      const ActionEventArgs& args)
    {
        const auto& realArgs = args.ActionArgs().try_as<PrevTabArgs>();
        if (realArgs)
        {
            _SelectNextTab(false, realArgs.SwitcherMode());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSendInput(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        if (args == nullptr)
        {
            args.Handled(false);
        }
        else if (const auto& realArgs = args.ActionArgs().try_as<SendInputArgs>())
        {
            const auto termControl = _GetActiveControl();
            termControl.SendInput(realArgs.Input());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSplitPane(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        if (args == nullptr)
        {
            args.Handled(false);
        }
        else if (const auto& realArgs = args.ActionArgs().try_as<SplitPaneArgs>())
        {
            _SplitPane(realArgs.SplitStyle(),
                       realArgs.SplitMode(),
                       // This is safe, we're already filtering so the value is (0, 1)
                       ::base::saturated_cast<float>(realArgs.SplitSize()),
                       realArgs.TerminalArgs());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleTogglePaneZoom(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            // Don't do anything if there's only one pane. It's already zoomed.
            if (activeTab->GetLeafPaneCount() > 1)
            {
                // First thing's first, remove the current content from the UI
                // tree. This is important, because we might be leaving zoom, and if
                // a pane is zoomed, then it's currently in the UI tree, and should
                // be removed before it's re-added in Pane::Restore
                _tabContent.Children().Clear();

                // Togging the zoom on the tab will cause the tab to inform us of
                // the new root Content for this tab.
                activeTab->ToggleZoom();
            }
        }

        args.Handled(true);
    }

    void TerminalPage::_HandleTogglePaneReadOnly(const IInspectable& /*sender*/,
                                                 const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            activeTab->TogglePaneReadOnly();
        }

        args.Handled(true);
    }

    void TerminalPage::_HandleScrollUpPage(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        _ScrollPage(ScrollUp);
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollDownPage(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        _ScrollPage(ScrollDown);
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollToTop(const IInspectable& /*sender*/,
                                          const ActionEventArgs& args)
    {
        _ScrollToBufferEdge(ScrollUp);
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollToBottom(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        _ScrollToBufferEdge(ScrollDown);
        args.Handled(true);
    }

    void TerminalPage::_HandleFindMatch(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<FindMatchArgs>())
        {
            if (const auto& control{ _GetActiveControl() })
            {
                control.SearchMatch(realArgs.Direction() == FindMatchDirection::Next);
                args.Handled(true);
            }
        }
    }
    void TerminalPage::_HandleOpenSettings(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<OpenSettingsArgs>())
        {
            _LaunchSettings(realArgs.Target());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandlePasteText(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        _PasteText();
        args.Handled(true);
    }

    void TerminalPage::_HandleNewTab(const IInspectable& /*sender*/,
                                     const ActionEventArgs& args)
    {
        if (args == nullptr)
        {
            _OpenNewTab(nullptr);
            args.Handled(true);
        }
        else if (const auto& realArgs = args.ActionArgs().try_as<NewTabArgs>())
        {
            _OpenNewTab(realArgs.TerminalArgs());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSwitchToTab(const IInspectable& /*sender*/,
                                          const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<SwitchToTabArgs>())
        {
            _SelectTab({ realArgs.TabIndex() });
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleResizePane(const IInspectable& /*sender*/,
                                         const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<ResizePaneArgs>())
        {
            if (realArgs.ResizeDirection() == ResizeDirection::None)
            {
                // Do nothing
                args.Handled(false);
            }
            else
            {
                _ResizePane(realArgs.ResizeDirection());
                args.Handled(true);
            }
        }
    }

    void TerminalPage::_HandleMoveFocus(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<MoveFocusArgs>())
        {
            if (realArgs.FocusDirection() == FocusDirection::None)
            {
                // Do nothing
                args.Handled(false);
            }
            else
            {
                _MoveFocus(realArgs.FocusDirection());
                args.Handled(true);
            }
        }
    }

    void TerminalPage::_HandleCopyText(const IInspectable& /*sender*/,
                                       const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<CopyTextArgs>())
        {
            const auto handled = _CopyText(realArgs.SingleLine(), realArgs.CopyFormatting());
            args.Handled(handled);
        }
    }

    void TerminalPage::_HandleAdjustFontSize(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<AdjustFontSizeArgs>())
        {
            const auto termControl = _GetActiveControl();
            termControl.AdjustFontSize(realArgs.Delta());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleFind(const IInspectable& /*sender*/,
                                   const ActionEventArgs& args)
    {
        _Find();
        args.Handled(true);
    }

    void TerminalPage::_HandleResetFontSize(const IInspectable& /*sender*/,
                                            const ActionEventArgs& args)
    {
        const auto termControl = _GetActiveControl();
        termControl.ResetFontSize();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleShaderEffects(const IInspectable& /*sender*/,
                                                  const ActionEventArgs& args)
    {
        const auto termControl = _GetActiveControl();
        termControl.ToggleShaderEffects();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleFocusMode(const IInspectable& /*sender*/,
                                              const ActionEventArgs& args)
    {
        ToggleFocusMode();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleFullscreen(const IInspectable& /*sender*/,
                                               const ActionEventArgs& args)
    {
        ToggleFullscreen();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleAlwaysOnTop(const IInspectable& /*sender*/,
                                                const ActionEventArgs& args)
    {
        ToggleAlwaysOnTop();
        args.Handled(true);
    }

    void TerminalPage::_HandleToggleCommandPalette(const IInspectable& /*sender*/,
                                                   const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<ToggleCommandPaletteArgs>())
        {
            CommandPalette().EnableCommandPaletteMode(realArgs.LaunchMode());
            CommandPalette().Visibility(CommandPalette().Visibility() == Visibility::Visible ?
                                            Visibility::Collapsed :
                                            Visibility::Visible);
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSetColorScheme(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        args.Handled(false);
        if (const auto& realArgs = args.ActionArgs().try_as<SetColorSchemeArgs>())
        {
            if (const auto activeTab{ _GetFocusedTabImpl() })
            {
                if (auto activeControl = activeTab->GetActiveTerminalControl())
                {
                    if (const auto scheme = _settings.GlobalSettings().ColorSchemes().TryLookup(realArgs.SchemeName()))
                    {
                        // Start by getting the current settings of the control
                        auto controlSettings = activeControl.Settings().as<TerminalSettings>();
                        auto parentSettings = controlSettings;
                        // Those are the _runtime_ settings however. What we
                        // need to do is:
                        //
                        //   1. Blow away any colors set in the runtime settings.
                        //   2. Apply the color scheme to the parent settings.
                        //
                        // 1 is important to make sure that the effects of
                        // something like `colortool` are cleared when setting
                        // the scheme.
                        if (controlSettings.GetParent() != nullptr)
                        {
                            parentSettings = controlSettings.GetParent();
                        }

                        // ApplyColorScheme(nullptr) will clear the old color scheme.
                        controlSettings.ApplyColorScheme(nullptr);
                        parentSettings.ApplyColorScheme(scheme);

                        activeControl.UpdateSettings();
                        args.Handled(true);
                    }
                }
            }
        }
    }

    void TerminalPage::_HandleSetTabColor(const IInspectable& /*sender*/,
                                          const ActionEventArgs& args)
    {
        Windows::Foundation::IReference<Windows::UI::Color> tabColor;

        if (const auto& realArgs = args.ActionArgs().try_as<SetTabColorArgs>())
        {
            tabColor = realArgs.TabColor();
        }

        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            if (tabColor)
            {
                activeTab->SetRuntimeTabColor(tabColor.Value());
            }
            else
            {
                activeTab->ResetRuntimeTabColor();
            }
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleOpenTabColorPicker(const IInspectable& /*sender*/,
                                                 const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            activeTab->ActivateColorPicker();
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleRenameTab(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        std::optional<winrt::hstring> title;

        if (const auto& realArgs = args.ActionArgs().try_as<RenameTabArgs>())
        {
            title = realArgs.Title();
        }

        if (const auto activeTab{ _GetFocusedTabImpl() })
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

    void TerminalPage::_HandleOpenTabRenamer(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            activeTab->ActivateTabRenamer();
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleExecuteCommandline(const IInspectable& /*sender*/,
                                                 const ActionEventArgs& actionArgs)
    {
        if (const auto& realArgs = actionArgs.ActionArgs().try_as<ExecuteCommandlineArgs>())
        {
            auto actions = winrt::single_threaded_vector<ActionAndArgs>(std::move(
                TerminalPage::ConvertExecuteCommandlineToActions(realArgs)));

            if (_startupActions.Size() != 0)
            {
                actionArgs.Handled(true);
                ProcessStartupActions(actions, false);
            }
        }
    }

    void TerminalPage::_HandleCloseOtherTabs(const IInspectable& /*sender*/,
                                             const ActionEventArgs& actionArgs)
    {
        if (const auto& realArgs = actionArgs.ActionArgs().try_as<CloseOtherTabsArgs>())
        {
            uint32_t index;
            if (realArgs.Index())
            {
                index = realArgs.Index().Value();
            }
            else if (auto focusedTabIndex = _GetFocusedTabIndex())
            {
                index = *focusedTabIndex;
            }
            else
            {
                // Do nothing
                actionArgs.Handled(false);
                return;
            }

            // Since _RemoveTabs is asynchronous, create a snapshot of the  tabs we want to remove
            std::vector<winrt::TerminalApp::TabBase> tabsToRemove;
            if (index > 0)
            {
                std::copy(begin(_tabs), begin(_tabs) + index, std::back_inserter(tabsToRemove));
            }

            if (index + 1 < _tabs.Size())
            {
                std::copy(begin(_tabs) + index + 1, end(_tabs), std::back_inserter(tabsToRemove));
            }

            _RemoveTabs(tabsToRemove);

            actionArgs.Handled(true);
        }
    }

    void TerminalPage::_HandleCloseTabsAfter(const IInspectable& /*sender*/,
                                             const ActionEventArgs& actionArgs)
    {
        if (const auto& realArgs = actionArgs.ActionArgs().try_as<CloseTabsAfterArgs>())
        {
            uint32_t index;
            if (realArgs.Index())
            {
                index = realArgs.Index().Value();
            }
            else if (auto focusedTabIndex = _GetFocusedTabIndex())
            {
                index = *focusedTabIndex;
            }
            else
            {
                // Do nothing
                actionArgs.Handled(false);
                return;
            }

            // Since _RemoveTabs is asynchronous, create a snapshot of the  tabs we want to remove
            std::vector<winrt::TerminalApp::TabBase> tabsToRemove;
            std::copy(begin(_tabs) + index + 1, end(_tabs), std::back_inserter(tabsToRemove));
            _RemoveTabs(tabsToRemove);

            // TODO:GH#7182 For whatever reason, if you run this action
            // when the tab that's currently focused is _before_ the `index`
            // param, then the tabs will expand to fill the entire width of the
            // tab row, until you mouse over them. Probably has something to do
            // with tabs not resizing down until there's a mouse exit event.

            actionArgs.Handled(true);
        }
    }

    void TerminalPage::_HandleTabSearch(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        CommandPalette().SetTabs(_tabs, _mruTabs);
        CommandPalette().EnableTabSearchMode();
        CommandPalette().Visibility(Visibility::Visible);

        args.Handled(true);
    }

    void TerminalPage::_HandleMoveTab(const IInspectable& /*sender*/,
                                      const ActionEventArgs& actionArgs)
    {
        if (const auto& realArgs = actionArgs.ActionArgs().try_as<MoveTabArgs>())
        {
            auto direction = realArgs.Direction();
            if (direction != MoveTabDirection::None)
            {
                if (auto focusedTabIndex = _GetFocusedTabIndex())
                {
                    auto currentTabIndex = focusedTabIndex.value();
                    auto delta = direction == MoveTabDirection::Forward ? 1 : -1;
                    _TryMoveTab(currentTabIndex, currentTabIndex + delta);
                }
            }
            actionArgs.Handled(true);
        }
    }

    void TerminalPage::_HandleBreakIntoDebugger(const IInspectable& /*sender*/,
                                                const ActionEventArgs& actionArgs)
    {
        if (_settings.GlobalSettings().DebugFeaturesEnabled())
        {
            actionArgs.Handled(true);
            DebugBreak();
        }
    }

    // Function Description:
    // - Helper to launch a new WT instance. It can either launch the instance
    //   elevated or unelevated.
    // - To launch elevated, it will as the shell to elevate the process for us.
    //   This might cause a UAC prompt. The elevation is performed on a
    //   background thread, as to not block the UI thread.
    // Arguments:
    // - elevate: If true, launch the new Terminal elevated using `runas`
    // - newTerminalArgs: A NewTerminalArgs describing the terminal instance
    //   that should be spawned. The Profile should be filled in with the GUID
    //   of the profile we want to launch.
    // Return Value:
    // - <none>
    // Important: Don't take the param by reference, since we'll be doing work
    // on another thread.
    fire_and_forget TerminalPage::_OpenNewWindow(const bool elevate,
                                                 const NewTerminalArgs newTerminalArgs)
    {
        // Hop to the BG thread
        co_await winrt::resume_background();

        // This will get us the correct exe for dev/preview/release. If you
        // don't stick this in a local, it'll get mangled by ShellExecute. I
        // have no idea why.
        const auto exePath{ GetWtExePath() };

        // Build the commandline to pass to wt for this set of NewTerminalArgs
        // `-w -1` will ensure a new window is created.
        winrt::hstring cmdline{
            fmt::format(L"-w -1 new-tab {}",
                        newTerminalArgs ? newTerminalArgs.ToCommandline().c_str() :
                                          L"")
        };

        // Build the args to ShellExecuteEx. We need to use ShellExecuteEx so we
        // can pass the SEE_MASK_NOASYNC flag. That flag allows us to safely
        // call this on the background thread, and have ShellExecute _not_ call
        // back to us on the main thread. Without this, if you close the
        // Terminal quickly after the UAC prompt, the elevated WT will never
        // actually spawn.
        SHELLEXECUTEINFOW seInfo{ 0 };
        seInfo.cbSize = sizeof(seInfo);
        seInfo.fMask = SEE_MASK_NOASYNC;
        // `runas` will cause the shell to launch this child process elevated.
        // `open` will just run the executable normally.
        seInfo.lpVerb = elevate ? L"runas" : L"open";
        seInfo.lpFile = exePath.c_str();
        seInfo.lpParameters = cmdline.c_str();
        seInfo.nShow = SW_SHOWNORMAL;
        LOG_IF_WIN32_BOOL_FALSE(ShellExecuteExW(&seInfo));

        co_return;
    }

    void TerminalPage::_HandleNewWindow(const IInspectable& /*sender*/,
                                        const ActionEventArgs& actionArgs)
    {
        NewTerminalArgs newTerminalArgs{ nullptr };
        // If the caller provided NewTerminalArgs, then try to use those
        if (actionArgs)
        {
            if (const auto& realArgs = actionArgs.ActionArgs().try_as<NewWindowArgs>())
            {
                newTerminalArgs = realArgs.TerminalArgs();
            }
        }
        // Otherwise, if no NewTerminalArgs were provided, then just use a
        // default-constructed one. The default-constructed one implies that
        // nothing about the launch should be modified (just use the default
        // profile).
        if (!newTerminalArgs)
        {
            newTerminalArgs = NewTerminalArgs();
        }

        const auto profileGuid{ _settings.GetProfileForArgs(newTerminalArgs) };
        const auto settings{ TerminalSettings::CreateWithNewTerminalArgs(_settings, newTerminalArgs, *_bindings) };

        // Manually fill in the evaluated profile.
        newTerminalArgs.Profile(::Microsoft::Console::Utils::GuidToString(profileGuid));
        _OpenNewWindow(false, newTerminalArgs);
        actionArgs.Handled(true);
    }

    // Method Description:
    // - Raise a IdentifyWindowsRequested event. This will bubble up to the
    //   AppLogic, to the AppHost, to the Peasant, to the Monarch, then get
    //   distributed down to _all_ the Peasants, as to display info about the
    //   window in _every_ Peasant window.
    // - This action is also buggy right now, because TeachingTips behave
    //   weird in XAML Islands. See microsoft-ui-xaml#4382
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void TerminalPage::_HandleIdentifyWindows(const IInspectable& /*sender*/,
                                              const ActionEventArgs& args)
    {
        _IdentifyWindowsRequestedHandlers(*this, nullptr);
        args.Handled(true);
    }

    // Method Description:
    // - Display the "Toast" with the name and ID of this window.
    // - Unlike _HandleIdentifyWindow**s**, this event just displays the window
    //   ID and name in the current window. It does not involve any bubbling
    //   up/down the page/logic/host/manager/peasant/monarch.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void TerminalPage::_HandleIdentifyWindow(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        IdentifyWindow();
        args.Handled(true);
    }

    void TerminalPage::_HandleRenameWindow(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        if (args)
        {
            if (const auto& realArgs = args.ActionArgs().try_as<RenameWindowArgs>())
            {
                const auto newName = realArgs.Name();
                const auto request = winrt::make_self<implementation::RenameWindowRequestedArgs>(newName);
                _RenameWindowRequestedHandlers(*this, *request);
                args.Handled(true);
            }
        }
    }

    void TerminalPage::_HandleOpenWindowRenamer(const IInspectable& /*sender*/,
                                                const ActionEventArgs& args)
    {
        if (WindowRenamer() == nullptr)
        {
            // We need to use FindName to lazy-load this object
            if (MUX::Controls::TeachingTip tip{ FindName(L"WindowRenamer").try_as<MUX::Controls::TeachingTip>() })
            {
                tip.Closed({ get_weak(), &TerminalPage::_FocusActiveControl });
            }
        }

        _UpdateTeachingTipTheme(WindowRenamer().try_as<winrt::Windows::UI::Xaml::FrameworkElement>());
        WindowRenamer().IsOpen(true);

        // PAIN: We can't immediately focus the textbox in the TeachingTip. It's
        // not technically focusable until it is opened. However, it doesn't
        // provide an event to tell us when it is opened. That's tracked in
        // microsoft/microsoft-ui-xaml#1607. So for now, the user _needs_ to
        // click on the text box manually.
        //
        // We're also not using a ContentDialog for this, because in Xaml
        // Islands a text box in a ContentDialog won't receive _any_ keypresses.
        // Fun!
        // WindowRenamerTextBox().Focus(FocusState::Programmatic);

        args.Handled(true);
    }

    void TerminalPage::_HandleGlobalSummon(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        // Manually return false. These shouldn't ever get here, except for when
        // we fail to register for the global hotkey. In that case, returning
        // false here will let the underlying terminal still process the key, as
        // if it wasn't bound at all.
        args.Handled(false);
    }
    void TerminalPage::_HandleQuakeMode(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        // Manually return false. These shouldn't ever get here, except for when
        // we fail to register for the global hotkey. In that case, returning
        // false here will let the underlying terminal still process the key, as
        // if it wasn't bound at all.
        args.Handled(false);
    }
}
