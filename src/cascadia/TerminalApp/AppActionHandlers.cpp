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
using namespace winrt::Microsoft::Terminal::Settings::Model;
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
                                                 const ActionEventArgs& args)
    {
        _OpenNewTabDropdown();
        args.Handled(true);
    }

    void TerminalPage::_HandleDuplicateTab(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        _DuplicateTabViewItem();
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
        _Scroll(-1);
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollDown(const IInspectable& /*sender*/,
                                         const ActionEventArgs& args)
    {
        _Scroll(1);
        args.Handled(true);
    }

    void TerminalPage::_HandleNextTab(const IInspectable& /*sender*/,
                                      const ActionEventArgs& args)
    {
        _SelectNextTab(true);
        args.Handled(true);
    }

    void TerminalPage::_HandlePrevTab(const IInspectable& /*sender*/,
                                      const ActionEventArgs& args)
    {
        _SelectNextTab(false);
        args.Handled(true);
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
            _SplitPane(realArgs.SplitStyle(), realArgs.SplitMode(), realArgs.TerminalArgs());
            args.Handled(true);
        }
    }

    void TerminalPage::_HandleTogglePaneZoom(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        auto activeTab = _GetFocusedTab();

        // Don't do anything if there's only one pane. It's already zoomed.
        if (activeTab && activeTab->GetLeafPaneCount() > 1)
        {
            // First thing's first, remove the current content from the UI
            // tree. This is important, because we might be leaving zoom, and if
            // a pane is zoomed, then it's currently in the UI tree, and should
            // be removed before it's re-added in Pane::Restore
            _tabContent.Children().Clear();

            activeTab->ToggleZoom();

            // Update the selected tab, to trigger us to re-add the tab's GetRootElement to the UI tree
            _UpdatedSelectedTab(_tabView.SelectedIndex());
        }
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollUpPage(const IInspectable& /*sender*/,
                                           const ActionEventArgs& args)
    {
        _ScrollPage(-1);
        args.Handled(true);
    }

    void TerminalPage::_HandleScrollDownPage(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        _ScrollPage(1);
        args.Handled(true);
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
            const auto handled = _SelectTab({ realArgs.TabIndex() });
            args.Handled(handled);
        }
    }

    void TerminalPage::_HandleResizePane(const IInspectable& /*sender*/,
                                         const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<ResizePaneArgs>())
        {
            if (realArgs.Direction() == Direction::None)
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
                                        const ActionEventArgs& args)
    {
        if (const auto& realArgs = args.ActionArgs().try_as<MoveFocusArgs>())
        {
            if (realArgs.Direction() == Direction::None)
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

    void TerminalPage::_HandleToggleRetroEffect(const IInspectable& /*sender*/,
                                                const ActionEventArgs& args)
    {
        const auto termControl = _GetActiveControl();
        termControl.ToggleRetroEffect();
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
        // TODO GH#6677: When we add support for commandline mode, first set the
        // mode that the command palette should be in, before making it visible.
        CommandPalette().EnableCommandPaletteMode();
        CommandPalette().Visibility(CommandPalette().Visibility() == Visibility::Visible ?
                                        Visibility::Collapsed :
                                        Visibility::Visible);
        args.Handled(true);
    }

    void TerminalPage::_HandleSetColorScheme(const IInspectable& /*sender*/,
                                             const ActionEventArgs& args)
    {
        args.Handled(false);
        if (const auto& realArgs = args.ActionArgs().try_as<SetColorSchemeArgs>())
        {
            if (auto activeTab = _GetFocusedTab())
            {
                if (auto activeControl = activeTab->GetActiveTerminalControl())
                {
                    if (const auto scheme = _settings.GlobalSettings().ColorSchemes().TryLookup(realArgs.SchemeName()))
                    {
                        auto controlSettings = activeControl.Settings().as<TerminalSettings>();
                        controlSettings->ApplyColorScheme(scheme);
                        activeControl.UpdateSettings(*controlSettings);
                        args.Handled(true);
                    }
                }
            }
        }
    }

    void TerminalPage::_HandleSetTabColor(const IInspectable& /*sender*/,
                                          const ActionEventArgs& args)
    {
        std::optional<til::color> tabColor;

        if (const auto& realArgs = args.ActionArgs().try_as<SetTabColorArgs>())
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
                activeTab->SetRuntimeTabColor(tabColor.value());
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
        auto activeTab = _GetFocusedTab();
        if (activeTab)
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
                                                 const ActionEventArgs& actionArgs)
    {
        if (const auto& realArgs = actionArgs.ActionArgs().try_as<ExecuteCommandlineArgs>())
        {
            auto actions = winrt::single_threaded_vector<ActionAndArgs>(std::move(
                TerminalPage::ConvertExecuteCommandlineToActions(realArgs)));

            if (_startupActions.Size() != 0)
            {
                actionArgs.Handled(true);
                _ProcessStartupActions(actions, false);
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

            // Remove tabs after the current one
            while (_tabs.Size() > index + 1)
            {
                _RemoveTabViewItemByIndex(_tabs.Size() - 1);
            }

            // Remove all of them leading up to the selected tab
            while (_tabs.Size() > 1)
            {
                _RemoveTabViewItemByIndex(0);
            }

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

            // Remove tabs after the current one
            while (_tabs.Size() > index + 1)
            {
                _RemoveTabViewItemByIndex(_tabs.Size() - 1);
            }

            // TODO:GH#7182 For whatever reason, if you run this action
            // when the tab that's currently focused is _before_ the `index`
            // param, then the tabs will expand to fill the entire width of the
            // tab row, until you mouse over them. Probably has something to do
            // with tabs not resizing down until there's a mouse exit event.

            actionArgs.Handled(true);
        }
    }

    void TerminalPage::_HandleOpenTabSearch(const IInspectable& /*sender*/,
                                            const ActionEventArgs& args)
    {
        auto opt = _GetFocusedTabIndex();
        uint32_t startIdx = opt.value_or(0);

        CommandPalette().EnableTabSwitcherMode(true, startIdx);
        CommandPalette().Visibility(Visibility::Visible);

        args.Handled(true);
    }
}
