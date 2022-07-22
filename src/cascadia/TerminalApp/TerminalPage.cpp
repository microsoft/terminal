
// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TerminalPage.h"
#include "TerminalPage.g.cpp"
#include "RenameWindowRequestedArgs.g.cpp"

#include <filesystem>

#include <inc/WindowingBehavior.h>
#include <LibraryResources.h>
#include <TerminalCore/ControlKeyStates.hpp>
#include <til/latch.h>

#include "../../types/inc/utils.hpp"
#include "ColorHelper.h"
#include "DebugTapConnection.h"
#include "SettingsTab.h"
#include "TabRowControl.h"

using namespace winrt;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace winrt::Microsoft::Terminal::TerminalConnection;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::System;
using namespace winrt::Windows::System;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml;
using namespace ::TerminalApp;
using namespace ::Microsoft::Console;
using namespace ::Microsoft::Terminal::Core;
using namespace std::chrono_literals;

#define HOOKUP_ACTION(action) _actionDispatch->action({ this, &TerminalPage::_Handle##action });

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
    using VirtualKeyModifiers = Windows::System::VirtualKeyModifiers;
}

namespace winrt::TerminalApp::implementation
{
    TerminalPage::TerminalPage() :
        _tabs{ winrt::single_threaded_observable_vector<TerminalApp::TabBase>() },
        _mruTabs{ winrt::single_threaded_observable_vector<TerminalApp::TabBase>() },
        _startupActions{ winrt::single_threaded_vector<ActionAndArgs>() },
        _hostingHwnd{}
    {
        InitializeComponent();
    }

    // Method Description:
    // - implements the IInitializeWithWindow interface from shobjidl_core.
    // - We're going to use this HWND as the owner for the ConPTY windows, via
    //   ConptyConnection::ReparentWindow. We need this for applications that
    //   call GetConsoleWindow, and attempt to open a MessageBox for the
    //   console. By marking the conpty windows as owned by the Terminal HWND,
    //   the message box will be owned by the Terminal window as well.
    //   - see GH#2988
    HRESULT TerminalPage::Initialize(HWND hwnd)
    {
        if (!_hostingHwnd.has_value())
        {
            // GH#13211 - if we haven't yet set the owning hwnd, reparent all the controls now.
            for (const auto& tab : _tabs)
            {
                if (auto terminalTab{ _GetTerminalTabImpl(tab) })
                {
                    terminalTab->GetRootPane()->WalkTree([&](auto&& pane) {
                        if (const auto& term{ pane->GetTerminalControl() })
                        {
                            term.OwningHwnd(reinterpret_cast<uint64_t>(hwnd));
                        }
                    });
                }
                // We don't need to worry about resetting the owning hwnd for the
                // SUI here. GH#13211 only repros for a defterm connection, where
                // the tab is spawned before the window is created. It's not
                // possible to make a SUI tab like that, before the window is
                // created. The SUI could be spawned as a part of a window restore,
                // but that would still work fine. The window would be created
                // before restoring previous tabs in that scenario.
            }
        }
        _hostingHwnd = hwnd;
        return S_OK;
    }

    // Function Description:
    // - Recursively check our commands to see if there's a keybinding for
    //   exactly their action. If there is, label that command with the text
    //   corresponding to that key chord.
    // - Will recurse into nested commands as well.
    // Arguments:
    // - settings: The settings who's keybindings we should use to look up the key chords from
    // - commands: The list of commands to label.
    static void _recursiveUpdateCommandKeybindingLabels(CascadiaSettings settings,
                                                        IMapView<winrt::hstring, Command> commands)
    {
        for (const auto& nameAndCmd : commands)
        {
            const auto& command = nameAndCmd.Value();
            if (command.HasNestedCommands())
            {
                _recursiveUpdateCommandKeybindingLabels(settings, command.NestedCommands());
            }
            else
            {
                // If there's a keybinding that's bound to exactly this command,
                // then get the keychord and display it as a
                // part of the command in the UI.
                // We specifically need to do this for nested commands.
                const auto keyChord{ settings.ActionMap().GetKeyBindingForAction(command.ActionAndArgs().Action(), command.ActionAndArgs().Args()) };
                command.RegisterKey(keyChord);
            }
        }
    }

    void TerminalPage::SetSettings(CascadiaSettings settings, bool needRefreshUI)
    {
        _settings = settings;

        // Make sure to _UpdateCommandsForPalette before
        // _RefreshUIForSettingsReload. _UpdateCommandsForPalette will make
        // sure the KeyChordText of Commands is updated, which needs to
        // happen before the Settings UI is reloaded and tries to re-read
        // those values.
        _UpdateCommandsForPalette();
        CommandPalette().SetActionMap(_settings.ActionMap());

        if (needRefreshUI)
        {
            _RefreshUIForSettingsReload();
        }

        // Upon settings update we reload the system settings for scrolling as well.
        // TODO: consider reloading this value periodically.
        _systemRowsToScroll = _ReadSystemRowsToScroll();
    }

    bool TerminalPage::IsElevated() const noexcept
    {
        // use C++11 magic statics to make sure we only do this once.
        // This won't change over the lifetime of the application

        static const auto isElevated = []() {
            // *** THIS IS A SINGLETON ***
            auto result = false;

            // GH#2455 - Make sure to try/catch calls to Application::Current,
            // because that _won't_ be an instance of TerminalApp::App in the
            // LocalTests
            try
            {
                result = ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Logic().IsElevated();
            }
            CATCH_LOG();
            return result;
        }();

        return isElevated;
    }

    void TerminalPage::Create()
    {
        // Hookup the key bindings
        _HookupKeyBindings(_settings.ActionMap());

        _tabContent = this->TabContent();
        _tabRow = this->TabRow();
        _tabView = _tabRow.TabView();
        _rearranging = false;

        const auto isElevated = IsElevated();

        _tabRow.PointerMoved({ get_weak(), &TerminalPage::_RestorePointerCursorHandler });
        _tabView.CanReorderTabs(!isElevated);
        _tabView.CanDragTabs(!isElevated);
        _tabView.TabDragStarting({ get_weak(), &TerminalPage::_TabDragStarted });
        _tabView.TabDragCompleted({ get_weak(), &TerminalPage::_TabDragCompleted });

        auto tabRowImpl = winrt::get_self<implementation::TabRowControl>(_tabRow);
        _newTabButton = tabRowImpl->NewTabButton();

        if (_settings.GlobalSettings().ShowTabsInTitlebar())
        {
            // Remove the TabView from the page. We'll hang on to it, we need to
            // put it in the titlebar.
            uint32_t index = 0;
            if (this->Root().Children().IndexOf(_tabRow, index))
            {
                this->Root().Children().RemoveAt(index);
            }

            // Inform the host that our titlebar content has changed.
            _SetTitleBarContentHandlers(*this, _tabRow);

            // GH#13143 Manually set the tab row's background to transparent here.
            //
            // We're doing it this way because ThemeResources are tricky. We
            // default in XAML to using the appropriate ThemeResource background
            // color for our TabRow. When tabs in the titlebar are _disabled_,
            // this will ensure that the tab row has the correct theme-dependent
            // value. When tabs in the titlebar are _enabled_ (the default),
            // we'll switch the BG to Transparent, to let the Titlebar Control's
            // background be used as the BG for the tab row.
            //
            // We can't do it the other way around (default to Transparent, only
            // switch to a color when disabling tabs in the titlebar), because
            // looking up the correct ThemeResource from and App dictionary is a
            // capital-H Hard problem.
            const auto transparent = Media::SolidColorBrush();
            transparent.Color(Windows::UI::Colors::Transparent());
            _tabRow.Background(transparent);
        }
        _updateThemeColors();

        // Hookup our event handlers to the ShortcutActionDispatch
        _RegisterActionCallbacks();

        // Hook up inbound connection event handler
        ConptyConnection::NewConnection({ this, &TerminalPage::_OnNewConnection });

        //Event Bindings (Early)
        _newTabButton.Click([weakThis{ get_weak() }](auto&&, auto&&) {
            if (auto page{ weakThis.get() })
            {
                page->_OpenNewTerminalViaDropdown(NewTerminalArgs());
            }
        });
        _newTabButton.Drop([weakThis{ get_weak() }](const Windows::Foundation::IInspectable&, winrt::Windows::UI::Xaml::DragEventArgs e) {
            if (auto page{ weakThis.get() })
            {
                page->NewTerminalByDrop(e);
            }
        });
        _tabView.SelectionChanged({ this, &TerminalPage::_OnTabSelectionChanged });
        _tabView.TabCloseRequested({ this, &TerminalPage::_OnTabCloseRequested });
        _tabView.TabItemsChanged({ this, &TerminalPage::_OnTabItemsChanged });

        _CreateNewTabFlyout();

        _UpdateTabWidthMode();

        // When the visibility of the command palette changes to "collapsed",
        // the palette has been closed. Toss focus back to the currently active
        // control.
        CommandPalette().RegisterPropertyChangedCallback(UIElement::VisibilityProperty(), [this](auto&&, auto&&) {
            if (CommandPalette().Visibility() == Visibility::Collapsed)
            {
                _FocusActiveControl(nullptr, nullptr);
            }
        });
        CommandPalette().DispatchCommandRequested({ this, &TerminalPage::_OnDispatchCommandRequested });
        CommandPalette().CommandLineExecutionRequested({ this, &TerminalPage::_OnCommandLineExecutionRequested });
        CommandPalette().SwitchToTabRequested({ this, &TerminalPage::_OnSwitchToTabRequested });
        CommandPalette().PreviewAction({ this, &TerminalPage::_PreviewActionHandler });

        // Settings AllowDependentAnimations will affect whether animations are
        // enabled application-wide, so we don't need to check it each time we
        // want to create an animation.
        WUX::Media::Animation::Timeline::AllowDependentAnimations(!_settings.GlobalSettings().DisableAnimations());

        // Once the page is actually laid out on the screen, trigger all our
        // startup actions. Things like Panes need to know at least how big the
        // window will be, so they can subdivide that space.
        //
        // _OnFirstLayout will remove this handler so it doesn't get called more than once.
        _layoutUpdatedRevoker = _tabContent.LayoutUpdated(winrt::auto_revoke, { this, &TerminalPage::_OnFirstLayout });

        _isAlwaysOnTop = _settings.GlobalSettings().AlwaysOnTop();

        // DON'T set up Toasts/TeachingTips here. They should be loaded and
        // initialized the first time they're opened, in whatever method opens
        // them.

        // Setup mouse vanish attributes
        SystemParametersInfoW(SPI_GETMOUSEVANISH, 0, &_shouldMouseVanish, false);

        _tabRow.ShowElevationShield(IsElevated() && _settings.GlobalSettings().ShowAdminShield());

        // Store cursor, so we can restore it, e.g., after mouse vanishing
        // (we'll need to adapt this logic once we make cursor context aware)
        try
        {
            _defaultPointerCursor = CoreWindow::GetForCurrentThread().PointerCursor();
        }
        CATCH_LOG();

        ShowSetAsDefaultInfoBar();
    }

    Automation::Peers::AutomationPeer TerminalPage::OnCreateAutomationPeer()
    {
        _autoPeer = Automation::Peers::FrameworkElementAutomationPeer(*this);
        return _autoPeer;
    }

    // Method Description;
    // - Checks if the current terminal window should load or save its layout information.
    // Arguments:
    // - settings: The settings to use as this may be called before the page is
    //   fully initialized.
    // Return Value:
    // - true if the ApplicationState should be used.
    bool TerminalPage::ShouldUsePersistedLayout(CascadiaSettings& settings) const
    {
        return settings.GlobalSettings().FirstWindowPreference() == FirstWindowPreference::PersistedWindowLayout;
    }

    // Method Description:
    // - This is a bit of trickiness: If we're running unelevated, and the user
    //   passed in only --elevate actions, the we don't _actually_ want to
    //   restore the layouts here. We're not _actually_ about to create the
    //   window. We're simply going to toss the commandlines
    // Arguments:
    // - <none>
    // Return Value:
    // - true if we're not elevated but all relevant pane-spawning actions are elevated
    bool TerminalPage::ShouldImmediatelyHandoffToElevated(const CascadiaSettings& settings) const
    {
        // GH#12267: Don't forget about defterm handoff here. If we're being
        // created for embedding, then _yea_, we don't need to handoff to an
        // elevated window.
        if (!_startupActions || IsElevated() || _shouldStartInboundListener)
        {
            // there aren't startup actions, or we're elevated. In that case, go for it.
            return false;
        }

        // Check that there's at least one action that's not just an elevated newTab action.
        for (const auto& action : _startupActions)
        {
            NewTerminalArgs newTerminalArgs{ nullptr };

            if (action.Action() == ShortcutAction::NewTab)
            {
                const auto& args{ action.Args().try_as<NewTabArgs>() };
                if (args)
                {
                    newTerminalArgs = args.TerminalArgs();
                }
                else
                {
                    // This was a nt action that didn't have any args. The default
                    // profile may want to be elevated, so don't just early return.
                }
            }
            else if (action.Action() == ShortcutAction::SplitPane)
            {
                const auto& args{ action.Args().try_as<SplitPaneArgs>() };
                if (args)
                {
                    newTerminalArgs = args.TerminalArgs();
                }
                else
                {
                    // This was a nt action that didn't have any args. The default
                    // profile may want to be elevated, so don't just early return.
                }
            }
            else
            {
                // This was not a new tab or split pane action.
                // This doesn't affect the outcome
                continue;
            }

            // It's possible that newTerminalArgs is null here.
            // GetProfileForArgs should be resilient to that.
            const auto profile{ settings.GetProfileForArgs(newTerminalArgs) };
            if (profile.Elevate())
            {
                continue;
            }

            // The profile didn't want to be elevated, and we aren't elevated.
            // We're going to open at least one tab, so return false.
            return false;
        }
        return true;
    }

    // Method Description:
    // - Escape hatch for immediately dispatching requests to elevated windows
    //   when first launched. At this point in startup, the window doesn't exist
    //   yet, XAML hasn't been started, but we need to dispatch these actions.
    //   We can't just go through ProcessStartupActions, because that processes
    //   the actions async using the XAML dispatcher (which doesn't exist yet)
    // - DON'T CALL THIS if you haven't already checked
    //   ShouldImmediatelyHandoffToElevated. If you're thinking about calling
    //   this outside of the one place it's used, that's probably the wrong
    //   solution.
    // Arguments:
    // - settings: the settings we should use for dispatching these actions. At
    //   this point in startup, we hadn't otherwise been initialized with these,
    //   so use them now.
    // Return Value:
    // - <none>
    void TerminalPage::HandoffToElevated(const CascadiaSettings& settings)
    {
        if (!_startupActions)
        {
            return;
        }

        // Hookup our event handlers to the ShortcutActionDispatch
        _settings = settings;
        _HookupKeyBindings(_settings.ActionMap());
        _RegisterActionCallbacks();

        for (const auto& action : _startupActions)
        {
            // only process new tabs and split panes. They're all going to the elevated window anyways.
            if (action.Action() == ShortcutAction::NewTab || action.Action() == ShortcutAction::SplitPane)
            {
                _actionDispatch->DoAction(action);
            }
        }
    }

    // Method Description;
    // - Checks if the current window is configured to load a particular layout
    // Arguments:
    // - settings: The settings to use as this may be called before the page is
    //   fully initialized.
    // Return Value:
    // - non-null if there is a particular saved layout to use
    std::optional<uint32_t> TerminalPage::LoadPersistedLayoutIdx(CascadiaSettings& settings) const
    {
        return ShouldUsePersistedLayout(settings) ? _loadFromPersistedLayoutIdx : std::nullopt;
    }

    WindowLayout TerminalPage::LoadPersistedLayout(CascadiaSettings& settings) const
    {
        if (const auto idx = LoadPersistedLayoutIdx(settings))
        {
            const auto i = idx.value();
            const auto layouts = ApplicationState::SharedInstance().PersistedWindowLayouts();
            if (layouts && layouts.Size() > i)
            {
                return layouts.GetAt(i);
            }
        }
        return nullptr;
    }

