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
        if (const auto realArgs = args.ActionArgs().try_as<CloseTabArgs>())
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
                args.Handled(false);
                return;
            }

            _CloseTabAtIndex(index);
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleClosePane(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        _CloseFocusedPane();
        args.Handled(true);
    }

    void TerminalPage::_HandleRestoreLastClosed(const IInspectable& /*sender*/,
                                                const ActionEventArgs& args)
    {
        if (_previouslyClosedPanesAndTabs.size() > 0)
        {
            const auto restoreActions = _previouslyClosedPanesAndTabs.back();
            for (const auto& action : restoreActions)
            {
                _actionDispatch->DoAction(action);
            }
            _previouslyClosedPanesAndTabs.pop_back();

            args.Handled(true);
        }
    }

    void TerminalPage::_HandleCloseWindow(const IInspectable& /*sender*/,
                                          const ActionEventArgs& args)
    {
        _CloseRequestedHandlers(nullptr, nullptr);
        args.Handled(true);
    }

    void TerminalPage::_HandleQuit(const IInspectable& /*sender*/,
                                   const ActionEventArgs& args)
    {
        RequestQuit();
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
            if (const auto termControl{ _GetActiveControl() })
            {
                termControl.SendInput(realArgs.Input());
                args.Handled(true);
            }
        }
    }

    void TerminalPage::_HandleCloseOtherPanes(const IInspectable& /*sender*/,
                                              const ActionEventArgs& args)
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            const auto activePane = terminalTab->GetActivePane();
            if (terminalTab->GetRootPane() != activePane)
            {
                _UnZoomIfNeeded();

                // Accumulate list of all unfocused leaf panes, ignore read-only panes
                std::vector<uint32_t> unfocusedPaneIds;
                const auto activePaneId = activePane->Id();
                terminalTab->GetRootPane()->WalkTree([&](auto&& p) {
                    const auto id = p->Id();
                    if (id.has_value() && id != activePaneId && !p->ContainsReadOnly())
                    {
                        unfocusedPaneIds.push_back(id.value());
                    }
                });

                if (!empty(unfocusedPaneIds))
                {
                    // Start by removing the panes that were least recently added
                    sort(begin(unfocusedPaneIds), end(unfocusedPaneIds), std::less<uint32_t>());
                    _ClosePanes(terminalTab->get_weak(), std::move(unfocusedPaneIds));
                    args.Handled(true);
                    return;
                }
            }
            args.Handled(false);
        }
    }

    void TerminalPage::_HandleMovePane(const IInspectable& /*sender*/,
                                       const ActionEventArgs& args)
    {
        if (args == nullptr)
        {
            args.Handled(false);
        }
        else if (const auto& realArgs = args.ActionArgs().try_as<MovePaneArgs>())
        {
            auto moved = _MovePane(realArgs);
            args.Handled(moved);
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
            if (const auto& newTerminalArgs{ realArgs.TerminalArgs() })
            {
                if (const auto index = realArgs.TerminalArgs().ProfileIndex())
                {
                    if (gsl::narrow<uint32_t>(index.Value()) >= _settings.ActiveProfiles().Size())
                    {
                        args.Handled(false);
                        return;
                    }
                }
            }

            const auto& duplicateFromTab{ realArgs.SplitMode() == SplitType::Duplicate ? _GetFocusedTab() : nullptr };
            _SplitPane(realArgs.SplitDirection(),
                       // This is safe, we're already filtering so the value is (0, 1)
                       ::base::saturated_cast<float>(realArgs.SplitSize()),
                       _MakePane(realArgs.TerminalArgs(), duplicateFromTab));
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleToggleSplitOrientation(const IInspectable& /*sender*/,
                                                     const ActionEventArgs& args)
    {
        _ToggleSplitOrientation();
        args.Handled(true);
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

    void TerminalPage::_HandleEnablePaneReadOnly(const IInspectable& /*sender*/,
                                                 const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            activeTab->SetPaneReadOnly(true);
        }

        args.Handled(true);
    }

    void TerminalPage::_HandleDisablePaneReadOnly(const IInspectable& /*sender*/,
                                                  const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            activeTab->SetPaneReadOnly(false);
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

    void TerminalPage::_HandleScrollToMark(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<ScrollToMarkArgs>())
        {
            _ApplyToActiveControls([&realArgs](auto& control) {
                control.ScrollToMark(realArgs.Direction());
            });
        }
        args.Handled(true);
    }
    void TerminalPage::_HandleAddMark(const IInspectable& /*sender*/,
                                      const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<AddMarkArgs>())
        {
            _ApplyToActiveControls([realArgs](auto& control) {
                Control::ScrollMark mark;
                if (realArgs.Color())
                {
                    mark.Color.Color = realArgs.Color().Value();
                    mark.Color.HasValue = true;
                }
                else
                {
                    mark.Color.HasValue = false;
                }
                control.AddMark(mark);
            });
        }
        args.Handled(true);
    }
    void TerminalPage::_HandleClearMark(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        _ApplyToActiveControls([](auto& control) {
            control.ClearMark();
        });
        args.Handled(true);
    }
    void TerminalPage::_HandleClearAllMarks(const IInspectable& /*sender*/,
                                            const ActionEventArgs& args)
    {
        _ApplyToActiveControls([](auto& control) {
            control.ClearAllMarks();
        });
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
            LOG_IF_FAILED(_OpenNewTab(nullptr));
            args.Handled(true);
        }
        else if (const auto& realArgs = args.ActionArgs().try_as<NewTabArgs>())
        {
            if (const auto& newTerminalArgs{ realArgs.TerminalArgs() })
            {
                if (const auto index = newTerminalArgs.ProfileIndex())
                {
                    if (gsl::narrow<uint32_t>(index.Value()) >= _settings.ActiveProfiles().Size())
                    {
                        args.Handled(false);
                        return;
                    }
                }
            }

            LOG_IF_FAILED(_OpenNewTab(realArgs.TerminalArgs()));
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
                // Mark as handled only when the move succeeded (e.g. when there
                // is a pane to move to), otherwise mark as unhandled so the
                // keychord can propagate to the terminal (GH#6129)
                const auto moveSucceeded = _MoveFocus(realArgs.FocusDirection());
                args.Handled(moveSucceeded);
            }
        }
    }

    void TerminalPage::_HandleSwapPane(const IInspectable& /*sender*/,
                                       const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<SwapPaneArgs>())
        {
            if (realArgs.Direction() == FocusDirection::None)
            {
                // Do nothing
                args.Handled(false);
            }
            else
            {
                auto swapped = _SwapPane(realArgs.Direction());
                args.Handled(swapped);
            }
        }
    }

    void TerminalPage::_HandleCopyText(const IInspectable& /*sender*/,
                                       const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<CopyTextArgs>())
        {
            const auto handled = _CopyText(realArgs.DismissSelection(), realArgs.SingleLine(), realArgs.CopyFormatting());
            args.Handled(handled);
        }
    }

    void TerminalPage::_HandleAdjustFontSize(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<AdjustFontSizeArgs>())
        {
            const auto res = _ApplyToActiveControls([&](auto& control) {
                control.AdjustFontSize(realArgs.Delta());
            });
            args.Handled(res);
        }
    }

    void TerminalPage::_HandleFind(const IInspectable& /*sender*/,
                                   const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            _Find(*activeTab);
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleResetFontSize(const IInspectable& /*sender*/,
                                            const ActionEventArgs& args)
    {
        const auto res = _ApplyToActiveControls([](auto& control) {
            control.ResetFontSize();
        });
        args.Handled(res);
    }

    void TerminalPage::_HandleToggleShaderEffects(const IInspectable& /*sender*/,
                                                  const ActionEventArgs& args)
    {
        const auto res = _ApplyToActiveControls([](auto& control) {
            control.ToggleShaderEffects();
        });
        args.Handled(res);
    }

    void TerminalPage::_HandleToggleFocusMode(const IInspectable& /*sender*/,
                                              const ActionEventArgs& args)
    {
        ToggleFocusMode();
        args.Handled(true);
    }

    void TerminalPage::_HandleSetFocusMode(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<SetFocusModeArgs>())
        {
            SetFocusMode(realArgs.IsFocusMode());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleToggleFullscreen(const IInspectable& /*sender*/,
                                               const ActionEventArgs& args)
    {
        ToggleFullscreen();
        args.Handled(true);
    }

    void TerminalPage::_HandleSetFullScreen(const IInspectable& /*sender*/,
                                            const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<SetFullScreenArgs>())
        {
            SetFullscreen(realArgs.IsFullScreen());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSetMaximized(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<SetMaximizedArgs>())
        {
            RequestSetMaximized(realArgs.IsMaximized());
            args.Handled(true);
        }
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
            const auto p = LoadCommandPalette();
            const auto v = p.Visibility() == Visibility::Visible ? Visibility::Collapsed : Visibility::Visible;
            p.EnableCommandPaletteMode(realArgs.LaunchMode());
            p.Visibility(v);
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSetColorScheme(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        args.Handled(false);
        if (const auto& realArgs = args.ActionArgs().try_as<SetColorSchemeArgs>())
        {
            if (const auto scheme = _settings.GlobalSettings().ColorSchemes().TryLookup(realArgs.SchemeName()))
            {
                const auto res = _ApplyToActiveControls([&](auto& control) {
                    control.ColorScheme(scheme.ToCoreScheme());
                });
                args.Handled(res);
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
            activeTab->RequestColorPicker();
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

            if (actions.Size() != 0)
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
        const auto p = LoadCommandPalette();
        p.SetTabs(_tabs, _mruTabs);
        p.EnableTabSearchMode();
        p.Visibility(Visibility::Visible);

        args.Handled(true);
    }

    void TerminalPage::_HandleMoveTab(const IInspectable& /*sender*/,
                                      const ActionEventArgs& actionArgs)
    {
        if (const auto& realArgs = actionArgs.ActionArgs().try_as<MoveTabArgs>())
        {
            auto moved = _MoveTab(realArgs);
            actionArgs.Handled(moved);
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
    // - newTerminalArgs: A NewTerminalArgs describing the terminal instance
    //   that should be spawned. The Profile should be filled in with the GUID
    //   of the profile we want to launch.
    // Return Value:
    // - <none>
    // Important: Don't take the param by reference, since we'll be doing work
    // on another thread.
    fire_and_forget TerminalPage::_OpenNewWindow(const NewTerminalArgs newTerminalArgs)
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
        // `open` will just run the executable normally.
        seInfo.lpVerb = L"open";
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

        const auto profile{ _settings.GetProfileForArgs(newTerminalArgs) };

        // Manually fill in the evaluated profile.
        newTerminalArgs.Profile(::Microsoft::Console::Utils::GuidToString(profile.Guid()));
        _OpenNewWindow(newTerminalArgs);
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
            if (auto tip{ FindName(L"WindowRenamer").try_as<MUX::Controls::TeachingTip>() })
            {
                tip.Closed({ get_weak(), &TerminalPage::_FocusActiveControl });
            }
        }

        _UpdateTeachingTipTheme(WindowRenamer().try_as<winrt::Windows::UI::Xaml::FrameworkElement>());

        // BODGY: GH#12021
        //
        // TeachingTip doesn't provide an Opened event.
        // (microsoft/microsoft-ui-xaml#1607). But we want to focus the renamer
        // text box when it's opened. We can't do that immediately, the TextBox
        // technically isn't in the visual tree yet. We have to wait for it to
        // get added some time after we call IsOpen. How do we do that reliably?
        // Usually, for this kind of thing, we'd just use a one-off
        // LayoutUpdated event, as a notification that the TextBox was added to
        // the tree. HOWEVER:
        //   * The _first_ time this is fired, when the box is _first_ opened,
        //     tossing focus doesn't work on the first LayoutUpdated. It does
        //     work on the second LayoutUpdated. Okay, so we'll wait for two
        //     LayoutUpdated events, and focus on the second.
        //   * On subsequent opens: We only ever get a single LayoutUpdated.
        //     Period. But, you can successfully focus it on that LayoutUpdated.
        //
        // So, we'll keep track of how many LayoutUpdated's we've _ever_ gotten.
        // If we've had at least 2, then we can focus the text box.
        //
        // We're also not using a ContentDialog for this, because in Xaml
        // Islands a text box in a ContentDialog won't receive _any_ keypresses.
        // Fun!
        // WindowRenamerTextBox().Focus(FocusState::Programmatic);
        _renamerLayoutUpdatedRevoker.revoke();
        _renamerLayoutUpdatedRevoker = WindowRenamerTextBox().LayoutUpdated(winrt::auto_revoke, [weakThis = get_weak()](auto&&, auto&&) {
            if (auto self{ weakThis.get() })
            {
                auto& count{ self->_renamerLayoutCount };

                // Don't just always increment this, we don't want to deal with overflow situations
                if (count < 2)
                {
                    count++;
                }

                if (count >= 2)
                {
                    self->_renamerLayoutUpdatedRevoker.revoke();
                    self->WindowRenamerTextBox().Focus(FocusState::Programmatic);
                }
            }
        });
        // Make sure to mark that enter was not pressed in the renamer quite
        // yet. More details in TerminalPage::_WindowRenamerKeyDown.
        _renamerPressedEnter = false;
        WindowRenamer().IsOpen(true);

        args.Handled(true);
    }

    void TerminalPage::_HandleDisplayWorkingDirectory(const IInspectable& /*sender*/,
                                                      const ActionEventArgs& args)
    {
        if (_settings.GlobalSettings().DebugFeaturesEnabled())
        {
            ShowTerminalWorkingDirectory();
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSearchForText(const IInspectable& /*sender*/,
                                            const ActionEventArgs& args)
    {
        if (const auto termControl{ _GetActiveControl() })
        {
            if (termControl.HasSelection())
            {
                const auto selections{ termControl.SelectedText(true) };

                // concatenate the selection into a single line
                auto searchText = std::accumulate(selections.begin(), selections.end(), std::wstring());

                // make it compact by replacing consecutive whitespaces with a single space
                searchText = std::regex_replace(searchText, std::wregex(LR"(\s+)"), L" ");

                std::wstring queryUrl;
                if (args)
                {
                    if (const auto& realArgs = args.ActionArgs().try_as<SearchForTextArgs>())
                    {
                        queryUrl = realArgs.QueryUrl().c_str();
                    }
                }

                // use global default if query URL is unspecified
                if (queryUrl.empty())
                {
                    queryUrl = _settings.GlobalSettings().SearchWebDefaultQueryUrl().c_str();
                }

                constexpr std::wstring_view queryToken{ L"%s" };
                if (const auto pos{ queryUrl.find(queryToken) }; pos != std::wstring_view::npos)
                {
                    queryUrl.replace(pos, queryToken.length(), Windows::Foundation::Uri::EscapeComponent(searchText));
                }

                winrt::Microsoft::Terminal::Control::OpenHyperlinkEventArgs shortcut{ queryUrl };
                _OpenHyperlinkHandler(termControl, shortcut);
                args.Handled(true);
            }
        }
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

    void TerminalPage::_HandleFocusPane(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        if (args)
        {
            if (const auto& realArgs = args.ActionArgs().try_as<FocusPaneArgs>())
            {
                const auto paneId = realArgs.Id();
                if (const auto activeTab{ _GetFocusedTabImpl() })
                {
                    _UnZoomIfNeeded();
                    args.Handled(activeTab->FocusPane(paneId));
                }
            }
        }
    }

    void TerminalPage::_HandleOpenSystemMenu(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        _OpenSystemMenuHandlers(*this, nullptr);
        args.Handled(true);
    }

    void TerminalPage::_HandleExportBuffer(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            if (args)
            {
                if (const auto& realArgs = args.ActionArgs().try_as<ExportBufferArgs>())
                {
                    _ExportTab(*activeTab, realArgs.Path());
                    args.Handled(true);
                    return;
                }
            }

            // If we didn't have args, or the args weren't ExportBufferArgs (somehow)
            _ExportTab(*activeTab, L"");
            if (args)
            {
                args.Handled(true);
            }
        }
    }

    void TerminalPage::_HandleClearBuffer(const IInspectable& /*sender*/,
                                          const ActionEventArgs& args)
    {
        if (args)
        {
            if (const auto& realArgs = args.ActionArgs().try_as<ClearBufferArgs>())
            {
                const auto res = _ApplyToActiveControls([&](auto& control) {
                    control.ClearBuffer(realArgs.Clear());
                });
                args.Handled(res);
            }
        }
    }

    void TerminalPage::_HandleMultipleActions(const IInspectable& /*sender*/,
                                              const ActionEventArgs& args)
    {
        if (args)
        {
            if (const auto& realArgs = args.ActionArgs().try_as<MultipleActionsArgs>())
            {
                for (const auto& action : realArgs.Actions())
                {
                    _actionDispatch->DoAction(action);
                }

                args.Handled(true);
            }
        }
    }

    void TerminalPage::_HandleAdjustOpacity(const IInspectable& /*sender*/,
                                            const ActionEventArgs& args)
    {
        if (args)
        {
            if (const auto& realArgs = args.ActionArgs().try_as<AdjustOpacityArgs>())
            {
                const auto res = _ApplyToActiveControls([&](auto& control) {
                    control.AdjustOpacity(realArgs.Opacity() / 100.0, realArgs.Relative());
                });
                args.Handled(res);
            }
        }
    }

    void TerminalPage::_HandleSelectAll(const IInspectable& /*sender*/,
                                        const ActionEventArgs& args)
    {
        if (const auto& control{ _GetActiveControl() })
        {
            control.SelectAll();
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleSelectCommand(const IInspectable& /*sender*/,
                                            const ActionEventArgs& args)
    {
        if (args)
        {
            if (const auto& realArgs = args.ActionArgs().try_as<SelectCommandArgs>())
            {
                const auto res = _ApplyToActiveControls([&](auto& control) {
                    control.SelectCommand(realArgs.Direction() == Settings::Model::SelectOutputDirection::Previous);
                });
                args.Handled(res);
            }
        }
    }
    void TerminalPage::_HandleSelectOutput(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        if (args)
        {
            if (const auto& realArgs = args.ActionArgs().try_as<SelectOutputArgs>())
            {
                const auto res = _ApplyToActiveControls([&](auto& control) {
                    control.SelectOutput(realArgs.Direction() == Settings::Model::SelectOutputDirection::Previous);
                });
                args.Handled(res);
            }
        }
    }

    void TerminalPage::_HandleMarkMode(const IInspectable& /*sender*/,
                                       const ActionEventArgs& args)
    {
        if (const auto& control{ _GetActiveControl() })
        {
            control.ToggleMarkMode();
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleToggleBlockSelection(const IInspectable& /*sender*/,
                                                   const ActionEventArgs& args)
    {
        if (const auto& control{ _GetActiveControl() })
        {
            const auto handled = control.ToggleBlockSelection();
            args.Handled(handled);
        }
    }

    void TerminalPage::_HandleSwitchSelectionEndpoint(const IInspectable& /*sender*/,
                                                      const ActionEventArgs& args)
    {
        if (const auto& control{ _GetActiveControl() })
        {
            const auto handled = control.SwitchSelectionEndpoint();
            args.Handled(handled);
        }
    }

    void TerminalPage::_HandleColorSelection(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        if (args)
        {
            if (const auto& realArgs = args.ActionArgs().try_as<ColorSelectionArgs>())
            {
                const auto res = _ApplyToActiveControls([&](auto& control) {
                    control.ColorSelection(realArgs.Foreground(), realArgs.Background(), realArgs.MatchMode());
                });
                args.Handled(res);
            }
        }
    }

    void TerminalPage::_HandleExpandSelectionToWord(const IInspectable& /*sender*/,
                                                    const ActionEventArgs& args)
    {
        if (const auto& control{ _GetActiveControl() })
        {
            const auto handled = control.ExpandSelectionToWord();
            args.Handled(handled);
        }
    }

    void TerminalPage::_HandleToggleBroadcastInput(const IInspectable& /*sender*/,
                                                   const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            activeTab->ToggleBroadcastInput();
            args.Handled(true);
        }
        // If the focused tab wasn't a TerminalTab, then leave handled=false
    }

    void TerminalPage::_HandleRestartConnection(const IInspectable& /*sender*/,
                                                const ActionEventArgs& args)
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            if (const auto activePane{ activeTab->GetActivePane() })
            {
                _restartPaneConnection(activePane);
            }
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleShowContextMenu(const IInspectable& /*sender*/,
                                              const ActionEventArgs& args)
    {
        if (const auto& control{ _GetActiveControl() })
        {
            control.ShowContextMenu();
        }
        args.Handled(true);
    }

}