    winrt::fire_and_forget TerminalPage::NewTerminalByDrop(winrt::Windows::UI::Xaml::DragEventArgs& e)
    {
        Windows::Foundation::Collections::IVectorView<Windows::Storage::IStorageItem> items;
        try
        {
            items = co_await e.DataView().GetStorageItemsAsync();
        }
        CATCH_LOG();

        if (items.Size() == 1)
        {
            std::filesystem::path path(items.GetAt(0).Path().c_str());
            if (!std::filesystem::is_directory(path))
            {
                path = path.parent_path();
            }

            NewTerminalArgs args;
            args.StartingDirectory(winrt::hstring{ path.wstring() });
            this->_OpenNewTerminalViaDropdown(args);

            TraceLoggingWrite(
                g_hTerminalAppProvider,
                "NewTabByDragDrop",
                TraceLoggingDescription("Event emitted when the user drag&drops onto the new tab button"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
        }
    }

    // Method Description:
    // - This method is called once command palette action was chosen for dispatching
    //   We'll use this event to dispatch this command.
    // Arguments:
    // - command - command to dispatch
    // Return Value:
    // - <none>
    void TerminalPage::_OnDispatchCommandRequested(const IInspectable& /*sender*/, const Microsoft::Terminal::Settings::Model::Command& command)
    {
        const auto& actionAndArgs = command.ActionAndArgs();
        _actionDispatch->DoAction(actionAndArgs);
    }

    // Method Description:
    // - This method is called once command palette command line was chosen for execution
    //   We'll use this event to create a command line execution command and dispatch it.
    // Arguments:
    // - command - command to dispatch
    // Return Value:
    // - <none>
    void TerminalPage::_OnCommandLineExecutionRequested(const IInspectable& /*sender*/, const winrt::hstring& commandLine)
    {
        ExecuteCommandlineArgs args{ commandLine };
        ActionAndArgs actionAndArgs{ ShortcutAction::ExecuteCommandline, args };
        _actionDispatch->DoAction(actionAndArgs);
    }

    // Method Description:
    // - This method is called once on startup, on the first LayoutUpdated event.
    //   We'll use this event to know that we have an ActualWidth and
    //   ActualHeight, so we can now attempt to process our list of startup
    //   actions.
    // - We'll remove this event handler when the event is first handled.
    // - If there are no startup actions, we'll open a single tab with the
    //   default profile.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void TerminalPage::_OnFirstLayout(const IInspectable& /*sender*/, const IInspectable& /*eventArgs*/)
    {
        // Only let this succeed once.
        _layoutUpdatedRevoker.revoke();

        // This event fires every time the layout changes, but it is always the
        // last one to fire in any layout change chain. That gives us great
        // flexibility in finding the right point at which to initialize our
        // renderer (and our terminal). Any earlier than the last layout update
        // and we may not know the terminal's starting size.
        if (_startupState == StartupState::NotInitialized)
        {
            _startupState = StartupState::InStartup;

            // If we are provided with an index, the cases where we have
            // commandline args and startup actions are already handled.
            if (const auto layout = LoadPersistedLayout(_settings))
            {
                if (layout.TabLayout().Size() > 0)
                {
                    _startupActions = layout.TabLayout();
                }
            }

            ProcessStartupActions(_startupActions, true);

            // If we were told that the COM server needs to be started to listen for incoming
            // default application connections, start it now.
            // This MUST be done after we've registered the event listener for the new connections
            // or the COM server might start receiving requests on another thread and dispatch
            // them to nowhere.
            _StartInboundListener();
        }
    }

    // Routine Description:
    // - Will start the listener for inbound console handoffs if we have already determined
    //   that we should do so.
    // NOTE: Must be after TerminalPage::_OnNewConnection has been connected up.
    // Arguments:
    // - <unused> - Looks at _shouldStartInboundListener
    // Return Value:
    // - <none> - May fail fast if setup fails as that would leave us in a weird state.
    void TerminalPage::_StartInboundListener()
    {
        if (_shouldStartInboundListener)
        {
            _shouldStartInboundListener = false;

            try
            {
                winrt::Microsoft::Terminal::TerminalConnection::ConptyConnection::StartInboundListener();
            }
            // If we failed to start the listener, it will throw.
            // We don't want to fail fast here because if a peasant has some trouble with
            // starting the listener, we don't want it to crash and take all its tabs down
            // with it.
            catch (...)
            {
                LOG_CAUGHT_EXCEPTION();
            }
        }
    }

    // Method Description:
    // - Process all the startup actions in the provided list of startup
    //   actions. We'll do this all at once here.
    // Arguments:
    // - actions: a winrt vector of actions to process. Note that this must NOT
    //   be an IVector&, because we need the collection to be accessible on the
    //   other side of the co_await.
    // - initial: if true, we're parsing these args during startup, and we
    //   should fire an Initialized event.
    // - cwd: If not empty, we should try switching to this provided directory
    //   while processing these actions. This will allow something like `wt -w 0
    //   nt -d .` from inside another directory to work as expected.
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalPage::ProcessStartupActions(Windows::Foundation::Collections::IVector<ActionAndArgs> actions,
                                                               const bool initial,
                                                               const winrt::hstring cwd)
    {
        auto weakThis{ get_weak() };

        // Handle it on a subsequent pass of the UI thread.
        co_await wil::resume_foreground(Dispatcher(), CoreDispatcherPriority::Normal);

        // If the caller provided a CWD, switch to that directory, then switch
        // back once we're done. This looks weird though, because we have to set
        // up the scope_exit _first_. We'll release the scope_exit if we don't
        // actually need it.
        auto originalCwd{ wil::GetCurrentDirectoryW<std::wstring>() };
        auto restoreCwd = wil::scope_exit([&originalCwd]() {
            // ignore errors, we'll just power on through. We'd rather do
            // something rather than fail silently if the directory doesn't
            // actually exist.
            LOG_IF_WIN32_BOOL_FALSE(SetCurrentDirectory(originalCwd.c_str()));
        });
        if (cwd.empty())
        {
            restoreCwd.release();
        }
        else
        {
            // ignore errors, we'll just power on through. We'd rather do
            // something rather than fail silently if the directory doesn't
            // actually exist.
            LOG_IF_WIN32_BOOL_FALSE(SetCurrentDirectory(cwd.c_str()));
        }

        if (auto page{ weakThis.get() })
        {
            for (const auto& action : actions)
            {
                if (auto page{ weakThis.get() })
                {
                    _actionDispatch->DoAction(action);
                }
                else
                {
                    co_return;
                }
            }

            // GH#6586: now that we're done processing all startup commands,
            // focus the active control. This will work as expected for both
            // commandline invocations and for `wt` action invocations.
            if (const auto control = _GetActiveControl())
            {
                control.Focus(FocusState::Programmatic);
            }
        }
        if (initial)
        {
            _CompleteInitialization();
        }
    }

    // Method Description:
    // - Perform and steps that need to be done once our initial state is all
    //   set up. This includes entering fullscreen mode and firing our
    //   Initialized event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_CompleteInitialization()
    {
        _startupState = StartupState::Initialized;

        // GH#632 - It's possible that the user tried to create the terminal
        // with only one tab, with only an elevated profile. If that happens,
        // we'll create _another_ process to host the elevated version of that
        // profile. This can happen from the jumplist, or if the default profile
        // is `elevate:true`, or from the commandline.
        //
        // However, we need to make sure to close this window in that scenario.
        // Since there aren't any _tabs_ in this window, we won't ever get a
        // closed event. So do it manually.
        //
        // GH#12267: Make sure that we don't instantly close ourselves when
        // we're readying to accept a defterm connection. In that case, we don't
        // have a tab yet, but will once we're initialized.
        if (_tabs.Size() == 0 && !(_shouldStartInboundListener || _isEmbeddingInboundListener))
        {
            _LastTabClosedHandlers(*this, nullptr);
        }
        else
        {
            _InitializedHandlers(*this, nullptr);
        }
    }

    // Method Description:
    // - Show a dialog with "About" information. Displays the app's Display
    //   Name, version, getting started link, source code link, documentation link, release
    //   Notes link, send feedback link and privacy policy link.
    void TerminalPage::_ShowAboutDialog()
    {
        _ShowDialogHelper(L"AboutDialog");
    }

    winrt::hstring TerminalPage::ApplicationDisplayName()
    {
        return CascadiaSettings::ApplicationDisplayName();
    }

    winrt::hstring TerminalPage::ApplicationVersion()
    {
        return CascadiaSettings::ApplicationVersion();
    }

    void TerminalPage::_SendFeedbackOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs& /*eventArgs*/)
    {
        ShellExecute(nullptr, nullptr, L"https://github.com/microsoft/terminal/issues", nullptr, nullptr, SW_SHOW);
    }

    void TerminalPage::_ThirdPartyNoticesOnClick(const IInspectable& /*sender*/, const Windows::UI::Xaml::RoutedEventArgs& /*eventArgs*/)
    {
        std::filesystem::path currentPath{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
        currentPath.replace_filename(L"NOTICE.html");
        ShellExecute(nullptr, nullptr, currentPath.c_str(), nullptr, nullptr, SW_SHOW);
    }

    // Method Description:
    // - Helper to show a content dialog
    // - We only open a content dialog if there isn't one open already
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> TerminalPage::_ShowDialogHelper(const std::wstring_view& name)
    {
        if (auto presenter{ _dialogPresenter.get() })
        {
            co_return co_await presenter.ShowDialog(FindName(name).try_as<WUX::Controls::ContentDialog>());
        }
        co_return ContentDialogResult::None;
    }

    // Method Description:
    // - Displays a dialog to warn the user that they are about to close all open windows.
    //   Once the user clicks the OK button, shut down the application.
    //   If cancel is clicked, the dialog will close.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> TerminalPage::_ShowQuitDialog()
    {
        return _ShowDialogHelper(L"QuitDialog");
    }

    // Method Description:
    // - Displays a dialog for warnings found while closing the terminal app using
    //   key binding with multiple tabs opened. Display messages to warn user
    //   that more than 1 tab is opened, and once the user clicks the OK button, remove
    //   all the tabs and shut down and app. If cancel is clicked, the dialog will close
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> TerminalPage::_ShowCloseWarningDialog()
    {
        return _ShowDialogHelper(L"CloseAllDialog");
    }

    // Method Description:
    // - Displays a dialog for warnings found while closing the terminal tab marked as read-only
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> TerminalPage::_ShowCloseReadOnlyDialog()
    {
        return _ShowDialogHelper(L"CloseReadOnlyDialog");
    }

    // Method Description:
    // - Displays a dialog to warn the user about the fact that the text that
    //   they are trying to paste contains the "new line" character which can
    //   have the effect of starting commands without the user's knowledge if
    //   it is pasted on a shell where the "new line" character marks the end
    //   of a command.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> TerminalPage::_ShowMultiLinePasteWarningDialog()
    {
        return _ShowDialogHelper(L"MultiLinePasteDialog");
    }

    // Method Description:
    // - Displays a dialog to warn the user about the fact that the text that
    //   they are trying to paste is very long, in case they did not mean to
    //   paste it but pressed the paste shortcut by accident.
    // - Only one dialog can be visible at a time. If another dialog is visible
    //   when this is called, nothing happens. See _ShowDialog for details
    winrt::Windows::Foundation::IAsyncOperation<ContentDialogResult> TerminalPage::_ShowLargePasteWarningDialog()
    {
        return _ShowDialogHelper(L"LargePasteDialog");
    }

    // Method Description:
    // - Builds the flyout (dropdown) attached to the new tab button, and
    //   attaches it to the button. Populates the flyout with one entry per
    //   Profile, displaying the profile's name. Clicking each flyout item will
    //   open a new tab with that profile.
    //   Below the profiles are the static menu items: settings, command palette
    void TerminalPage::_CreateNewTabFlyout()
    {
        auto newTabFlyout = WUX::Controls::MenuFlyout{};
        newTabFlyout.Placement(WUX::Controls::Primitives::FlyoutPlacementMode::BottomEdgeAlignedLeft);

        auto actionMap = _settings.ActionMap();
        const auto defaultProfileGuid = _settings.GlobalSettings().DefaultProfile();
        // the number of profiles should not change in the loop for this to work
        const auto profileCount = gsl::narrow_cast<int>(_settings.ActiveProfiles().Size());
        for (auto profileIndex = 0; profileIndex < profileCount; profileIndex++)
        {
            const auto profile = _settings.ActiveProfiles().GetAt(profileIndex);
            auto profileMenuItem = WUX::Controls::MenuFlyoutItem{};

            // Add the keyboard shortcuts based on the number of profiles defined
            // Look for a keychord that is bound to the equivalent
            // NewTab(ProfileIndex=N) action
            NewTerminalArgs newTerminalArgs{ profileIndex };
            NewTabArgs newTabArgs{ newTerminalArgs };
            auto profileKeyChord{ actionMap.GetKeyBindingForAction(ShortcutAction::NewTab, newTabArgs) };

            // make sure we find one to display
            if (profileKeyChord)
            {
                _SetAcceleratorForMenuItem(profileMenuItem, profileKeyChord);
            }

            auto profileName = profile.Name();
            profileMenuItem.Text(profileName);

            // If there's an icon set for this profile, set it as the icon for
            // this flyout item.
            if (!profile.Icon().empty())
            {
                const auto iconSource{ IconPathConverter().IconSourceWUX(profile.Icon()) };

                WUX::Controls::IconSourceElement iconElement;
                iconElement.IconSource(iconSource);
                profileMenuItem.Icon(iconElement);
                Automation::AutomationProperties::SetAccessibilityView(iconElement, Automation::Peers::AccessibilityView::Raw);
            }

            if (profile.Guid() == defaultProfileGuid)
            {
                // Contrast the default profile with others in font weight.
                profileMenuItem.FontWeight(FontWeights::Bold());
            }

            auto newTabRun = WUX::Documents::Run();
            newTabRun.Text(RS_(L"NewTabRun/Text"));
            auto newPaneRun = WUX::Documents::Run();
            newPaneRun.Text(RS_(L"NewPaneRun/Text"));
            newPaneRun.FontStyle(FontStyle::Italic);
            auto newWindowRun = WUX::Documents::Run();
            newWindowRun.Text(RS_(L"NewWindowRun/Text"));
            newWindowRun.FontStyle(FontStyle::Italic);
            auto elevatedRun = WUX::Documents::Run();
            elevatedRun.Text(RS_(L"ElevatedRun/Text"));
            elevatedRun.FontStyle(FontStyle::Italic);

            auto textBlock = WUX::Controls::TextBlock{};
            textBlock.Inlines().Append(newTabRun);
            textBlock.Inlines().Append(WUX::Documents::LineBreak{});
            textBlock.Inlines().Append(newPaneRun);
            textBlock.Inlines().Append(WUX::Documents::LineBreak{});
            textBlock.Inlines().Append(newWindowRun);
            textBlock.Inlines().Append(WUX::Documents::LineBreak{});
            textBlock.Inlines().Append(elevatedRun);

            auto toolTip = WUX::Controls::ToolTip{};
            toolTip.Content(textBlock);
            WUX::Controls::ToolTipService::SetToolTip(profileMenuItem, toolTip);

            profileMenuItem.Click([profileIndex, weakThis{ get_weak() }](auto&&, auto&&) {
                if (auto page{ weakThis.get() })
                {
                    NewTerminalArgs newTerminalArgs{ profileIndex };
                    page->_OpenNewTerminalViaDropdown(newTerminalArgs);
                }
            });
            newTabFlyout.Items().Append(profileMenuItem);
        }

        // add menu separator
        auto separatorItem = WUX::Controls::MenuFlyoutSeparator{};
        newTabFlyout.Items().Append(separatorItem);

        // add static items
        {
            // GH#2455 - Make sure to try/catch calls to Application::Current,
            // because that _won't_ be an instance of TerminalApp::App in the
            // LocalTests
            auto isUwp = false;
            try
            {
                isUwp = ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Logic().IsUwp();
            }
            CATCH_LOG();

            if (!isUwp)
            {
                // Create the settings button.
                auto settingsItem = WUX::Controls::MenuFlyoutItem{};
                settingsItem.Text(RS_(L"SettingsMenuItem"));

                WUX::Controls::SymbolIcon ico{};
                ico.Symbol(WUX::Controls::Symbol::Setting);
                settingsItem.Icon(ico);

                settingsItem.Click({ this, &TerminalPage::_SettingsButtonOnClick });
                newTabFlyout.Items().Append(settingsItem);

                const auto settingsKeyChord{ actionMap.GetKeyBindingForAction(ShortcutAction::OpenSettings, OpenSettingsArgs{ SettingsTarget::SettingsUI }) };
                if (settingsKeyChord)
                {
                    _SetAcceleratorForMenuItem(settingsItem, settingsKeyChord);
                }

                // Create the command palette button.
                auto commandPaletteFlyout = WUX::Controls::MenuFlyoutItem{};
                commandPaletteFlyout.Text(RS_(L"CommandPaletteMenuItem"));

                WUX::Controls::FontIcon commandPaletteIcon{};
                commandPaletteIcon.Glyph(L"\xE945");
                commandPaletteIcon.FontFamily(Media::FontFamily{ L"Segoe Fluent Icons, Segoe MDL2 Assets" });
                commandPaletteFlyout.Icon(commandPaletteIcon);

                commandPaletteFlyout.Click({ this, &TerminalPage::_CommandPaletteButtonOnClick });
                newTabFlyout.Items().Append(commandPaletteFlyout);

                const auto commandPaletteKeyChord{ actionMap.GetKeyBindingForAction(ShortcutAction::ToggleCommandPalette) };
                if (commandPaletteKeyChord)
                {
                    _SetAcceleratorForMenuItem(commandPaletteFlyout, commandPaletteKeyChord);
                }
            }

            // Create the about button.
            auto aboutFlyout = WUX::Controls::MenuFlyoutItem{};
            aboutFlyout.Text(RS_(L"AboutMenuItem"));

            WUX::Controls::SymbolIcon aboutIcon{};
            aboutIcon.Symbol(WUX::Controls::Symbol::Help);
            aboutFlyout.Icon(aboutIcon);

            aboutFlyout.Click({ this, &TerminalPage::_AboutButtonOnClick });
            newTabFlyout.Items().Append(aboutFlyout);
        }

        // Before opening the fly-out set focus on the current tab
        // so no matter how fly-out is closed later on the focus will return to some tab.
        // We cannot do it on closing because if the window loses focus (alt+tab)
        // the closing event is not fired.
        // It is important to set the focus on the tab
        // Since the previous focus location might be discarded in the background,
        // e.g., the command palette will be dismissed by the menu,
        // and then closing the fly-out will move the focus to wrong location.
        newTabFlyout.Opening([this](auto&&, auto&&) {
            _FocusCurrentTab(true);
        });
        _newTabButton.Flyout(newTabFlyout);
    }

    // Function Description:
    // Called when the openNewTabDropdown keybinding is used.
    // Shows the dropdown flyout.
    void TerminalPage::_OpenNewTabDropdown()
    {
        _newTabButton.Flyout().ShowAt(_newTabButton);
    }

    void TerminalPage::_OpenNewTerminalViaDropdown(const NewTerminalArgs newTerminalArgs)
    {
        // if alt is pressed, open a pane
        const auto window = CoreWindow::GetForCurrentThread();
        const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
        const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
        const auto altPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

        const auto shiftState{ window.GetKeyState(VirtualKey::Shift) };
        const auto rShiftState = window.GetKeyState(VirtualKey::RightShift);
        const auto lShiftState = window.GetKeyState(VirtualKey::LeftShift);
        const auto shiftPressed{ WI_IsFlagSet(shiftState, CoreVirtualKeyStates::Down) ||
                                 WI_IsFlagSet(lShiftState, CoreVirtualKeyStates::Down) ||
                                 WI_IsFlagSet(rShiftState, CoreVirtualKeyStates::Down) };

        const auto ctrlState{ window.GetKeyState(VirtualKey::Control) };
        const auto rCtrlState = window.GetKeyState(VirtualKey::RightControl);
        const auto lCtrlState = window.GetKeyState(VirtualKey::LeftControl);
        const auto ctrlPressed{ WI_IsFlagSet(ctrlState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(rCtrlState, CoreVirtualKeyStates::Down) ||
                                WI_IsFlagSet(lCtrlState, CoreVirtualKeyStates::Down) };

        // Check for DebugTap
        auto debugTap = this->_settings.GlobalSettings().DebugFeaturesEnabled() &&
                        WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) &&
                        WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);

        const auto dispatchToElevatedWindow = ctrlPressed && !IsElevated();

        if ((shiftPressed || dispatchToElevatedWindow) && !debugTap)
        {
            // Manually fill in the evaluated profile.
            if (newTerminalArgs.ProfileIndex() != nullptr)
            {
                // We want to promote the index to a GUID because there is no "launch to profile index" command.
                const auto profile = _settings.GetProfileForArgs(newTerminalArgs);
                if (profile)
                {
                    newTerminalArgs.Profile(::Microsoft::Console::Utils::GuidToString(profile.Guid()));
                }
            }

            if (dispatchToElevatedWindow)
            {
                _OpenElevatedWT(newTerminalArgs);
            }
            else
            {
                _OpenNewWindow(newTerminalArgs);
            }
        }
        else
        {
            const auto newPane = _MakePane(newTerminalArgs);
            // If the newTerminalArgs caused us to open an elevated window
            // instead of creating a pane, it may have returned nullptr. Just do
            // nothing then.
            if (!newPane)
            {
                return;
            }
            if (altPressed && !debugTap)
            {
                this->_SplitPane(SplitDirection::Automatic,
                                 0.5f,
                                 newPane);
            }
            else
            {
                _CreateNewTabFromPane(newPane);
            }
        }
    }

    winrt::fire_and_forget TerminalPage::_RemoveOnCloseRoutine(Microsoft::UI::Xaml::Controls::TabViewItem tabViewItem, winrt::com_ptr<TerminalPage> page)
    {
        co_await wil::resume_foreground(page->_tabView.Dispatcher());

        if (auto tab{ _GetTabByTabViewItem(tabViewItem) })
        {
            _RemoveTab(tab);
        }
    }

    // Method Description:
    // - Creates a new connection based on the profile settings
    // Arguments:
    // - the profile we want the settings from
    // - the terminal settings
    // Return value:
    // - the desired connection
    TerminalConnection::ITerminalConnection TerminalPage::_CreateConnectionFromSettings(Profile profile,
                                                                                        TerminalSettings settings)
    {
        TerminalConnection::ITerminalConnection connection{ nullptr };

        auto connectionType = profile.ConnectionType();
        winrt::guid sessionGuid{};

        if (connectionType == TerminalConnection::AzureConnection::ConnectionType() &&
            TerminalConnection::AzureConnection::IsAzureConnectionAvailable())
        {
            // TODO GH#4661: Replace this with directly using the AzCon when our VT is better
            std::filesystem::path azBridgePath{ wil::GetModuleFileNameW<std::wstring>(nullptr) };
            azBridgePath.replace_filename(L"TerminalAzBridge.exe");
            connection = TerminalConnection::ConptyConnection();
            auto valueSet = TerminalConnection::ConptyConnection::CreateSettings(azBridgePath.wstring(),
                                                                                 L".",
                                                                                 L"Azure",
                                                                                 nullptr,
                                                                                 settings.InitialRows(),
                                                                                 settings.InitialCols(),
                                                                                 winrt::guid());

            if constexpr (Feature_VtPassthroughMode::IsEnabled())
            {
                valueSet.Insert(L"passthroughMode", Windows::Foundation::PropertyValue::CreateBoolean(settings.VtPassthrough()));
            }

            connection.Initialize(valueSet);
        }

        else
        {
            // profile is guaranteed to exist here
            auto guidWString = Utils::GuidToString(profile.Guid());

            StringMap envMap{};
            envMap.Insert(L"WT_PROFILE_ID", guidWString);
            envMap.Insert(L"WSLENV", L"WT_PROFILE_ID");

            // Update the path to be relative to whatever our CWD is.
            //
            // Refer to the examples in
            // https://en.cppreference.com/w/cpp/filesystem/path/append
            //
            // We need to do this here, to ensure we tell the ConptyConnection
            // the correct starting path. If we're being invoked from another
            // terminal instance (e.g. wt -w 0 -d .), then we have switched our
            // CWD to the provided path. We should treat the StartingDirectory
            // as relative to the current CWD.
            //
            // The connection must be informed of the current CWD on
            // construction, because the connection might not spawn the child
            // process until later, on another thread, after we've already
            // restored the CWD to it's original value.
            auto newWorkingDirectory{ settings.StartingDirectory() };
            if (newWorkingDirectory.size() == 0 || newWorkingDirectory.size() == 1 &&
                                                       !(newWorkingDirectory[0] == L'~' || newWorkingDirectory[0] == L'/'))
            { // We only want to resolve the new WD against the CWD if it doesn't look like a Linux path (see GH#592)
                auto cwdString{ wil::GetCurrentDirectoryW<std::wstring>() };
                std::filesystem::path cwd{ cwdString };
                cwd /= settings.StartingDirectory().c_str();
                newWorkingDirectory = winrt::hstring{ cwd.wstring() };
            }

            auto conhostConn = TerminalConnection::ConptyConnection();
            auto valueSet = TerminalConnection::ConptyConnection::CreateSettings(settings.Commandline(),
                                                                                 newWorkingDirectory,
                                                                                 settings.StartingTitle(),
                                                                                 envMap.GetView(),
                                                                                 settings.InitialRows(),
                                                                                 settings.InitialCols(),
                                                                                 winrt::guid());

            valueSet.Insert(L"passthroughMode", Windows::Foundation::PropertyValue::CreateBoolean(settings.VtPassthrough()));

            conhostConn.Initialize(valueSet);

            sessionGuid = conhostConn.Guid();
            connection = conhostConn;
        }

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "ConnectionCreated",
            TraceLoggingDescription("Event emitted upon the creation of a connection"),
            TraceLoggingGuid(connectionType, "ConnectionTypeGuid", "The type of the connection"),
            TraceLoggingGuid(profile.Guid(), "ProfileGuid", "The profile's GUID"),
            TraceLoggingGuid(sessionGuid, "SessionGuid", "The WT_SESSION's GUID"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));

        return connection;
    }

    // Method Description:
    // - Called when the settings button is clicked. Launches a background
    //   thread to open the settings file in the default JSON editor.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_SettingsButtonOnClick(const IInspectable&,
                                              const RoutedEventArgs&)
    {
        const auto window = CoreWindow::GetForCurrentThread();

        // check alt state
        const auto rAltState{ window.GetKeyState(VirtualKey::RightMenu) };
        const auto lAltState{ window.GetKeyState(VirtualKey::LeftMenu) };
        const auto altPressed{ WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) ||
                               WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down) };

        // check shift state
        const auto shiftState{ window.GetKeyState(VirtualKey::Shift) };
        const auto lShiftState{ window.GetKeyState(VirtualKey::LeftShift) };
        const auto rShiftState{ window.GetKeyState(VirtualKey::RightShift) };
        const auto shiftPressed{ WI_IsFlagSet(shiftState, CoreVirtualKeyStates::Down) ||
                                 WI_IsFlagSet(lShiftState, CoreVirtualKeyStates::Down) ||
                                 WI_IsFlagSet(rShiftState, CoreVirtualKeyStates::Down) };

        auto target{ SettingsTarget::SettingsUI };
        if (shiftPressed)
        {
            target = SettingsTarget::SettingsFile;
        }
        else if (altPressed)
        {
            target = SettingsTarget::DefaultsFile;
        }
        _LaunchSettings(target);
    }

    // Method Description:
    // - Called when the command palette button is clicked. Opens the command palette.
    void TerminalPage::_CommandPaletteButtonOnClick(const IInspectable&,
                                                    const RoutedEventArgs&)
    {
        CommandPalette().EnableCommandPaletteMode(CommandPaletteLaunchMode::Action);
        CommandPalette().Visibility(Visibility::Visible);
    }

    // Method Description:
    // - Called when the about button is clicked. See _ShowAboutDialog for more info.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void TerminalPage::_AboutButtonOnClick(const IInspectable&,
                                           const RoutedEventArgs&)
    {
        _ShowAboutDialog();
    }

    // Method Description:
    // - Called when the users pressed keyBindings while CommandPalette is open.
    // - As of GH#8480, this is also bound to the TabRowControl's KeyUp event.
    //   That should only fire when focus is in the tab row, which is hard to
    //   do. Notably, that's possible:
    //   - When you have enough tabs to make the little scroll arrows appear,
    //     click one, then hit tab
    //   - When Narrator is in Scan mode (which is the a11y bug we're fixing here)
    // - This method is effectively an extract of TermControl::_KeyHandler and TermControl::_TryHandleKeyBinding.
    // Arguments:
    // - e: the KeyRoutedEventArgs containing info about the keystroke.
    // Return Value:
    // - <none>
    void TerminalPage::_KeyDownHandler(const Windows::Foundation::IInspectable& /*sender*/, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto keyStatus = e.KeyStatus();
        const auto vkey = gsl::narrow_cast<WORD>(e.OriginalKey());
        const auto scanCode = gsl::narrow_cast<WORD>(keyStatus.ScanCode);
        const auto modifiers = _GetPressedModifierKeys();

        // GH#11076:
        // For some weird reason we sometimes receive a WM_KEYDOWN
        // message without vkey or scanCode if a user drags a tab.
        // The KeyChord constructor has a debug assertion ensuring that all KeyChord
        // either have a valid vkey/scanCode. This is important, because this prevents
        // accidental insertion of invalid KeyChords into classes like ActionMap.
        if (!vkey && !scanCode)
        {
            return;
        }

        // Alt-Numpad# input will send us a character once the user releases
        // Alt, so we should be ignoring the individual keydowns. The character
        // will be sent through the TSFInputControl. See GH#1401 for more
        // details
        if (modifiers.IsAltPressed() && (vkey >= VK_NUMPAD0 && vkey <= VK_NUMPAD9))
        {
            return;
        }

        // GH#2235: Terminal::Settings hasn't been modified to differentiate
        // between AltGr and Ctrl+Alt yet.
        // -> Don't check for key bindings if this is an AltGr key combination.
        if (modifiers.IsAltGrPressed())
        {
            return;
        }

        const auto actionMap = _settings.ActionMap();
        if (!actionMap)
        {
            return;
        }

        const auto cmd = actionMap.GetActionByKeyChord({
            modifiers.IsCtrlPressed(),
            modifiers.IsAltPressed(),
            modifiers.IsShiftPressed(),
            modifiers.IsWinPressed(),
            vkey,
            scanCode,
        });
        if (!cmd)
        {
            return;
        }

        if (!_actionDispatch->DoAction(cmd.ActionAndArgs()))
        {
            return;
        }

        if (const auto p = CommandPalette(); p.Visibility() == Visibility::Visible && cmd.ActionAndArgs().Action() != ShortcutAction::ToggleCommandPalette)
        {
            p.Visibility(Visibility::Collapsed);
        }

        // Let's assume the user has bound the dead key "^" to a sendInput command that sends "b".
        // If the user presses the two keys "^a" it'll produce "bâ", despite us marking the key event as handled.
        // The following is used to manually "consume" such dead keys and clear them from the keyboard state.
        _ClearKeyboardState(vkey, scanCode);
        e.Handled(true);
    }

    // Method Description:
    // - Get the modifier keys that are currently pressed. This can be used to
    //   find out which modifiers (ctrl, alt, shift) are pressed in events that
    //   don't necessarily include that state.
    // - This is a copy of TermControl::_GetPressedModifierKeys.
    // Return Value:
    // - The Microsoft::Terminal::Core::ControlKeyStates representing the modifier key states.
    ControlKeyStates TerminalPage::_GetPressedModifierKeys() noexcept
    {
        const auto window = CoreWindow::GetForCurrentThread();
        // DONT USE
        //      != CoreVirtualKeyStates::None
        // OR
        //      == CoreVirtualKeyStates::Down
        // Sometimes with the key down, the state is Down | Locked.
        // Sometimes with the key up, the state is Locked.
        // IsFlagSet(Down) is the only correct solution.

        struct KeyModifier
        {
            VirtualKey vkey;
            ControlKeyStates flags;
        };

        constexpr std::array<KeyModifier, 7> modifiers{ {
            { VirtualKey::RightMenu, ControlKeyStates::RightAltPressed },
            { VirtualKey::LeftMenu, ControlKeyStates::LeftAltPressed },
            { VirtualKey::RightControl, ControlKeyStates::RightCtrlPressed },
            { VirtualKey::LeftControl, ControlKeyStates::LeftCtrlPressed },
            { VirtualKey::Shift, ControlKeyStates::ShiftPressed },
            { VirtualKey::RightWindows, ControlKeyStates::RightWinPressed },
            { VirtualKey::LeftWindows, ControlKeyStates::LeftWinPressed },
        } };

        ControlKeyStates flags;

        for (const auto& mod : modifiers)
        {
            const auto state = window.GetKeyState(mod.vkey);
            const auto isDown = WI_IsFlagSet(state, CoreVirtualKeyStates::Down);

            if (isDown)
            {
                flags |= mod.flags;
            }
        }

        return flags;
    }

    // Method Description:
    // - Discards currently pressed dead keys.
    // - This is a copy of TermControl::_ClearKeyboardState.
    // Arguments:
    // - vkey: The vkey of the key pressed.
    // - scanCode: The scan code of the key pressed.
    void TerminalPage::_ClearKeyboardState(const WORD vkey, const WORD scanCode) noexcept
    {
        std::array<BYTE, 256> keyState;
        if (!GetKeyboardState(keyState.data()))
        {
            return;
        }

        // As described in "Sometimes you *want* to interfere with the keyboard's state buffer":
        //   http://archives.miloush.net/michkap/archive/2006/09/10/748775.html
        // > "The key here is to keep trying to pass stuff to ToUnicode until -1 is not returned."
        std::array<wchar_t, 16> buffer;
        while (ToUnicodeEx(vkey, scanCode, keyState.data(), buffer.data(), gsl::narrow_cast<int>(buffer.size()), 0b1, nullptr) < 0)
        {
        }
    }

    // Method Description:
    // - Configure the AppKeyBindings to use our ShortcutActionDispatch and the updated ActionMap
    //    as the object to handle dispatching ShortcutAction events.
    // Arguments:
    // - bindings: An IActionMapView object to wire up with our event handlers
    void TerminalPage::_HookupKeyBindings(const IActionMapView& actionMap) noexcept
    {
        _bindings->SetDispatch(*_actionDispatch);
        _bindings->SetActionMap(actionMap);
    }

    // Method Description:
    // - Register our event handlers with our ShortcutActionDispatch. The
    //   ShortcutActionDispatch is responsible for raising the appropriate
    //   events for an ActionAndArgs. WE'll handle each possible event in our
    //   own way.
    // Arguments:
    // - <none>
    void TerminalPage::_RegisterActionCallbacks()
    {
        // Hook up the ShortcutActionDispatch object's events to our handlers.
        // They should all be hooked up here, regardless of whether or not
        // there's an actual keychord for them.
#define ON_ALL_ACTIONS(action) HOOKUP_ACTION(action);
        ALL_SHORTCUT_ACTIONS
#undef ON_ALL_ACTIONS
    }

    // Method Description:
    // - Get the title of the currently focused terminal control. If this tab is
    //   the focused tab, then also bubble this title to any listeners of our
    //   TitleChanged event.
    // Arguments:
    // - tab: the Tab to update the title for.
    void TerminalPage::_UpdateTitle(const TerminalTab& tab)
    {
        auto newTabTitle = tab.Title();

        if (tab == _GetFocusedTab())
        {
            _TitleChangedHandlers(*this, newTabTitle);
        }
    }

    // Method Description:
    // - Connects event handlers to the TermControl for events that we want to
    //   handle. This includes:
    //    * the Copy and Paste events, for setting and retrieving clipboard data
    //      on the right thread
    // Arguments:
    // - term: The newly created TermControl to connect the events for
    void TerminalPage::_RegisterTerminalEvents(TermControl term)
    {
        term.RaiseNotice({ this, &TerminalPage::_ControlNoticeRaisedHandler });

        // Add an event handler when the terminal's selection wants to be copied.
        // When the text buffer data is retrieved, we'll copy the data into the Clipboard
        term.CopyToClipboard({ this, &TerminalPage::_CopyToClipboardHandler });

        // Add an event handler when the terminal wants to paste data from the Clipboard.
        term.PasteFromClipboard({ this, &TerminalPage::_PasteFromClipboardHandler });

        term.OpenHyperlink({ this, &TerminalPage::_OpenHyperlinkHandler });

        term.HidePointerCursor({ get_weak(), &TerminalPage::_HidePointerCursorHandler });
        term.RestorePointerCursor({ get_weak(), &TerminalPage::_RestorePointerCursorHandler });
        // Add an event handler for when the terminal or tab wants to set a
        // progress indicator on the taskbar
        term.SetTaskbarProgress({ get_weak(), &TerminalPage::_SetTaskbarProgressHandler });

        term.ConnectionStateChanged({ get_weak(), &TerminalPage::_ConnectionStateChangedHandler });

        term.PropertyChanged([weakThis = get_weak()](auto& /*sender*/, auto& e) {
            if (auto page{ weakThis.get() })
            {
                if (e.PropertyName() == L"BackgroundBrush")
                {
                    page->_updateThemeColors();
                }
            }
        });

        term.ShowWindowChanged({ get_weak(), &TerminalPage::_ShowWindowChangedHandler });
    }

    // Method Description:
    // - Connects event handlers to the TerminalTab for events that we want to
    //   handle. This includes:
    //    * the TitleChanged event, for changing the text of the tab
    //    * the Color{Selected,Cleared} events to change the color of a tab.
    // Arguments:
    // - hostingTab: The Tab that's hosting this TermControl instance
    void TerminalPage::_RegisterTabEvents(TerminalTab& hostingTab)
    {
        auto weakTab{ hostingTab.get_weak() };
        auto weakThis{ get_weak() };
        // PropertyChanged is the generic mechanism by which the Tab
        // communicates changes to any of its observable properties, including
        // the Title
        hostingTab.PropertyChanged([weakTab, weakThis](auto&&, const WUX::Data::PropertyChangedEventArgs& args) {
            auto page{ weakThis.get() };
            auto tab{ weakTab.get() };
            if (page && tab)
            {
                if (args.PropertyName() == L"Title")
                {
                    page->_UpdateTitle(*tab);
                }
                else if (args.PropertyName() == L"Content")
                {
                    if (*tab == page->_GetFocusedTab())
                    {
                        page->_tabContent.Children().Clear();
                        page->_tabContent.Children().Append(tab->Content());

                        tab->Focus(FocusState::Programmatic);
                    }
                }
            }
        });

        // Add an event handler for when the terminal or tab wants to set a
        // progress indicator on the taskbar
        hostingTab.TaskbarProgressChanged({ get_weak(), &TerminalPage::_SetTaskbarProgressHandler });
    }

    // Method Description:
    // - Helper to manually exit "zoom" when certain actions take place.
    //   Anything that modifies the state of the pane tree should probably
    //   un-zoom the focused pane first, so that the user can see the full pane
    //   tree again. These actions include:
    //   * Splitting a new pane
    //   * Closing a pane
    //   * Moving focus between panes
    //   * Resizing a pane
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_UnZoomIfNeeded()
    {
        if (const auto activeTab{ _GetFocusedTabImpl() })
        {
            if (activeTab->IsZoomed())
            {
                // Remove the content from the tab first, so Pane::UnZoom can
                // re-attach the content to the tree w/in the pane
                _tabContent.Children().Clear();
                // In ExitZoom, we'll change the Tab's Content(), triggering the
                // content changed event, which will re-attach the tab's new content
                // root to the tree.
                activeTab->ExitZoom();
            }
        }
    }

    // Method Description:
    // - Attempt to move focus between panes, as to focus the child on
    //   the other side of the separator. See Pane::NavigateFocus for details.
    // - Moves the focus of the currently focused tab.
    // Arguments:
    // - direction: The direction to move the focus in.
    // Return Value:
    // - Whether changing the focus succeeded. This allows a keychord to propagate
    //   to the terminal when no other panes are present (GH#6219)
    bool TerminalPage::_MoveFocus(const FocusDirection& direction)
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            return terminalTab->NavigateFocus(direction);
        }
        return false;
    }

    // Method Description:
    // - Attempt to swap the positions of the focused pane with another pane.
    //   See Pane::SwapPane for details.
    // Arguments:
    // - direction: The direction to move the focused pane in.
    // Return Value:
    // - true if panes were swapped.
    bool TerminalPage::_SwapPane(const FocusDirection& direction)
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            _UnZoomIfNeeded();
            return terminalTab->SwapPane(direction);
        }
        return false;
    }

    TermControl TerminalPage::_GetActiveControl()
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            return terminalTab->GetActiveTerminalControl();
        }
        return nullptr;
    }
    // Method Description:
    // - Warn the user that they are about to close all open windows, then
    //   signal that we want to close everything.
    fire_and_forget TerminalPage::RequestQuit()
    {
        if (!_displayingCloseDialog)
        {
            _displayingCloseDialog = true;
            auto warningResult = co_await _ShowQuitDialog();
            _displayingCloseDialog = false;

            if (warningResult != ContentDialogResult::Primary)
            {
                co_return;
            }

            _QuitRequestedHandlers(nullptr, nullptr);
        }
    }

    // Method Description:
    // - Saves the window position and tab layout to the application state
    // - This does not create the InitialPosition field, that needs to be
    //   added externally.
    // Arguments:
    // - <none>
    // Return Value:
    // - the window layout
    WindowLayout TerminalPage::GetWindowLayout()
    {
        if (_startupState != StartupState::Initialized)
        {
            return nullptr;
        }

        std::vector<ActionAndArgs> actions;

        for (auto tab : _tabs)
        {
            auto t = winrt::get_self<implementation::TabBase>(tab);
            auto tabActions = t->BuildStartupActions();
            actions.insert(actions.end(), std::make_move_iterator(tabActions.begin()), std::make_move_iterator(tabActions.end()));
        }

        // if the focused tab was not the last tab, restore that
        auto idx = _GetFocusedTabIndex();
        if (idx && idx != _tabs.Size() - 1)
        {
            ActionAndArgs action;
            action.Action(ShortcutAction::SwitchToTab);
            SwitchToTabArgs switchToTabArgs{ idx.value() };
            action.Args(switchToTabArgs);

            actions.emplace_back(std::move(action));
        }

        // If the user set a custom name, save it
        if (_WindowName != L"")
        {
            ActionAndArgs action;
            action.Action(ShortcutAction::RenameWindow);
            RenameWindowArgs args{ _WindowName };
            action.Args(args);

            actions.emplace_back(std::move(action));
        }

        WindowLayout layout{};
        layout.TabLayout(winrt::single_threaded_vector<ActionAndArgs>(std::move(actions)));

        auto mode = LaunchMode::DefaultMode;
        WI_SetFlagIf(mode, LaunchMode::FullscreenMode, _isFullscreen);
        WI_SetFlagIf(mode, LaunchMode::FocusMode, _isInFocusMode);
        WI_SetFlagIf(mode, LaunchMode::MaximizedMode, _isMaximized);

        layout.LaunchMode({ mode });

        // Only save the content size because the tab size will be added on load.
        const auto contentWidth = ::base::saturated_cast<float>(_tabContent.ActualWidth());
        const auto contentHeight = ::base::saturated_cast<float>(_tabContent.ActualHeight());
        const winrt::Windows::Foundation::Size windowSize{ contentWidth, contentHeight };

        layout.InitialSize(windowSize);

        return layout;
    }

    // Method Description:
    // - Close the terminal app. If there is more
    //   than one tab opened, show a warning dialog.
    // Arguments:
    // - bypassDialog: if true a dialog won't be shown even if the user would
    //   normally get confirmation. This is used in the case where the user
    //   has already been prompted by the Quit action.
    fire_and_forget TerminalPage::CloseWindow(bool bypassDialog)
    {
        if (!bypassDialog &&
            _HasMultipleTabs() &&
            _settings.GlobalSettings().ConfirmCloseAllTabs() &&
            !_displayingCloseDialog)
        {
            _displayingCloseDialog = true;
            auto warningResult = co_await _ShowCloseWarningDialog();
            _displayingCloseDialog = false;

            if (warningResult != ContentDialogResult::Primary)
            {
                co_return;
            }
        }

        if (ShouldUsePersistedLayout(_settings))
        {
            // Don't delete the ApplicationState when all of the tabs are removed.
            // If there is still a monarch living they will get the event that
            // a window closed and trigger a new save without this window.
            _maintainStateOnTabClose = true;
        }

        _RemoveAllTabs();
    }

    // Method Description:
    // - Move the viewport of the terminal of the currently focused tab up or
    //      down a number of lines.
    // Arguments:
    // - scrollDirection: ScrollUp will move the viewport up, ScrollDown will move the viewport down
    // - rowsToScroll: a number of lines to move the viewport. If not provided we will use a system default.
    void TerminalPage::_Scroll(ScrollDirection scrollDirection, const Windows::Foundation::IReference<uint32_t>& rowsToScroll)
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            uint32_t realRowsToScroll;
            if (rowsToScroll == nullptr)
            {
                // The magic value of WHEEL_PAGESCROLL indicates that we need to scroll the entire page
                realRowsToScroll = _systemRowsToScroll == WHEEL_PAGESCROLL ?
                                       terminalTab->GetActiveTerminalControl().ViewHeight() :
                                       _systemRowsToScroll;
            }
            else
            {
                // use the custom value specified in the command
                realRowsToScroll = rowsToScroll.Value();
            }
            auto scrollDelta = _ComputeScrollDelta(scrollDirection, realRowsToScroll);
            terminalTab->Scroll(scrollDelta);
        }
    }

    // Method Description:
    // - Moves the currently active pane on the currently active tab to the
    //   specified tab. If the tab index is greater than the number of
    //   tabs, then a new tab will be created for the pane. Similarly, if a pane
    //   is the last remaining pane on a tab, that tab will be closed upon moving.
    // - No move will occur if the tabIdx is the same as the current tab, or if
    //   the specified tab is not a host of terminals (such as the settings tab).
    // Arguments:
    // - tabIdx: The target tab index.
    // Return Value:
    // - true if the pane was successfully moved to the new tab.
    bool TerminalPage::_MovePane(const uint32_t tabIdx)
    {
        auto focusedTab{ _GetFocusedTabImpl() };

        if (!focusedTab)
        {
            return false;
        }

        // If we are trying to move from the current tab to the current tab do nothing.
        if (_GetFocusedTabIndex() == tabIdx)
        {
            return false;
        }

        // Moving the pane from the current tab might close it, so get the next
        // tab before its index changes.
        if (_tabs.Size() > tabIdx)
        {
            auto targetTab = _GetTerminalTabImpl(_tabs.GetAt(tabIdx));
            // if the selected tab is not a host of terminals (e.g. settings)
            // don't attempt to add a pane to it.
            if (!targetTab)
            {
                return false;
            }
            auto pane = focusedTab->DetachPane();
            targetTab->AttachPane(pane);
            _SetFocusedTab(*targetTab);
        }
        else
        {
            auto pane = focusedTab->DetachPane();
            _CreateNewTabFromPane(pane);
        }

        return true;
    }

    // Method Description:
    // - Split the focused pane either horizontally or vertically, and place the
    //   given pane accordingly in the tree
    // Arguments:
    // - newPane: the pane to add to our tree of panes
    // - splitDirection: one value from the TerminalApp::SplitDirection enum, indicating how the
    //   new pane should be split from its parent.
    // - splitSize: the size of the split
    void TerminalPage::_SplitPane(const SplitDirection splitDirection,
                                  const float splitSize,
                                  std::shared_ptr<Pane> newPane)
    {
        const auto focusedTab{ _GetFocusedTabImpl() };

        // Clever hack for a crash in startup, with multiple sub-commands. Say
        // you have the following commandline:
        //
        //   wtd nt -p "elevated cmd" ; sp -p "elevated cmd" ; sp -p "Command Prompt"
        //
        // Where "elevated cmd" is an elevated profile.
        //
        // In that scenario, we won't dump off the commandline immediately to an
        // elevated window, because it's got the final unelevated split in it.
        // However, when we get to that command, there won't be a tab yet. So
        // we'd crash right about here.
        //
        // Instead, let's just promote this first split to be a tab instead.
        // Crash avoided, and we don't need to worry about inserting a new-tab
        // command in at the start.
        if (!focusedTab)
        {
            if (_tabs.Size() == 0)
            {
                _CreateNewTabFromPane(newPane);
            }
            else
            {
                // The focused tab isn't a terminal tab
                return;
            }
        }
        else
        {
            _SplitPane(*focusedTab, splitDirection, splitSize, newPane);
        }
    }

    // Method Description:
    // - Split the focused pane of the given tab, either horizontally or vertically, and place the
    //   given pane accordingly
    // Arguments:
    // - tab: The tab that is going to be split.
    // - newPane: the pane to add to our tree of panes
    // - splitDirection: one value from the TerminalApp::SplitDirection enum, indicating how the
    //   new pane should be split from its parent.
    // - splitSize: the size of the split
    void TerminalPage::_SplitPane(TerminalTab& tab,
                                  const SplitDirection splitDirection,
                                  const float splitSize,
                                  std::shared_ptr<Pane> newPane)
    {
        // If the caller is calling us with the return value of _MakePane
        // directly, it's possible that nullptr was returned, if the connections
        // was supposed to be launched in an elevated window. In that case, do
        // nothing here. We don't have a pane with which to create the split.
        if (!newPane)
        {
            return;
        }
        const auto contentWidth = ::base::saturated_cast<float>(_tabContent.ActualWidth());
        const auto contentHeight = ::base::saturated_cast<float>(_tabContent.ActualHeight());
        const winrt::Windows::Foundation::Size availableSpace{ contentWidth, contentHeight };

        const auto realSplitType = tab.PreCalculateCanSplit(splitDirection, splitSize, availableSpace);
        if (!realSplitType)
        {
            return;
        }

        _UnZoomIfNeeded();
        tab.SplitPane(*realSplitType, splitSize, newPane);

        if (_autoPeer)
        {
            // we can't check if this is a leaf pane,
            // but getting the profile returns null if we aren't, so that works!
            if (const auto profile{ newPane->GetProfile() })
            {
                _autoPeer.RaiseNotificationEvent(
                    Automation::Peers::AutomationNotificationKind::ActionCompleted,
                    Automation::Peers::AutomationNotificationProcessing::ImportantMostRecent,
                    fmt::format(std::wstring_view{ RS_(L"SplitPaneAnnouncement") }, profile.Name()),
                    L"NewSplitPane" /* unique name for this notification category */);
            }
        }

        // After GH#6586, the control will no longer focus itself
        // automatically when it's finished being laid out. Manually focus
        // the control here instead.
        if (_startupState == StartupState::Initialized)
        {
            if (const auto control = _GetActiveControl())
            {
                control.Focus(FocusState::Programmatic);
            }
        }
    }

    // Method Description:
    // - Switches the split orientation of the currently focused pane.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_ToggleSplitOrientation()
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            _UnZoomIfNeeded();
            terminalTab->ToggleSplitOrientation();
        }
    }

    // Method Description:
    // - Attempt to move a separator between panes, as to resize each child on
    //   either size of the separator. See Pane::ResizePane for details.
    // - Moves a separator on the currently focused tab.
    // Arguments:
    // - direction: The direction to move the separator in.
    // Return Value:
    // - <none>
    void TerminalPage::_ResizePane(const ResizeDirection& direction)
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            _UnZoomIfNeeded();
            terminalTab->ResizePane(direction);
        }
    }

    // Method Description:
    // - Move the viewport of the terminal of the currently focused tab up or
    //      down a page. The page length will be dependent on the terminal view height.
    // Arguments:
    // - scrollDirection: ScrollUp will move the viewport up, ScrollDown will move the viewport down
    void TerminalPage::_ScrollPage(ScrollDirection scrollDirection)
    {
        // Do nothing if for some reason, there's no terminal tab in focus. We don't want to crash.
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            if (const auto& control{ _GetActiveControl() })
            {
                const auto termHeight = control.ViewHeight();
                auto scrollDelta = _ComputeScrollDelta(scrollDirection, termHeight);
                terminalTab->Scroll(scrollDelta);
            }
        }
    }

    void TerminalPage::_ScrollToBufferEdge(ScrollDirection scrollDirection)
    {
        if (const auto terminalTab{ _GetFocusedTabImpl() })
        {
            auto scrollDelta = _ComputeScrollDelta(scrollDirection, INT_MAX);
            terminalTab->Scroll(scrollDelta);
        }
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Terminal"
    hstring TerminalPage::Title()
    {
        if (_settings.GlobalSettings().ShowTitleInTitlebar())
        {
            auto selectedIndex = _tabView.SelectedIndex();
            if (selectedIndex >= 0)
            {
                try
                {
                    if (auto focusedControl{ _GetActiveControl() })
                    {
                        return focusedControl.Title();
                    }
                }
                CATCH_LOG();
            }
        }
        return { L"Terminal" };
    }

    // Method Description:
    // - Handles the special case of providing a text override for the UI shortcut due to VK_OEM issue.
    //      Looks at the flags from the KeyChord modifiers and provides a concatenated string value of all
    //      in the same order that XAML would put them as well.
    // Return Value:
    // - a string representation of the key modifiers for the shortcut
    //NOTE: This needs to be localized with https://github.com/microsoft/terminal/issues/794 if XAML framework issue not resolved before then
    static std::wstring _FormatOverrideShortcutText(VirtualKeyModifiers modifiers)
    {
        std::wstring buffer{ L"" };

        if (WI_IsFlagSet(modifiers, VirtualKeyModifiers::Control))
        {
            buffer += L"Ctrl+";
        }

        if (WI_IsFlagSet(modifiers, VirtualKeyModifiers::Shift))
        {
            buffer += L"Shift+";
        }

        if (WI_IsFlagSet(modifiers, VirtualKeyModifiers::Menu))
        {
            buffer += L"Alt+";
        }

        if (WI_IsFlagSet(modifiers, VirtualKeyModifiers::Windows))
        {
            buffer += L"Win+";
        }

        return buffer;
    }

    // Method Description:
    // - Takes a MenuFlyoutItem and a corresponding KeyChord value and creates the accelerator for UI display.
    //   Takes into account a special case for an error condition for a comma
    // Arguments:
    // - MenuFlyoutItem that will be displayed, and a KeyChord to map an accelerator
    void TerminalPage::_SetAcceleratorForMenuItem(WUX::Controls::MenuFlyoutItem& menuItem,
                                                  const KeyChord& keyChord)
    {
#ifdef DEP_MICROSOFT_UI_XAML_708_FIXED
        // work around https://github.com/microsoft/microsoft-ui-xaml/issues/708 in case of VK_OEM_COMMA
        if (keyChord.Vkey() != VK_OEM_COMMA)
        {
            // use the XAML shortcut to give us the automatic capabilities
            auto menuShortcut = Windows::UI::Xaml::Input::KeyboardAccelerator{};

            // TODO: Modify this when https://github.com/microsoft/terminal/issues/877 is resolved
            menuShortcut.Key(static_cast<Windows::System::VirtualKey>(keyChord.Vkey()));

            // add the modifiers to the shortcut
            menuShortcut.Modifiers(keyChord.Modifiers());

            // add to the menu
            menuItem.KeyboardAccelerators().Append(menuShortcut);
        }
        else // we've got a comma, so need to just use the alternate method
#endif
        {
            // extract the modifier and key to a nice format
            auto overrideString = _FormatOverrideShortcutText(keyChord.Modifiers());
            auto mappedCh = MapVirtualKeyW(keyChord.Vkey(), MAPVK_VK_TO_CHAR);
            if (mappedCh != 0)
            {
                menuItem.KeyboardAcceleratorTextOverride(overrideString + gsl::narrow_cast<wchar_t>(mappedCh));
            }
        }
    }

    // Method Description:
    // - Calculates the appropriate size to snap to in the given direction, for
    //   the given dimension. If the global setting `snapToGridOnResize` is set
    //   to `false`, this will just immediately return the provided dimension,
    //   effectively disabling snapping.
    // - See Pane::CalcSnappedDimension
    float TerminalPage::CalcSnappedDimension(const bool widthOrHeight, const float dimension) const
    {
        if (_settings && _settings.GlobalSettings().SnapToGridOnResize())
        {
            if (const auto terminalTab{ _GetFocusedTabImpl() })
            {
                return terminalTab->CalcSnappedDimension(widthOrHeight, dimension);
            }
        }
        return dimension;
    }

    // Method Description:
    // - Place `copiedData` into the clipboard as text. Triggered when a
    //   terminal control raises it's CopyToClipboard event.
    // Arguments:
    // - copiedData: the new string content to place on the clipboard.
    winrt::fire_and_forget TerminalPage::_CopyToClipboardHandler(const IInspectable /*sender*/,
                                                                 const CopyToClipboardEventArgs copiedData)
    {
        co_await wil::resume_foreground(Dispatcher(), CoreDispatcherPriority::High);

        auto dataPack = DataPackage();
        dataPack.RequestedOperation(DataPackageOperation::Copy);

        // The EventArgs.Formats() is an override for the global setting "copyFormatting"
        //   iff it is set
        auto useGlobal = copiedData.Formats() == nullptr;
        auto copyFormats = useGlobal ?
                               _settings.GlobalSettings().CopyFormatting() :
                               copiedData.Formats().Value();

        // copy text to dataPack
        dataPack.SetText(copiedData.Text());

        if (WI_IsFlagSet(copyFormats, CopyFormat::HTML))
        {
            // copy html to dataPack
            const auto htmlData = copiedData.Html();
            if (!htmlData.empty())
            {
                dataPack.SetHtmlFormat(htmlData);
            }
        }

        if (WI_IsFlagSet(copyFormats, CopyFormat::RTF))
        {
            // copy rtf data to dataPack
            const auto rtfData = copiedData.Rtf();
            if (!rtfData.empty())
            {
                dataPack.SetRtf(rtfData);
            }
        }

        try
        {
            Clipboard::SetContent(dataPack);
            Clipboard::Flush();
        }
        CATCH_LOG();
    }

    // Function Description:
    // - This function is called when the `TermControl` requests that we send
    //   it the clipboard's content.
    // - Retrieves the data from the Windows Clipboard and converts it to text.
    // - Shows warnings if the clipboard is too big or contains multiple lines
    //   of text.
    // - Sends the text back to the TermControl through the event's
    //   `HandleClipboardData` member function.
    // - Does some of this in a background thread, as to not hang/crash the UI thread.
    // Arguments:
    // - eventArgs: the PasteFromClipboard event sent from the TermControl
    fire_and_forget TerminalPage::_PasteFromClipboardHandler(const IInspectable /*sender*/,
                                                             const PasteFromClipboardEventArgs eventArgs)
    {
        const auto data = Clipboard::GetContent();

        // This will switch the execution of the function to a background (not
        // UI) thread. This is IMPORTANT, because the getting the clipboard data
        // will crash on the UI thread, because the main thread is a STA.
        co_await winrt::resume_background();

        try
        {
            hstring text = L"";
            if (data.Contains(StandardDataFormats::Text()))
            {
                text = co_await data.GetTextAsync();
            }
            // Windows Explorer's "Copy address" menu item stores a StorageItem in the clipboard, and no text.
            else if (data.Contains(StandardDataFormats::StorageItems()))
            {
                auto items = co_await data.GetStorageItemsAsync();
                if (items.Size() > 0)
                {
                    auto item = items.GetAt(0);
                    text = item.Path();
                }
            }

            if (_settings.GlobalSettings().TrimPaste())
            {
                std::wstring_view textView{ text };
                const auto pos = textView.find_last_not_of(L"\t\n\v\f\r ");
                if (pos == textView.npos)
                {
                    // Text is all white space, nothing to paste
                    co_return;
                }
                else if (const auto toRemove = textView.size() - 1 - pos; toRemove > 0)
                {
                    textView.remove_suffix(toRemove);
                    text = { textView };
                }
            }

            // If the requesting terminal is in bracketed paste mode, then we don't need to warn about a multi-line paste.
            auto warnMultiLine = _settings.GlobalSettings().WarnAboutMultiLinePaste() &&
                                 !eventArgs.BracketedPasteEnabled();
            if (warnMultiLine)
            {
                const auto isNewLineLambda = [](auto c) { return c == L'\n' || c == L'\r'; };
                const auto hasNewLine = std::find_if(text.cbegin(), text.cend(), isNewLineLambda) != text.cend();
                warnMultiLine = hasNewLine;
            }

            constexpr const std::size_t minimumSizeForWarning = 1024 * 5; // 5 KiB
            const auto warnLargeText = text.size() > minimumSizeForWarning &&
                                       _settings.GlobalSettings().WarnAboutLargePaste();

            if (warnMultiLine || warnLargeText)
            {
                co_await wil::resume_foreground(Dispatcher());

                // We have to initialize the dialog here to be able to change the text of the text block within it
                FindName(L"MultiLinePasteDialog").try_as<WUX::Controls::ContentDialog>();
                ClipboardText().Text(text);

                // The vertical offset on the scrollbar does not reset automatically, so reset it manually
                ClipboardContentScrollViewer().ScrollToVerticalOffset(0);

                auto warningResult = ContentDialogResult::Primary;
                if (warnMultiLine)
                {
                    warningResult = co_await _ShowMultiLinePasteWarningDialog();
                }
                else if (warnLargeText)
                {
                    warningResult = co_await _ShowLargePasteWarningDialog();
                }

                // Clear the clipboard text so it doesn't lie around in memory
                ClipboardText().Text(L"");

                if (warningResult != ContentDialogResult::Primary)
                {
                    // user rejected the paste
                    co_return;
                }
            }

            eventArgs.HandleClipboardData(text);
        }
        CATCH_LOG();
    }

    void TerminalPage::_OpenHyperlinkHandler(const IInspectable /*sender*/, const Microsoft::Terminal::Control::OpenHyperlinkEventArgs eventArgs)
    {
        try
        {
            auto parsed = winrt::Windows::Foundation::Uri(eventArgs.Uri().c_str());
            if (_IsUriSupported(parsed))
            {
                ShellExecute(nullptr, L"open", eventArgs.Uri().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            }
            else
            {
                _ShowCouldNotOpenDialog(RS_(L"UnsupportedSchemeText"), eventArgs.Uri());
            }
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            _ShowCouldNotOpenDialog(RS_(L"InvalidUriText"), eventArgs.Uri());
        }
    }

    // Method Description:
    // - Opens up a dialog box explaining why we could not open a URI
    // Arguments:
    // - The reason (unsupported scheme, invalid uri, potentially more in the future)
    // - The uri
    void TerminalPage::_ShowCouldNotOpenDialog(winrt::hstring reason, winrt::hstring uri)
    {
        if (auto presenter{ _dialogPresenter.get() })
        {
            // FindName needs to be called first to actually load the xaml object
            auto unopenedUriDialog = FindName(L"CouldNotOpenUriDialog").try_as<WUX::Controls::ContentDialog>();

            // Insert the reason and the URI
            CouldNotOpenUriReason().Text(reason);
            UnopenedUri().Text(uri);

            // Show the dialog
            presenter.ShowDialog(unopenedUriDialog);
        }
    }

    // Method Description:
    // - Determines if the given URI is currently supported
    // Arguments:
    // - The parsed URI
    // Return value:
    // - True if we support it, false otherwise
    bool TerminalPage::_IsUriSupported(const winrt::Windows::Foundation::Uri& parsedUri)
    {
        if (parsedUri.SchemeName() == L"http" || parsedUri.SchemeName() == L"https")
        {
            return true;
        }
        if (parsedUri.SchemeName() == L"file")
        {
            const auto host = parsedUri.Host();
            // If no hostname was provided or if the hostname was "localhost", Host() will return an empty string
            // and we allow it
            if (host == L"")
            {
                return true;
            }
            // TODO: by the OSC 8 spec, if a hostname (other than localhost) is provided, we _should_ be
            // comparing that value against what is returned by GetComputerNameExW and making sure they match.
            // However, ShellExecute does not seem to be happy with file URIs of the form
            //          file://{hostname}/path/to/file.ext
            // and so while we could do the hostname matching, we do not know how to actually open the URI
            // if its given in that form. So for now we ignore all hostnames other than localhost
        }
        return false;
    }

    // Important! Don't take this eventArgs by reference, we need to extend the
    // lifetime of it to the other side of the co_await!
    winrt::fire_and_forget TerminalPage::_ControlNoticeRaisedHandler(const IInspectable /*sender*/,
                                                                     const Microsoft::Terminal::Control::NoticeEventArgs eventArgs)
    {
        auto weakThis = get_weak();
        co_await wil::resume_foreground(Dispatcher());
        if (auto page = weakThis.get())
        {
            auto message = eventArgs.Message();

            winrt::hstring title;

            switch (eventArgs.Level())
            {
            case NoticeLevel::Debug:
                title = RS_(L"NoticeDebug"); //\xebe8
                break;
            case NoticeLevel::Info:
                title = RS_(L"NoticeInfo"); // \xe946
                break;
            case NoticeLevel::Warning:
                title = RS_(L"NoticeWarning"); //\xe7ba
                break;
            case NoticeLevel::Error:
                title = RS_(L"NoticeError"); //\xe783
                break;
            }

            page->_ShowControlNoticeDialog(title, message);
        }
    }

    void TerminalPage::_ShowControlNoticeDialog(const winrt::hstring& title, const winrt::hstring& message)
    {
        if (auto presenter{ _dialogPresenter.get() })
        {
            // FindName needs to be called first to actually load the xaml object
            auto controlNoticeDialog = FindName(L"ControlNoticeDialog").try_as<WUX::Controls::ContentDialog>();

            ControlNoticeDialog().Title(winrt::box_value(title));

            // Insert the message
            NoticeMessage().Text(message);

            // Show the dialog
            presenter.ShowDialog(controlNoticeDialog);
        }
    }

    // Method Description:
    // - Copy text from the focused terminal to the Windows Clipboard
    // Arguments:
    // - singleLine: if enabled, copy contents as a single line of text
    // - formats: dictate which formats need to be copied
    // Return Value:
    // - true iff we we able to copy text (if a selection was active)
    bool TerminalPage::_CopyText(const bool singleLine, const Windows::Foundation::IReference<CopyFormat>& formats)
    {
        if (const auto& control{ _GetActiveControl() })
        {
            return control.CopySelectionToClipboard(singleLine, formats);
        }
        return false;
    }

    // Method Description:
    // - Send an event (which will be caught by AppHost) to set the progress indicator on the taskbar
    // Arguments:
    // - sender (not used)
    // - eventArgs: the arguments specifying how to set the progress indicator
    winrt::fire_and_forget TerminalPage::_SetTaskbarProgressHandler(const IInspectable /*sender*/, const IInspectable /*eventArgs*/)
    {
        co_await wil::resume_foreground(Dispatcher());
        _SetTaskbarProgressHandlers(*this, nullptr);
    }

    // Method Description:
    // - Send an event (which will be caught by AppHost) to change the show window state of the entire hosting window
    // Arguments:
    // - sender (not used)
    // - args: the arguments specifying how to set the display status to ShowWindow for our window handle
    winrt::fire_and_forget TerminalPage::_ShowWindowChangedHandler(const IInspectable /*sender*/, const Microsoft::Terminal::Control::ShowWindowArgs args)
    {
        co_await resume_foreground(Dispatcher());
        _ShowWindowChangedHandlers(*this, args);
    }

    // Method Description:
    // - Paste text from the Windows Clipboard to the focused terminal
    void TerminalPage::_PasteText()
    {
        if (const auto& control{ _GetActiveControl() })
        {
            control.PasteTextFromClipboard();
        }
    }

    // Function Description:
    // - Called when the settings button is clicked. ShellExecutes the settings
    //   file, as to open it in the default editor for .json files. Does this in
    //   a background thread, as to not hang/crash the UI thread.
    fire_and_forget TerminalPage::_LaunchSettings(const SettingsTarget target)
    {
        if (target == SettingsTarget::SettingsUI)
        {
            OpenSettingsUI();
        }
        else
        {
            // This will switch the execution of the function to a background (not
            // UI) thread. This is IMPORTANT, because the Windows.Storage API's
            // (used for retrieving the path to the file) will crash on the UI
            // thread, because the main thread is a STA.
            co_await winrt::resume_background();

            auto openFile = [](const auto& filePath) {
                HINSTANCE res = ShellExecute(nullptr, nullptr, filePath.c_str(), nullptr, nullptr, SW_SHOW);
                if (static_cast<int>(reinterpret_cast<uintptr_t>(res)) <= 32)
                {
                    ShellExecute(nullptr, nullptr, L"notepad", filePath.c_str(), nullptr, SW_SHOW);
                }
            };

            switch (target)
            {
            case SettingsTarget::DefaultsFile:
                openFile(CascadiaSettings::DefaultSettingsPath());
                break;
            case SettingsTarget::SettingsFile:
                openFile(CascadiaSettings::SettingsPath());
                break;
            case SettingsTarget::AllFiles:
                openFile(CascadiaSettings::DefaultSettingsPath());
                openFile(CascadiaSettings::SettingsPath());
                break;
            }
        }
    }

    // Method Description:
    // - Responds to the TabView control's Tab Closing event by removing
    //      the indicated tab from the set and focusing another one.
    //      The event is cancelled so App maintains control over the
    //      items in the tabview.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void TerminalPage::_OnTabCloseRequested(const IInspectable& /*sender*/, const MUX::Controls::TabViewTabCloseRequestedEventArgs& eventArgs)
    {
        const auto tabViewItem = eventArgs.Tab();
        if (auto tab{ _GetTabByTabViewItem(tabViewItem) })
        {
            _HandleCloseTabRequested(tab);
        }
    }

    TermControl TerminalPage::_InitControl(const TerminalSettingsCreateResult& settings, const ITerminalConnection& connection)
    {
        // Do any initialization that needs to apply to _every_ TermControl we
        // create here.
        // TermControl will copy the settings out of the settings passed to it.
        TermControl term{ settings.DefaultSettings(), settings.UnfocusedSettings(), connection };

        // GH#12515: ConPTY assumes it's hidden at the start. If we're not, let it know now.
        if (_visible)
        {
            term.WindowVisibilityChanged(_visible);
        }

        if (_hostingHwnd.has_value())
        {
            term.OwningHwnd(reinterpret_cast<uint64_t>(*_hostingHwnd));
        }
        return term;
    }

    // Method Description:
    // - Creates a pane and returns a shared_ptr to it
    // - The caller should handle where the pane goes after creation,
    //   either to split an already existing pane or to create a new tab with it
    // Arguments:
    // - newTerminalArgs: an object that may contain a blob of parameters to
    //   control which profile is created and with possible other
    //   configurations. See CascadiaSettings::BuildSettings for more details.
    // - duplicate: a boolean to indicate whether the pane we create should be
    //   a duplicate of the currently focused pane
    // - existingConnection: optionally receives a connection from the outside
    //   world instead of attempting to create one
    // Return Value:
    // - If the newTerminalArgs required us to open the pane as a new elevated
    //   connection, then we'll return nullptr. Otherwise, we'll return a new
    //   Pane for this connection.
    std::shared_ptr<Pane> TerminalPage::_MakePane(const NewTerminalArgs& newTerminalArgs,
                                                  const bool duplicate,
                                                  TerminalConnection::ITerminalConnection existingConnection)
    {
        TerminalSettingsCreateResult controlSettings{ nullptr };
        Profile profile{ nullptr };

        if (duplicate)
        {
            const auto focusedTab{ _GetFocusedTabImpl() };
            if (focusedTab)
            {
                profile = focusedTab->GetFocusedProfile();
                if (profile)
                {
                    // TODO GH#5047 If we cache the NewTerminalArgs, we no longer need to do this.
                    profile = GetClosestProfileForDuplicationOfProfile(profile);
                    controlSettings = TerminalSettings::CreateWithProfile(_settings, profile, *_bindings);
                    const auto workingDirectory = focusedTab->GetActiveTerminalControl().WorkingDirectory();
                    const auto validWorkingDirectory = !workingDirectory.empty();
                    if (validWorkingDirectory)
                    {
                        controlSettings.DefaultSettings().StartingDirectory(workingDirectory);
                    }
                }
            }
        }
        if (!profile)
        {
            profile = _settings.GetProfileForArgs(newTerminalArgs);
            controlSettings = TerminalSettings::CreateWithNewTerminalArgs(_settings, newTerminalArgs, *_bindings);
        }

        // Try to handle auto-elevation
        if (_maybeElevate(newTerminalArgs, controlSettings, profile))
        {
            return nullptr;
        }

        auto connection = existingConnection ? existingConnection : _CreateConnectionFromSettings(profile, controlSettings.DefaultSettings());
        if (existingConnection)
        {
            connection.Resize(controlSettings.DefaultSettings().InitialRows(), controlSettings.DefaultSettings().InitialCols());
        }

        TerminalConnection::ITerminalConnection debugConnection{ nullptr };
        if (_settings.GlobalSettings().DebugFeaturesEnabled())
        {
            const auto window = CoreWindow::GetForCurrentThread();
            const auto rAltState = window.GetKeyState(VirtualKey::RightMenu);
            const auto lAltState = window.GetKeyState(VirtualKey::LeftMenu);
            const auto bothAltsPressed = WI_IsFlagSet(lAltState, CoreVirtualKeyStates::Down) &&
                                         WI_IsFlagSet(rAltState, CoreVirtualKeyStates::Down);
            if (bothAltsPressed)
            {
                std::tie(connection, debugConnection) = OpenDebugTapConnection(connection);
            }
        }

        const auto control = _InitControl(controlSettings, connection);
        _RegisterTerminalEvents(control);

        auto resultPane = std::make_shared<Pane>(profile, control);

        if (debugConnection) // this will only be set if global debugging is on and tap is active
        {
            auto newControl = _InitControl(controlSettings, debugConnection);
            _RegisterTerminalEvents(newControl);
            // Split (auto) with the debug tap.
            auto debugPane = std::make_shared<Pane>(profile, newControl);

            // Since we're doing this split directly on the pane (instead of going through TerminalTab,
            // we need to handle the panes 'active' states

            // Set the pane we're splitting to active (otherwise Split will not do anything)
            resultPane->SetActive();
            auto [original, _] = resultPane->Split(SplitDirection::Automatic, 0.5f, debugPane);

            // Set the non-debug pane as active
            resultPane->ClearActive();
            original->SetActive();
        }

        return resultPane;
    }

    // Method Description:
    // - Sets background image and applies its settings (stretch, opacity and alignment)
    // - Checks path validity
    // Arguments:
    // - newAppearance
    // Return Value:
    // - <none>
    void TerminalPage::_SetBackgroundImage(const winrt::Microsoft::Terminal::Settings::Model::IAppearanceConfig& newAppearance)
    {
        const auto path = newAppearance.ExpandedBackgroundImagePath();
        if (path.empty())
        {
            _tabContent.Background(nullptr);
            return;
        }

        Windows::Foundation::Uri imageUri{ nullptr };
        try
        {
            imageUri = Windows::Foundation::Uri{ path };
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            _tabContent.Background(nullptr);
            return;
        }
        // Check if the image brush is already pointing to the image
        // in the modified settings; if it isn't (or isn't there),
        // set a new image source for the brush

        auto brush = _tabContent.Background().try_as<Media::ImageBrush>();
        Media::Imaging::BitmapImage imageSource = brush == nullptr ? nullptr : brush.ImageSource().try_as<Media::Imaging::BitmapImage>();

        if (imageSource == nullptr ||
            imageSource.UriSource() == nullptr ||
            !imageSource.UriSource().Equals(imageUri))
        {
            Media::ImageBrush b{};
            // Note that BitmapImage handles the image load asynchronously,
            // which is especially important since the image
            // may well be both large and somewhere out on the
            // internet.
            Media::Imaging::BitmapImage image(imageUri);
            b.ImageSource(image);
            _tabContent.Background(b);

            b.Stretch(newAppearance.BackgroundImageStretchMode());
            b.Opacity(newAppearance.BackgroundImageOpacity());
        }
    }

    // Method Description:
    // - Hook up keybindings, and refresh the UI of the terminal.
    //   This includes update the settings of all the tabs according
    //   to their profiles, update the title and icon of each tab, and
    //   finally create the tab flyout
    void TerminalPage::_RefreshUIForSettingsReload()
    {
        // Re-wire the keybindings to their handlers, as we'll have created a
        // new AppKeyBindings object.
        _HookupKeyBindings(_settings.ActionMap());

        // Refresh UI elements

        // Mapping by GUID isn't _excellent_ because the defaults profile doesn't have a stable GUID; however,
        // when we stabilize its guid this will become fully safe.
        std::unordered_map<winrt::guid, std::pair<Profile, TerminalSettingsCreateResult>> profileGuidSettingsMap;
        const auto profileDefaults{ _settings.ProfileDefaults() };
        const auto allProfiles{ _settings.AllProfiles() };

        profileGuidSettingsMap.reserve(allProfiles.Size() + 1);

        // Include the Defaults profile for consideration
        profileGuidSettingsMap.insert_or_assign(profileDefaults.Guid(), std::pair{ profileDefaults, nullptr });
        for (const auto& newProfile : allProfiles)
        {
            // Avoid creating a TerminalSettings right now. They're not totally cheap, and we suspect that users with many
            // panes may not be using all of their profiles at the same time. Lazy evaluation is king!
            profileGuidSettingsMap.insert_or_assign(newProfile.Guid(), std::pair{ newProfile, nullptr });
        }

        if (_settings.GlobalSettings().UseBackgroundImageForWindow())
        {
            const auto focusedTab{ _GetFocusedTabImpl() };
            if (focusedTab)
            {
                auto profile = focusedTab->GetFocusedProfile();
                if (profile)
                {
                    _SetBackgroundImage(profile.DefaultAppearance());
                }
            }
        }
        for (const auto& tab : _tabs)
        {
            if (auto terminalTab{ _GetTerminalTabImpl(tab) })
            {
                terminalTab->UpdateSettings();

                // Manually enumerate the panes in each tab; this will let us recycle TerminalSettings
                // objects but only have to iterate one time.
                terminalTab->GetRootPane()->WalkTree([&](auto&& pane) {
                    if (const auto profile{ pane->GetProfile() })
                    {
                        const auto found{ profileGuidSettingsMap.find(profile.Guid()) };
                        // GH#2455: If there are any panes with controls that had been
                        // initialized with a Profile that no longer exists in our list of
                        // profiles, we'll leave it unmodified. The profile doesn't exist
                        // anymore, so we can't possibly update its settings.
                        if (found != profileGuidSettingsMap.cend())
                        {
                            auto& pair{ found->second };
                            if (!pair.second)
                            {
                                pair.second = TerminalSettings::CreateWithProfile(_settings, pair.first, *_bindings);
                            }
                            pane->UpdateSettings(pair.second, pair.first);
                        }
                    }
                });

                // Update the icon of the tab for the currently focused profile in that tab.
                // Only do this for TerminalTabs. Other types of tabs won't have multiple panes
                // and profiles so the Title and Icon will be set once and only once on init.
                _UpdateTabIcon(*terminalTab);

                // Force the TerminalTab to re-grab its currently active control's title.
                terminalTab->UpdateTitle();
            }
            else if (auto settingsTab = tab.try_as<TerminalApp::SettingsTab>())
            {
                settingsTab.UpdateSettings(_settings);
            }

            auto tabImpl{ winrt::get_self<TabBase>(tab) };
            tabImpl->SetActionMap(_settings.ActionMap());
        }

        // repopulate the new tab button's flyout with entries for each
        // profile, which might have changed
        _UpdateTabWidthMode();
        _CreateNewTabFlyout();

        // Reload the current value of alwaysOnTop from the settings file. This
        // will let the user hot-reload this setting, but any runtime changes to
        // the alwaysOnTop setting will be lost.
        _isAlwaysOnTop = _settings.GlobalSettings().AlwaysOnTop();
        _AlwaysOnTopChangedHandlers(*this, nullptr);

        // Settings AllowDependentAnimations will affect whether animations are
        // enabled application-wide, so we don't need to check it each time we
        // want to create an animation.
        WUX::Media::Animation::Timeline::AllowDependentAnimations(!_settings.GlobalSettings().DisableAnimations());

        _tabRow.ShowElevationShield(IsElevated() && _settings.GlobalSettings().ShowAdminShield());

        Media::SolidColorBrush transparent{ Windows::UI::Colors::Transparent() };
        _tabView.Background(transparent);

        ////////////////////////////////////////////////////////////////////////
        // Begin Theme handling
        _updateThemeColors();
    }

    // This is a helper to aid in sorting commands by their `Name`s, alphabetically.
    static bool _compareSchemeNames(const ColorScheme& lhs, const ColorScheme& rhs)
    {
        std::wstring leftName{ lhs.Name() };
        std::wstring rightName{ rhs.Name() };
        return leftName.compare(rightName) < 0;
    }

    // Method Description:
    // - Takes a mapping of names->commands and expands them
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    IMap<winrt::hstring, Command> TerminalPage::_ExpandCommands(IMapView<winrt::hstring, Command> commandsToExpand,
                                                                IVectorView<Profile> profiles,
                                                                IMapView<winrt::hstring, ColorScheme> schemes)
    {
        auto warnings{ winrt::single_threaded_vector<SettingsLoadWarnings>() };

        std::vector<ColorScheme> sortedSchemes;
        sortedSchemes.reserve(schemes.Size());

        for (const auto& nameAndScheme : schemes)
        {
            sortedSchemes.push_back(nameAndScheme.Value());
        }
        std::sort(sortedSchemes.begin(),
                  sortedSchemes.end(),
                  _compareSchemeNames);

        auto copyOfCommands = winrt::single_threaded_map<winrt::hstring, Command>();
        for (const auto& nameAndCommand : commandsToExpand)
        {
            copyOfCommands.Insert(nameAndCommand.Key(), nameAndCommand.Value());
        }

        Command::ExpandCommands(copyOfCommands,
                                profiles,
                                { sortedSchemes },
                                warnings);

        return copyOfCommands;
    }
    // Method Description:
    // - Repopulates the list of commands in the command palette with the
    //   current commands in the settings. Also updates the keybinding labels to
    //   reflect any matching keybindings.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_UpdateCommandsForPalette()
    {
        auto copyOfCommands = _ExpandCommands(_settings.GlobalSettings().ActionMap().NameMap(),
                                              _settings.ActiveProfiles().GetView(),
                                              _settings.GlobalSettings().ColorSchemes());

        _recursiveUpdateCommandKeybindingLabels(_settings, copyOfCommands.GetView());

        // Update the command palette when settings reload
        auto commandsCollection = winrt::single_threaded_vector<Command>();
        for (const auto& nameAndCommand : copyOfCommands)
        {
            commandsCollection.Append(nameAndCommand.Value());
        }

        CommandPalette().SetCommands(commandsCollection);
    }

    // Method Description:
    // - Sets the initial actions to process on startup. We'll make a copy of
    //   this list, and process these actions when we're loaded.
    // - This function will have no effective result after Create() is called.
    // Arguments:
    // - actions: a list of Actions to process on startup.
    // Return Value:
    // - <none>
    void TerminalPage::SetStartupActions(std::vector<ActionAndArgs>& actions)
    {
        // The fastest way to copy all the actions out of the std::vector and
        // put them into a winrt::IVector is by making a copy, then moving the
        // copy into the winrt vector ctor.
        auto listCopy = actions;
        _startupActions = winrt::single_threaded_vector<ActionAndArgs>(std::move(listCopy));
    }

    // Routine Description:
    // - Notifies this Terminal Page that it should start the incoming connection
    //   listener for command-line tools attempting to join this Terminal
    //   through the default application channel.
    // Arguments:
    // - isEmbedding - True if COM started us to be a server. False if we're doing it of our own accord.
    // Return Value:
    // - <none>
    void TerminalPage::SetInboundListener(bool isEmbedding)
    {
        _shouldStartInboundListener = true;
        _isEmbeddingInboundListener = isEmbedding;

        // If the page has already passed the NotInitialized state,
        // then it is ready-enough for us to just start this immediately.
        if (_startupState != StartupState::NotInitialized)
        {
            _StartInboundListener();
        }
    }

    winrt::TerminalApp::IDialogPresenter TerminalPage::DialogPresenter() const
    {
        return _dialogPresenter.get();
    }

    void TerminalPage::DialogPresenter(winrt::TerminalApp::IDialogPresenter dialogPresenter)
    {
        _dialogPresenter = dialogPresenter;
    }

    // Method Description:
    // - Get the combined taskbar state for the page. This is the combination of
    //   all the states of all the tabs, which are themselves a combination of
    //   all their panes. Taskbar states are given a priority based on the rules
    //   in:
    //   https://docs.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-itaskbarlist3-setprogressstate
    //   under "How the Taskbar Button Chooses the Progress Indicator for a Group"
    // Arguments:
    // - <none>
    // Return Value:
    // - A TaskbarState object representing the combined taskbar state and
    //   progress percentage of all our tabs.
    winrt::TerminalApp::TaskbarState TerminalPage::TaskbarState() const
    {
        auto state{ winrt::make<winrt::TerminalApp::implementation::TaskbarState>() };

        for (const auto& tab : _tabs)
        {
            if (auto tabImpl{ _GetTerminalTabImpl(tab) })
            {
                auto tabState{ tabImpl->GetCombinedTaskbarState() };
                // lowest priority wins
                if (tabState.Priority() < state.Priority())
                {
                    state = tabState;
                }
            }
        }

        return state;
    }

    // Method Description:
    // - This is the method that App will call when the titlebar
    //   has been clicked. It dismisses any open flyouts.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::TitlebarClicked()
    {
        if (_newTabButton && _newTabButton.Flyout())
        {
            _newTabButton.Flyout().Hide();
        }
        _DismissTabContextMenus();
    }

    // Method Description:
    // - Notifies all attached console controls that the visibility of the
    //   hosting window has changed. The underlying PTYs may need to know this
    //   for the proper response to `::GetConsoleWindow()` from a Win32 console app.
    // Arguments:
    // - showOrHide: Show is true; hide is false.
    // Return Value:
    // - <none>
    void TerminalPage::WindowVisibilityChanged(const bool showOrHide)
    {
        _visible = showOrHide;
        for (const auto& tab : _tabs)
        {
            if (auto terminalTab{ _GetTerminalTabImpl(tab) })
            {
                // Manually enumerate the panes in each tab; this will let us recycle TerminalSettings
                // objects but only have to iterate one time.
                terminalTab->GetRootPane()->WalkTree([&](auto&& pane) {
                    if (auto control = pane->GetTerminalControl())
                    {
                        control.WindowVisibilityChanged(showOrHide);
                    }
                });
            }
        }
    }

    // Method Description:
    // - Called when the user tries to do a search using keybindings.
    //   This will tell the current focused terminal control to create
    //   a search box and enable find process.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_Find()
    {
        if (const auto& control{ _GetActiveControl() })
        {
            control.CreateSearchBoxControl();
        }
    }

    // Method Description:
    // - Toggles borderless mode. Hides the tab row, and raises our
    //   FocusModeChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::ToggleFocusMode()
    {
        SetFocusMode(!_isInFocusMode);
    }

    void TerminalPage::SetFocusMode(const bool inFocusMode)
    {
        const auto newInFocusMode = inFocusMode;
        if (newInFocusMode != FocusMode())
        {
            _isInFocusMode = newInFocusMode;
            _UpdateTabView();
            _FocusModeChangedHandlers(*this, nullptr);
        }
    }

    // Method Description:
    // - Toggles fullscreen mode. Hides the tab row, and raises our
    //   FullscreenChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::ToggleFullscreen()
    {
        SetFullscreen(!_isFullscreen);
    }

    // Method Description:
    // - Toggles always on top mode. Raises our AlwaysOnTopChanged event.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::ToggleAlwaysOnTop()
    {
        _isAlwaysOnTop = !_isAlwaysOnTop;
        _AlwaysOnTopChangedHandlers(*this, nullptr);
    }

    // Method Description:
    // - Sets the tab split button color when a new tab color is selected
    // Arguments:
    // - color: The color of the newly selected tab, used to properly calculate
    //          the foreground color of the split button (to match the font
    //          color of the tab)
    // - accentColor: the actual color we are going to use to paint the tab row and
    //                split button, so that there is some contrast between the tab
    //                and the non-client are behind it
    // Return Value:
    // - <none>
    void TerminalPage::_SetNewTabButtonColor(const Windows::UI::Color& color, const Windows::UI::Color& accentColor)
    {
        // TODO GH#3327: Look at what to do with the tab button when we have XAML theming
        auto IsBrightColor = ColorHelper::IsBrightColor(color);
        auto isLightAccentColor = ColorHelper::IsBrightColor(accentColor);
        winrt::Windows::UI::Color pressedColor{};
        winrt::Windows::UI::Color hoverColor{};
        winrt::Windows::UI::Color foregroundColor{};
        const auto hoverColorAdjustment = 5.f;
        const auto pressedColorAdjustment = 7.f;

        if (IsBrightColor)
        {
            foregroundColor = winrt::Windows::UI::Colors::Black();
        }
        else
        {
            foregroundColor = winrt::Windows::UI::Colors::White();
        }

        if (isLightAccentColor)
        {
            hoverColor = ColorHelper::Darken(accentColor, hoverColorAdjustment);
            pressedColor = ColorHelper::Darken(accentColor, pressedColorAdjustment);
        }
        else
        {
            hoverColor = ColorHelper::Lighten(accentColor, hoverColorAdjustment);
            pressedColor = ColorHelper::Lighten(accentColor, pressedColorAdjustment);
        }

        Media::SolidColorBrush backgroundBrush{ accentColor };
        Media::SolidColorBrush backgroundHoverBrush{ hoverColor };
        Media::SolidColorBrush backgroundPressedBrush{ pressedColor };
        Media::SolidColorBrush foregroundBrush{ foregroundColor };

        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonBackground"), backgroundBrush);
        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonBackgroundPointerOver"), backgroundHoverBrush);
        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonBackgroundPressed"), backgroundPressedBrush);

        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonForeground"), foregroundBrush);
        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonForegroundPointerOver"), foregroundBrush);
        _newTabButton.Resources().Insert(winrt::box_value(L"SplitButtonForegroundPressed"), foregroundBrush);

        _newTabButton.Background(backgroundBrush);
        _newTabButton.Foreground(foregroundBrush);
    }

    // Method Description:
    // - Clears the tab split button color to a system color
    //   (or white if none is found) when the tab's color is cleared
    // - Clears the tab row color to a system color
    //   (or white if none is found) when the tab's color is cleared
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_ClearNewTabButtonColor()
    {
        // TODO GH#3327: Look at what to do with the tab button when we have XAML theming
        winrt::hstring keys[] = {
            L"SplitButtonBackground",
            L"SplitButtonBackgroundPointerOver",
            L"SplitButtonBackgroundPressed",
            L"SplitButtonForeground",
            L"SplitButtonForegroundPointerOver",
            L"SplitButtonForegroundPressed"
        };

        // simply clear any of the colors in the split button's dict
        for (auto keyString : keys)
        {
            auto key = winrt::box_value(keyString);
            if (_newTabButton.Resources().HasKey(key))
            {
                _newTabButton.Resources().Remove(key);
            }
        }

        const auto res = Application::Current().Resources();

        const auto defaultBackgroundKey = winrt::box_value(L"TabViewItemHeaderBackground");
        const auto defaultForegroundKey = winrt::box_value(L"SystemControlForegroundBaseHighBrush");
        winrt::Windows::UI::Xaml::Media::SolidColorBrush backgroundBrush;
        winrt::Windows::UI::Xaml::Media::SolidColorBrush foregroundBrush;

        // TODO: Related to GH#3917 - I think if the system is set to "Dark"
        // theme, but the app is set to light theme, then this lookup still
        // returns to us the dark theme brushes. There's gotta be a way to get
        // the right brushes...
        // See also GH#5741
        if (res.HasKey(defaultBackgroundKey))
        {
            auto obj = res.Lookup(defaultBackgroundKey);
            backgroundBrush = obj.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>();
        }
        else
        {
            backgroundBrush = winrt::Windows::UI::Xaml::Media::SolidColorBrush{ winrt::Windows::UI::Colors::Black() };
        }

        if (res.HasKey(defaultForegroundKey))
        {
            auto obj = res.Lookup(defaultForegroundKey);
            foregroundBrush = obj.try_as<winrt::Windows::UI::Xaml::Media::SolidColorBrush>();
        }
        else
        {
            foregroundBrush = winrt::Windows::UI::Xaml::Media::SolidColorBrush{ winrt::Windows::UI::Colors::White() };
        }

        _newTabButton.Background(backgroundBrush);
        _newTabButton.Foreground(foregroundBrush);
    }

    // Function Description:
    // - This is a helper method to get the commandline out of a
    //   ExecuteCommandline action, break it into subcommands, and attempt to
    //   parse it into actions. This is used by _HandleExecuteCommandline for
    //   processing commandlines in the current WT window.
    // Arguments:
    // - args: the ExecuteCommandlineArgs to synthesize a list of startup actions for.
    // Return Value:
    // - an empty list if we failed to parse, otherwise a list of actions to execute.
    std::vector<ActionAndArgs> TerminalPage::ConvertExecuteCommandlineToActions(const ExecuteCommandlineArgs& args)
    {
        ::TerminalApp::AppCommandlineArgs appArgs;
        if (appArgs.ParseArgs(args) == 0)
        {
            return appArgs.GetStartupActions();
        }

        return {};
    }

    void TerminalPage::_FocusActiveControl(IInspectable /*sender*/,
                                           IInspectable /*eventArgs*/)
    {
        _FocusCurrentTab(false);
    }

    bool TerminalPage::FocusMode() const
    {
        return _isInFocusMode;
    }

    bool TerminalPage::Fullscreen() const
    {
        return _isFullscreen;
    }

    // Method Description:
    // - Returns true if we're currently in "Always on top" mode. When we're in
    //   always on top mode, the window should be on top of all other windows.
    //   If multiple windows are all "always on top", they'll maintain their own
    //   z-order, with all the windows on top of all other non-topmost windows.
    // Arguments:
    // - <none>
    // Return Value:
    // - true if we should be in "always on top" mode
    bool TerminalPage::AlwaysOnTop() const
    {
        return _isAlwaysOnTop;
    }

    void TerminalPage::SetFullscreen(bool newFullscreen)
    {
        if (_isFullscreen == newFullscreen)
        {
            return;
        }
        _isFullscreen = newFullscreen;
        _UpdateTabView();
        _FullscreenChangedHandlers(*this, nullptr);
    }

    // Method Description:
    // - Updates the page's state for isMaximized when the window changes externally.
    void TerminalPage::Maximized(bool newMaximized)
    {
        _isMaximized = newMaximized;
    }

    // Method Description:
    // - Asks the window to change its maximized state.
    void TerminalPage::RequestSetMaximized(bool newMaximized)
    {
        if (_isMaximized == newMaximized)
        {
            return;
        }
        _isMaximized = newMaximized;
        _ChangeMaximizeRequestedHandlers(*this, nullptr);
    }

    HRESULT TerminalPage::_OnNewConnection(const ConptyConnection& connection)
    {
        // We need to be on the UI thread in order for _OpenNewTab to run successfully.
        // HasThreadAccess will return true if we're currently on a UI thread and false otherwise.
        // When we're on a COM thread, we'll need to dispatch the calls to the UI thread
        // and wait on it hence the locking mechanism.
        if (!Dispatcher().HasThreadAccess())
        {
            til::latch latch{ 1 };
            auto finalVal = S_OK;

            Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [&]() {
                finalVal = _OnNewConnection(connection);
                latch.count_down();
            });

            latch.wait();
            return finalVal;
        }

        try
        {
            NewTerminalArgs newTerminalArgs;
            newTerminalArgs.Commandline(connection.Commandline());
            // GH #12370: We absolutely cannot allow a defterm connection to
            // auto-elevate. Defterm doesn't work for elevated scenarios in the
            // first place. If we try accepting the connection, the spawning an
            // elevated version of the Terminal with that profile... that's a
            // recipe for disaster. We won't ever open up a tab in this window.
            newTerminalArgs.Elevate(false);
            _CreateNewTabFromPane(_MakePane(newTerminalArgs, false, connection));

            // Request a summon of this window to the foreground
            _SummonWindowRequestedHandlers(*this, nullptr);

            const IInspectable unused{ nullptr };
            _SetAsDefaultDismissHandler(unused, unused);
            return S_OK;
        }
        CATCH_RETURN()
    }

    // Method Description:
    // - Creates a settings UI tab and focuses it. If there's already a settings UI tab open,
    //   just focus the existing one.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::OpenSettingsUI()
    {
        // If we're holding the settings tab's switch command, don't create a new one, switch to the existing one.
        if (!_settingsTab)
        {
            winrt::Microsoft::Terminal::Settings::Editor::MainPage sui{ _settings };
            if (_hostingHwnd)
            {
                sui.SetHostingWindow(reinterpret_cast<uint64_t>(*_hostingHwnd));
            }

            // GH#8767 - let unhandled keys in the SUI try to run commands too.
            sui.KeyDown({ this, &TerminalPage::_KeyDownHandler });

            sui.OpenJson([weakThis{ get_weak() }](auto&& /*s*/, winrt::Microsoft::Terminal::Settings::Model::SettingsTarget e) {
                if (auto page{ weakThis.get() })
                {
                    page->_LaunchSettings(e);
                }
            });

            auto newTabImpl = winrt::make_self<SettingsTab>(sui);

            // Add the new tab to the list of our tabs.
            _tabs.Append(*newTabImpl);
            _mruTabs.Append(*newTabImpl);

            newTabImpl->SetDispatch(*_actionDispatch);
            newTabImpl->SetActionMap(_settings.ActionMap());

            // Give the tab its index in the _tabs vector so it can manage its own SwitchToTab command.
            _UpdateTabIndices();

            // Don't capture a strong ref to the tab. If the tab is removed as this
            // is called, we don't really care anymore about handling the event.
            auto weakTab = make_weak(newTabImpl);

            auto tabViewItem = newTabImpl->TabViewItem();
            _tabView.TabItems().Append(tabViewItem);

            tabViewItem.PointerPressed({ this, &TerminalPage::_OnTabClick });

            // When the tab requests close, try to close it (prompt for approval, if required)
            newTabImpl->CloseRequested([weakTab, weakThis{ get_weak() }](auto&& /*s*/, auto&& /*e*/) {
                auto page{ weakThis.get() };
                auto tab{ weakTab.get() };

                if (page && tab)
                {
                    page->_HandleCloseTabRequested(*tab);
                }
            });

            // When the tab is closed, remove it from our list of tabs.
            newTabImpl->Closed([tabViewItem, weakThis{ get_weak() }](auto&& /*s*/, auto&& /*e*/) {
                if (auto page{ weakThis.get() })
                {
                    page->_settingsTab = nullptr;
                    page->_RemoveOnCloseRoutine(tabViewItem, page);
                }
            });

            _settingsTab = *newTabImpl;

            // This kicks off TabView::SelectionChanged, in response to which
            // we'll attach the terminal's Xaml control to the Xaml root.
            _tabView.SelectedItem(tabViewItem);
        }
        else
        {
            _tabView.SelectedItem(_settingsTab.TabViewItem());
        }
    }

    // Method Description:
    // - Returns a com_ptr to the implementation type of the given tab if it's a TerminalTab.
    //   If the tab is not a TerminalTab, returns nullptr.
    // Arguments:
    // - tab: the projected type of a Tab
    // Return Value:
    // - If the tab is a TerminalTab, a com_ptr to the implementation type.
    //   If the tab is not a TerminalTab, nullptr
    winrt::com_ptr<TerminalTab> TerminalPage::_GetTerminalTabImpl(const TerminalApp::TabBase& tab)
    {
        if (auto terminalTab = tab.try_as<TerminalApp::TerminalTab>())
        {
            winrt::com_ptr<TerminalTab> tabImpl;
            tabImpl.copy_from(winrt::get_self<TerminalTab>(terminalTab));
            return tabImpl;
        }
        else
        {
            return nullptr;
        }
    }

    // Method Description:
    // - Computes the delta for scrolling the tab's viewport.
    // Arguments:
    // - scrollDirection - direction (up / down) to scroll
    // - rowsToScroll - the number of rows to scroll
    // Return Value:
    // - delta - Signed delta, where a negative value means scrolling up.
    int TerminalPage::_ComputeScrollDelta(ScrollDirection scrollDirection, const uint32_t rowsToScroll)
    {
        return scrollDirection == ScrollUp ? -1 * rowsToScroll : rowsToScroll;
    }

    // Method Description:
    // - Reads system settings for scrolling (based on the step of the mouse scroll).
    // Upon failure fallbacks to default.
    // Return Value:
    // - The number of rows to scroll or a magic value of WHEEL_PAGESCROLL
    // indicating that we need to scroll an entire view height
    uint32_t TerminalPage::_ReadSystemRowsToScroll()
    {
        uint32_t systemRowsToScroll;
        if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &systemRowsToScroll, 0))
        {
            LOG_LAST_ERROR();

            // If SystemParametersInfoW fails, which it shouldn't, fall back to
            // Windows' default value.
            return DefaultRowsToScroll;
        }

        return systemRowsToScroll;
    }

    // Method Description:
    // - Displays a dialog stating the "Touch Keyboard and Handwriting Panel
    //   Service" is disabled.
    void TerminalPage::ShowKeyboardServiceWarning() const
    {
        if (!_IsMessageDismissed(InfoBarMessage::KeyboardServiceWarning))
        {
            if (const auto keyboardServiceWarningInfoBar = FindName(L"KeyboardServiceWarningInfoBar").try_as<MUX::Controls::InfoBar>())
            {
                keyboardServiceWarningInfoBar.IsOpen(true);
            }
        }
    }

    // Method Description:
    // - Displays a info popup guiding the user into setting their default terminal.
    void TerminalPage::ShowSetAsDefaultInfoBar() const
    {
        if (::winrt::Windows::UI::Xaml::Application::Current().try_as<::winrt::TerminalApp::App>() == nullptr)
        {
            // Just ignore this in the tests (where the Application::Current()
            // is not a TerminalApp::App)
            return;
        }
        if (!CascadiaSettings::IsDefaultTerminalAvailable() || _IsMessageDismissed(InfoBarMessage::SetAsDefault))
        {
            return;
        }

        // If the user has already configured any terminal for hand-off we
        // shouldn't inform them again about the possibility to do so.
        if (CascadiaSettings::IsDefaultTerminalSet())
        {
            _DismissMessage(InfoBarMessage::SetAsDefault);
            return;
        }

        if (const auto infoBar = FindName(L"SetAsDefaultInfoBar").try_as<MUX::Controls::InfoBar>())
        {
            TraceLoggingWrite(g_hTerminalAppProvider, "SetAsDefaultTipPresented", TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES), TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
            infoBar.IsOpen(true);
        }
    }

    // Function Description:
    // - Helper function to get the OS-localized name for the "Touch Keyboard
    //   and Handwriting Panel Service". If we can't open up the service for any
    //   reason, then we'll just return the service's key, "TabletInputService".
    // Return Value:
    // - The OS-localized name for the TabletInputService
    winrt::hstring _getTabletServiceName()
    {
        auto isUwp = false;
        try
        {
            isUwp = ::winrt::Windows::UI::Xaml::Application::Current().as<::winrt::TerminalApp::App>().Logic().IsUwp();
        }
        CATCH_LOG();

        if (isUwp)
        {
            return winrt::hstring{ TabletInputServiceKey };
        }

        wil::unique_schandle hManager{ OpenSCManager(nullptr, nullptr, 0) };

        if (LOG_LAST_ERROR_IF(!hManager.is_valid()))
        {
            return winrt::hstring{ TabletInputServiceKey };
        }

        DWORD cchBuffer = 0;
        GetServiceDisplayName(hManager.get(), TabletInputServiceKey.data(), nullptr, &cchBuffer);
        std::wstring buffer;
        cchBuffer += 1; // Add space for a null
        buffer.resize(cchBuffer);

        if (LOG_LAST_ERROR_IF(!GetServiceDisplayName(hManager.get(),
                                                     TabletInputServiceKey.data(),
                                                     buffer.data(),
                                                     &cchBuffer)))
        {
            return winrt::hstring{ TabletInputServiceKey };
        }
        return winrt::hstring{ buffer };
    }

    // Method Description:
    // - Return the fully-formed warning message for the
    //   "KeyboardServiceDisabled" InfoBar. This InfoBar is used to warn the user
    //   if the keyboard service is disabled, and uses the OS localization for
    //   the service's actual name. It's bound to the bar in XAML.
    // Return Value:
    // - The warning message, including the OS-localized service name.
    winrt::hstring TerminalPage::KeyboardServiceDisabledText()
    {
        const auto serviceName{ _getTabletServiceName() };
        const winrt::hstring text{ fmt::format(std::wstring_view(RS_(L"KeyboardServiceWarningText")), serviceName) };
        return text;
    }

    // Method Description:
    // - Hides cursor if required
    // Return Value:
    // - <none>
    void TerminalPage::_HidePointerCursorHandler(const IInspectable& /*sender*/, const IInspectable& /*eventArgs*/)
    {
        if (_shouldMouseVanish && !_isMouseHidden)
        {
            if (auto window{ CoreWindow::GetForCurrentThread() })
            {
                try
                {
                    window.PointerCursor(nullptr);
                    _isMouseHidden = true;
                }
                CATCH_LOG();
            }
        }
    }

    // Method Description:
    // - Restores cursor if required
    // Return Value:
    // - <none>
    void TerminalPage::_RestorePointerCursorHandler(const IInspectable& /*sender*/, const IInspectable& /*eventArgs*/)
    {
        if (_isMouseHidden)
        {
            if (auto window{ CoreWindow::GetForCurrentThread() })
            {
                try
                {
                    window.PointerCursor(_defaultPointerCursor);
                    _isMouseHidden = false;
                }
                CATCH_LOG();
            }
        }
    }

    // Method Description:
    // - Update the RequestedTheme of the specified FrameworkElement and all its
    //   Parent elements. We need to do this so that we can actually theme all
    //   of the elements of the TeachingTip. See GH#9717
    // Arguments:
    // - element: The TeachingTip to set the theme on.
    // Return Value:
    // - <none>
    void TerminalPage::_UpdateTeachingTipTheme(winrt::Windows::UI::Xaml::FrameworkElement element)
    {
        auto theme{ _settings.GlobalSettings().CurrentTheme() };
        auto requestedTheme{ theme.RequestedTheme() };
        while (element)
        {
            element.RequestedTheme(requestedTheme);
            element = element.Parent().try_as<winrt::Windows::UI::Xaml::FrameworkElement>();
        }
    }

    // Method Description:
    // - Display the name and ID of this window in a TeachingTip. If the window
    //   has no name, the name will be presented as "<unnamed-window>".
    // - This can be invoked by either:
    //   * An identifyWindow action, that displays the info only for the current
    //     window
    //   * An identifyWindows action, that displays the info for all windows.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalPage::IdentifyWindow()
    {
        auto weakThis{ get_weak() };
        co_await wil::resume_foreground(Dispatcher());
        if (auto page{ weakThis.get() })
        {
            // If we haven't ever loaded the TeachingTip, then do so now and
            // create the toast for it.
            if (page->_windowIdToast == nullptr)
            {
                if (auto tip{ page->FindName(L"WindowIdToast").try_as<MUX::Controls::TeachingTip>() })
                {
                    page->_windowIdToast = std::make_shared<Toast>(tip);
                    // Make sure to use the weak ref when setting up this
                    // callback.
                    tip.Closed({ page->get_weak(), &TerminalPage::_FocusActiveControl });
                }
            }
            _UpdateTeachingTipTheme(WindowIdToast().try_as<winrt::Windows::UI::Xaml::FrameworkElement>());

            if (page->_windowIdToast != nullptr)
            {
                page->_windowIdToast->Open();
            }
        }
    }

    // WindowName is a otherwise generic WINRT_OBSERVABLE_PROPERTY, but it needs
    // to raise a PropertyChanged for WindowNameForDisplay, instead of
    // WindowName.
    winrt::hstring TerminalPage::WindowName() const noexcept
    {
        return _WindowName;
    }

    winrt::fire_and_forget TerminalPage::WindowName(const winrt::hstring& value)
    {
        const auto oldIsQuakeMode = IsQuakeWindow();
        const auto changed = _WindowName != value;
        if (changed)
        {
            _WindowName = value;
        }
        auto weakThis{ get_weak() };
        // On the foreground thread, raise property changed notifications, and
        // display the success toast.
        co_await wil::resume_foreground(Dispatcher());
        if (auto page{ weakThis.get() })
        {
            if (changed)
            {
                page->_PropertyChangedHandlers(*this, WUX::Data::PropertyChangedEventArgs{ L"WindowName" });
                page->_PropertyChangedHandlers(*this, WUX::Data::PropertyChangedEventArgs{ L"WindowNameForDisplay" });

                // DON'T display the confirmation if this is the name we were
                // given on startup!
                if (page->_startupState == StartupState::Initialized)
                {
                    page->IdentifyWindow();

                    // If we're entering quake mode, or leaving it
                    if (IsQuakeWindow() != oldIsQuakeMode)
                    {
                        // If we're entering Quake Mode from ~Focus Mode, then this will enter Focus Mode
                        // If we're entering Quake Mode from Focus Mode, then this will do nothing
                        // If we're leaving Quake Mode (we're already in Focus Mode), then this will do nothing
                        SetFocusMode(true);
                        _IsQuakeWindowChangedHandlers(*this, nullptr);
                    }
                }
            }
        }
    }

    // WindowId is a otherwise generic WINRT_OBSERVABLE_PROPERTY, but it needs
    // to raise a PropertyChanged for WindowIdForDisplay, instead of
    // WindowId.
    uint64_t TerminalPage::WindowId() const noexcept
    {
        return _WindowId;
    }
    void TerminalPage::WindowId(const uint64_t& value)
    {
        if (_WindowId != value)
        {
            _WindowId = value;
            _PropertyChangedHandlers(*this, WUX::Data::PropertyChangedEventArgs{ L"WindowIdForDisplay" });
        }
    }

    void TerminalPage::SetPersistedLayoutIdx(const uint32_t idx)
    {
        _loadFromPersistedLayoutIdx = idx;
    }

    void TerminalPage::SetNumberOfOpenWindows(const uint64_t num)
    {
        _numOpenWindows = num;
    }

    // Method Description:
    // - Returns a label like "Window: 1234" for the ID of this window
    // Arguments:
    // - <none>
    // Return Value:
    // - a string for displaying the name of the window.
    winrt::hstring TerminalPage::WindowIdForDisplay() const noexcept
    {
        return winrt::hstring{ fmt::format(L"{}: {}",
                                           std::wstring_view(RS_(L"WindowIdLabel")),
                                           _WindowId) };
    }

    // Method Description:
    // - Returns a label like "<unnamed window>" when the window has no name, or the name of the window.
    // Arguments:
    // - <none>
    // Return Value:
    // - a string for displaying the name of the window.
    winrt::hstring TerminalPage::WindowNameForDisplay() const noexcept
    {
        return _WindowName.empty() ?
                   winrt::hstring{ fmt::format(L"<{}>", RS_(L"UnnamedWindowName")) } :
                   _WindowName;
    }

    // Method Description:
    // - Called when an attempt to rename the window has failed. This will open
    //   the toast displaying a message to the user that the attempt to rename
    //   the window has failed.
    // - This will load the RenameFailedToast TeachingTip the first time it's called.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalPage::RenameFailed()
    {
        auto weakThis{ get_weak() };
        co_await wil::resume_foreground(Dispatcher());
        if (auto page{ weakThis.get() })
        {
            // If we haven't ever loaded the TeachingTip, then do so now and
            // create the toast for it.
            if (page->_windowRenameFailedToast == nullptr)
            {
                if (auto tip{ page->FindName(L"RenameFailedToast").try_as<MUX::Controls::TeachingTip>() })
                {
                    page->_windowRenameFailedToast = std::make_shared<Toast>(tip);
                    // Make sure to use the weak ref when setting up this
                    // callback.
                    tip.Closed({ page->get_weak(), &TerminalPage::_FocusActiveControl });
                }
            }
            _UpdateTeachingTipTheme(RenameFailedToast().try_as<winrt::Windows::UI::Xaml::FrameworkElement>());

            if (page->_windowRenameFailedToast != nullptr)
            {
                page->_windowRenameFailedToast->Open();
            }
        }
    }

    // Method Description:
    // - Called when the user hits the "Ok" button on the WindowRenamer TeachingTip.
    // - Will raise an event that will bubble up to the monarch, asking if this
    //   name is acceptable.
    //   - If it is, we'll eventually get called back in TerminalPage::WindowName(hstring).
    //   - If not, then TerminalPage::RenameFailed will get called.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void TerminalPage::_WindowRenamerActionClick(const IInspectable& /*sender*/,
                                                 const IInspectable& /*eventArgs*/)
    {
        auto newName = WindowRenamerTextBox().Text();
        _RequestWindowRename(newName);
    }

    void TerminalPage::_RequestWindowRename(const winrt::hstring& newName)
    {
        auto request = winrt::make<implementation::RenameWindowRequestedArgs>(newName);
        // The WindowRenamer is _not_ a Toast - we want it to stay open until
        // the user dismisses it.
        if (WindowRenamer())
        {
            WindowRenamer().IsOpen(false);
        }
        _RenameWindowRequestedHandlers(*this, request);
        // We can't just use request.Successful here, because the handler might
        // (will) be handling this asynchronously, so when control returns to
        // us, this hasn't actually been handled yet. We'll get called back in
        // RenameFailed if this fails.
        //
        // Theoretically we could do a IAsyncOperation<RenameWindowResult> kind
        // of thing with co_return winrt::make<RenameWindowResult>(false).
    }

    // Method Description:
    // - Used to track if the user pressed enter with the renamer open. If we
    //   immediately focus it after hitting Enter on the command palette, then
    //   the Enter keydown will dismiss the command palette and open the
    //   renamer, and then the enter keyup will go to the renamer. So we need to
    //   make sure both a down and up go to the renamer.
    // Arguments:
    // - e: the KeyRoutedEventArgs describing the key that was released
    // Return Value:
    // - <none>
    void TerminalPage::_WindowRenamerKeyDown(const IInspectable& /*sender*/,
                                             const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto key = e.OriginalKey();
        if (key == Windows::System::VirtualKey::Enter)
        {
            _renamerPressedEnter = true;
        }
    }

    // Method Description:
    // - Manually handle Enter and Escape for committing and dismissing a window
    //   rename. This is highly similar to the TabHeaderControl's KeyUp handler.
    // Arguments:
    // - e: the KeyRoutedEventArgs describing the key that was released
    // Return Value:
    // - <none>
    void TerminalPage::_WindowRenamerKeyUp(const IInspectable& sender,
                                           const winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs& e)
    {
        const auto key = e.OriginalKey();
        if (key == Windows::System::VirtualKey::Enter && _renamerPressedEnter)
        {
            // User is done making changes, close the rename box
            _WindowRenamerActionClick(sender, nullptr);
        }
        else if (key == Windows::System::VirtualKey::Escape)
        {
            // User wants to discard the changes they made
            WindowRenamerTextBox().Text(WindowName());
            WindowRenamer().IsOpen(false);
            _renamerPressedEnter = false;
        }
    }

    bool TerminalPage::IsQuakeWindow() const noexcept
    {
        return WindowName() == QuakeWindowName;
    }

    // Method Description:
    // - This function stops people from duplicating the base profile, because
    //   it gets ~ ~ weird ~ ~ when they do. Remove when TODO GH#5047 is done.
    Profile TerminalPage::GetClosestProfileForDuplicationOfProfile(const Profile& profile) const noexcept
    {
        if (profile == _settings.ProfileDefaults())
        {
            return _settings.FindProfile(_settings.GlobalSettings().DefaultProfile());
        }
        return profile;
    }

    // Function Description:
    // - Helper to launch a new WT instance elevated. It'll do this by spawning
    //   a helper process, who will asking the shell to elevate the process for
    //   us. This might cause a UAC prompt. The elevation is performed on a
    //   background thread, as to not block the UI thread.
    // Arguments:
    // - newTerminalArgs: A NewTerminalArgs describing the terminal instance
    //   that should be spawned. The Profile should be filled in with the GUID
    //   of the profile we want to launch.
    // Return Value:
    // - <none>
    // Important: Don't take the param by reference, since we'll be doing work
    // on another thread.
    void TerminalPage::_OpenElevatedWT(NewTerminalArgs newTerminalArgs)
    {
        // BODGY
        //
        // We're going to construct the commandline we want, then toss it to a
        // helper process called `elevate-shim.exe` that happens to live next to
        // us. elevate-shim.exe will be the one to call ShellExecute with the
        // args that we want (to elevate the given profile).
        //
        // We can't be the one to call ShellExecute ourselves. ShellExecute
        // requires that the calling process stays alive until the child is
        // spawned. However, in the case of something like `wt -p
        // AlwaysElevateMe`, then the original WT will try to ShellExecute a new
        // wt.exe (elevated) and immediately exit, preventing ShellExecute from
        // successfully spawning the elevated WT.

        std::filesystem::path exePath = wil::GetModuleFileNameW<std::wstring>(nullptr);
        exePath.replace_filename(L"elevate-shim.exe");

        // Build the commandline to pass to wt for this set of NewTerminalArgs
        auto cmdline{
            fmt::format(L"new-tab {}", newTerminalArgs.ToCommandline().c_str())
        };

        wil::unique_process_information pi;
        STARTUPINFOW si{};
        si.cb = sizeof(si);

        LOG_IF_WIN32_BOOL_FALSE(CreateProcessW(exePath.c_str(),
                                               cmdline.data(),
                                               nullptr,
                                               nullptr,
                                               FALSE,
                                               0,
                                               nullptr,
                                               nullptr,
                                               &si,
                                               &pi));

        // TODO: GH#8592 - It may be useful to pop a Toast here in the original
        // Terminal window informing the user that the tab was opened in a new
        // window.
    }

    // Method Description:
    // - If the requested settings want us to elevate this new terminal
    //   instance, and we're not currently elevated, then open the new terminal
    //   as an elevated instance (using _OpenElevatedWT). Does nothing if we're
    //   already elevated, or if the control settings don't want to be elevated.
    // Arguments:
    // - newTerminalArgs: The NewTerminalArgs for this terminal instance
    // - controlSettings: The constructed TerminalSettingsCreateResult for this Terminal instance
    // - profile: The Profile we're using to launch this Terminal instance
    // Return Value:
    // - true iff we tossed this request to an elevated window. Callers can use
    //   this result to early-return if needed.
    bool TerminalPage::_maybeElevate(const NewTerminalArgs& newTerminalArgs,
                                     const TerminalSettingsCreateResult& controlSettings,
                                     const Profile& profile)
    {
        // Try to handle auto-elevation
        const auto requestedElevation = controlSettings.DefaultSettings().Elevate();
        const auto currentlyElevated = IsElevated();

        // We aren't elevated, but we want to be.
        if (requestedElevation && !currentlyElevated)
        {
            // Manually set the Profile of the NewTerminalArgs to the guid we've
            // resolved to. If there was a profile in the NewTerminalArgs, this
            // will be that profile's GUID. If there wasn't, then we'll use
            // whatever the default profile's GUID is.

            newTerminalArgs.Profile(::Microsoft::Console::Utils::GuidToString(profile.Guid()));
            _OpenElevatedWT(newTerminalArgs);
            return true;
        }
        return false;
    }

    // Method Description:
    // - Handles the change of connection state.
    // If the connection state is failure show information bar suggesting to configure termination behavior
    // (unless user asked not to show this message again)
    // Arguments:
    // - sender: the ICoreState instance containing the connection state
    // Return Value:
    // - <none>
    winrt::fire_and_forget TerminalPage::_ConnectionStateChangedHandler(const IInspectable& sender, const IInspectable& /*args*/) const
    {
        if (const auto coreState{ sender.try_as<winrt::Microsoft::Terminal::Control::ICoreState>() })
        {
            const auto newConnectionState = coreState.ConnectionState();
            if (newConnectionState == ConnectionState::Failed && !_IsMessageDismissed(InfoBarMessage::CloseOnExitInfo))
            {
                co_await wil::resume_foreground(Dispatcher());
                if (const auto infoBar = FindName(L"CloseOnExitInfoBar").try_as<MUX::Controls::InfoBar>())
                {
                    infoBar.IsOpen(true);
                }
            }
        }
    }

    // Method Description:
    // - Persists the user's choice not to show information bar guiding to configure termination behavior.
    // Then hides this information buffer.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_CloseOnExitInfoDismissHandler(const IInspectable& /*sender*/, const IInspectable& /*args*/) const
    {
        _DismissMessage(InfoBarMessage::CloseOnExitInfo);
        if (const auto infoBar = FindName(L"CloseOnExitInfoBar").try_as<MUX::Controls::InfoBar>())
        {
            infoBar.IsOpen(false);
        }
    }

    // Method Description:
    // - Persists the user's choice not to show information bar warning about "Touch keyboard and Handwriting Panel Service" disabled
    // Then hides this information buffer.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_KeyboardServiceWarningInfoDismissHandler(const IInspectable& /*sender*/, const IInspectable& /*args*/) const
    {
        _DismissMessage(InfoBarMessage::KeyboardServiceWarning);
        if (const auto infoBar = FindName(L"KeyboardServiceWarningInfoBar").try_as<MUX::Controls::InfoBar>())
        {
            infoBar.IsOpen(false);
        }
    }

    // Method Description:
    // - Persists the user's choice not to show the information bar warning about "Windows Terminal can be set as your default terminal application"
    // Then hides this information buffer.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TerminalPage::_SetAsDefaultDismissHandler(const IInspectable& /*sender*/, const IInspectable& /*args*/)
    {
        _DismissMessage(InfoBarMessage::SetAsDefault);
        if (const auto infoBar = FindName(L"SetAsDefaultInfoBar").try_as<MUX::Controls::InfoBar>())
        {
            infoBar.IsOpen(false);
        }

        TraceLoggingWrite(g_hTerminalAppProvider, "SetAsDefaultTipDismissed", TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES), TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        _FocusCurrentTab(true);
    }

    // Method Description:
    // - Dismisses the Default Terminal tip and opens the settings.
    void TerminalPage::_SetAsDefaultOpenSettingsHandler(const IInspectable& /*sender*/, const IInspectable& /*args*/)
    {
        if (const auto infoBar = FindName(L"SetAsDefaultInfoBar").try_as<MUX::Controls::InfoBar>())
        {
            infoBar.IsOpen(false);
        }

        TraceLoggingWrite(g_hTerminalAppProvider, "SetAsDefaultTipInteracted", TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES), TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));

        OpenSettingsUI();
    }

    // Method Description:
    // - Checks whether information bar message was dismissed earlier (in the application state)
    // Arguments:
    // - message: message to look for in the state
    // Return Value:
    // - true, if the message was dismissed
    bool TerminalPage::_IsMessageDismissed(const InfoBarMessage& message)
    {
        if (const auto dismissedMessages{ ApplicationState::SharedInstance().DismissedMessages() })
        {
            for (const auto& dismissedMessage : dismissedMessages)
            {
                if (dismissedMessage == message)
                {
                    return true;
                }
            }
        }
        return false;
    }

    // Method Description:
    // - Persists the user's choice to dismiss information bar message (in application state)
    // Arguments:
    // - message: message to dismiss
    // Return Value:
    // - <none>
    void TerminalPage::_DismissMessage(const InfoBarMessage& message)
    {
        const auto applicationState = ApplicationState::SharedInstance();
        std::vector<InfoBarMessage> messages;

        if (const auto values = applicationState.DismissedMessages())
        {
            messages.resize(values.Size());
            values.GetMany(0, messages);
        }

        if (std::none_of(messages.begin(), messages.end(), [&](const auto& m) { return m == message; }))
        {
            messages.emplace_back(message);
        }

        applicationState.DismissedMessages(std::move(messages));
    }

    void TerminalPage::_updateThemeColors()
    {
        if (_settings == nullptr)
        {
            return;
        }

        const auto theme = _settings.GlobalSettings().CurrentTheme();
        auto requestedTheme{ theme.RequestedTheme() };

        // First: Update the colors of our individual TabViewItems. This applies tab.background to the tabs via TerminalTab::ThemeColor
        {
            auto tabBackground = theme.Tab() ? theme.Tab().Background() : nullptr;
            for (const auto& tab : _tabs)
            {
                if (const auto& terminalTabImpl{ _GetTerminalTabImpl(tab) })
                {
                    terminalTabImpl->ThemeColor(tabBackground);
                }
            }
        }

        const auto res = Application::Current().Resources();

        // XAML Hacks:
        //
        // the App is always in the OS theme, so the
        // App::Current().Resources() lookup will always get the value for the
        // OS theme, not the requested theme.
        //
        // This helper allows us to instead lookup the value of a resource
        // specified by `key` for the given `requestedTheme`, from the
        // dictionaries in App.xaml. Make sure the value is actually there!
        // Otherwise this'll throw like any other Lookup for a resource that
        // isn't there.
        static const auto lookup = [](auto& res, auto& requestedTheme, auto& key) {
            // You want the Default version of the resource? Great, the App is
            // always in the OS theme. Just look it up and be done.
            if (requestedTheme == ElementTheme::Default)
            {
                return res.Lookup(key);
            }
            static const auto lightKey = winrt::box_value(L"Light");
            static const auto darkKey = winrt::box_value(L"Dark");
            // There isn't an ElementTheme::HighContrast.

            auto requestedThemeKey = requestedTheme == ElementTheme::Dark ? darkKey : lightKey;
            for (const auto& dictionary : res.MergedDictionaries())
            {
                // Don't look in the MUX resources. They come first. A person
                // with more patience than me may find a way to look through our
                // dictionaries first, then the MUX ones, but that's not needed
                // currently
                if (dictionary.Source())
                {
                    continue;
                }
                // Look through the theme dictionaries we defined:
                for (const auto& [dictionaryKey, dict] : dictionary.ThemeDictionaries())
                {
                    // Does the key for this dict match the theme we're looking for?
                    if (winrt::unbox_value<winrt::hstring>(dictionaryKey) !=
                        winrt::unbox_value<winrt::hstring>(requestedThemeKey))
                    {
                        // No? skip it.
                        continue;
                    }
                    // Look for the requested resource in this dict.
                    const auto themeDictionary = dict.as<winrt::Windows::UI::Xaml::ResourceDictionary>();
                    if (themeDictionary.HasKey(key))
                    {
                        return themeDictionary.Lookup(key);
                    }
                }
            }

            // We didn't find it in the requested dict, fall back to the default dictionary.
            return res.Lookup(key);
        };

        // Use our helper to lookup the theme-aware version of the resource.
        const auto tabViewBackgroundKey = winrt::box_value(L"TabViewBackground");
        const auto backgroundSolidBrush = lookup(res, requestedTheme, tabViewBackgroundKey).as<Media::SolidColorBrush>();

        til::color bgColor = backgroundSolidBrush.Color();

        if (_settings.GlobalSettings().UseAcrylicInTabRow())
        {
            const auto acrylicBrush = Media::AcrylicBrush();
            acrylicBrush.BackgroundSource(Media::AcrylicBackgroundSource::HostBackdrop);
            acrylicBrush.FallbackColor(bgColor);
            acrylicBrush.TintColor(bgColor);
            acrylicBrush.TintOpacity(0.5);

            TitlebarBrush(acrylicBrush);
        }
        else if (auto tabRowBg{ theme.TabRow() ? (_activated ? theme.TabRow().Background() :
                                                               theme.TabRow().UnfocusedBackground()) :
                                                 ThemeColor{ nullptr } })
        {
            const auto terminalBrush = [this]() -> Media::Brush {
                if (const auto& control{ _GetActiveControl() })
                {
                    return control.BackgroundBrush();
                }
                else if (auto settingsTab = _GetFocusedTab().try_as<TerminalApp::SettingsTab>())
                {
                    return settingsTab.Content().try_as<Settings::Editor::MainPage>().BackgroundBrush();
                }
                return nullptr;
            }();

            const auto themeBrush{ tabRowBg.Evaluate(res, terminalBrush, true) };
            bgColor = ThemeColor::ColorFromBrush(themeBrush);
            TitlebarBrush(themeBrush);
        }
        else
        {
            // Nothing was set in the theme - fall back to our original `TabViewBackground` color.
            TitlebarBrush(backgroundSolidBrush);
        }

        if (!_settings.GlobalSettings().ShowTabsInTitlebar())
        {
            _tabRow.Background(TitlebarBrush());
        }

        // Update the new tab button to have better contrast with the new color.
        // In theory, it would be convenient to also change these for the
        // inactive tabs as well, but we're leaving that as a follow up.
        _SetNewTabButtonColor(bgColor, bgColor);
    }

    void TerminalPage::WindowActivated(const bool activated)
    {
        // Stash if we're activated. Use that when we reload
        // the settings, change active panes, etc.
        _activated = activated;
        _updateThemeColors();
    }
}
