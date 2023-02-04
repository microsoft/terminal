// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TerminalPage.g.h"
#include "TerminalTab.h"
#include "AppKeyBindings.h"
#include "AppCommandlineArgs.h"
#include "RenameWindowRequestedArgs.g.h"
#include "Toast.h"

#define DECLARE_ACTION_HANDLER(action) void _Handle##action(const IInspectable& sender, const MTSM::ActionEventArgs& args);

static constexpr uint32_t DefaultRowsToScroll{ 3 };
static constexpr std::wstring_view TabletInputServiceKey{ L"TabletInputService" };

namespace TerminalAppLocalTests
{
    class TabTests;
    class SettingsTests;
}

namespace Microsoft::Terminal::Core
{
    class ControlKeyStates;
}

namespace winrt::TerminalApp::implementation
{
    enum StartupState : int
    {
        NotInitialized = 0,
        InStartup = 1,
        Initialized = 2
    };

    enum ScrollDirection : int
    {
        ScrollUp = 0,
        ScrollDown = 1
    };

    struct RenameWindowRequestedArgs : RenameWindowRequestedArgsT<RenameWindowRequestedArgs>
    {
        WINRT_PROPERTY(winrt::hstring, ProposedName);

    public:
        RenameWindowRequestedArgs(const winrt::hstring& name) :
            _ProposedName{ name } {};
    };

    struct TerminalPage : TerminalPageT<TerminalPage>
    {
    public:
        TerminalPage();

        // This implements shobjidl's IInitializeWithWindow, but due to a XAML Compiler bug we cannot
        // put it in our inheritance graph. https://github.com/microsoft/microsoft-ui-xaml/issues/3331
        STDMETHODIMP Initialize(HWND hwnd);

        void SetSettings(MTSM::CascadiaSettings settings, bool needRefreshUI);

        void Create();

        bool ShouldUsePersistedLayout(MTSM::CascadiaSettings& settings) const;
        bool ShouldImmediatelyHandoffToElevated(const MTSM::CascadiaSettings& settings) const;
        void HandoffToElevated(const MTSM::CascadiaSettings& settings);
        std::optional<uint32_t> LoadPersistedLayoutIdx(MTSM::CascadiaSettings& settings) const;
        MTSM::WindowLayout LoadPersistedLayout(MTSM::CascadiaSettings& settings) const;
        MTSM::WindowLayout GetWindowLayout();

        winrt::fire_and_forget NewTerminalByDrop(WUX::DragEventArgs& e);

        hstring Title();

        void TitlebarClicked();
        void WindowVisibilityChanged(const bool showOrHide);

        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        winrt::hstring ApplicationDisplayName();
        winrt::hstring ApplicationVersion();

        winrt::fire_and_forget RequestQuit();
        winrt::fire_and_forget CloseWindow(bool bypassDialog);

        void ToggleFocusMode();
        void ToggleFullscreen();
        void ToggleAlwaysOnTop();
        bool FocusMode() const;
        bool Fullscreen() const;
        bool AlwaysOnTop() const;
        void SetFullscreen(bool);
        void SetFocusMode(const bool inFocusMode);
        void Maximized(bool newMaximized);
        void RequestSetMaximized(bool newMaximized);

        void SetStartupActions(std::vector<MTSM::ActionAndArgs>& actions);

        void SetInboundListener(bool isEmbedding);
        static std::vector<MTSM::ActionAndArgs> ConvertExecuteCommandlineToActions(const MTSM::ExecuteCommandlineArgs& args);

        MTApp::IDialogPresenter DialogPresenter() const;
        void DialogPresenter(MTApp::IDialogPresenter dialogPresenter);

        MTApp::TaskbarState TaskbarState() const;

        void ShowKeyboardServiceWarning() const;
        void ShowSetAsDefaultInfoBar() const;
        winrt::hstring KeyboardServiceDisabledText();

        winrt::fire_and_forget IdentifyWindow();
        winrt::fire_and_forget RenameFailed();

        winrt::fire_and_forget ProcessStartupActions(WFC::IVector<MTSM::ActionAndArgs> actions,
                                                     const bool initial,
                                                     const winrt::hstring cwd = L"");

        // Normally, WindowName and WindowId would be
        // WINRT_OBSERVABLE_PROPERTY's, but we want them to raise
        // WindowNameForDisplay and WindowIdForDisplay instead
        winrt::hstring WindowName() const noexcept;
        winrt::fire_and_forget WindowName(const winrt::hstring& value);
        uint64_t WindowId() const noexcept;
        void WindowId(const uint64_t& value);

        void SetNumberOfOpenWindows(const uint64_t value);
        void SetPersistedLayoutIdx(const uint32_t value);

        winrt::hstring WindowIdForDisplay() const noexcept;
        winrt::hstring WindowNameForDisplay() const noexcept;
        bool IsQuakeWindow() const noexcept;
        bool IsElevated() const noexcept;

        void OpenSettingsUI();
        void WindowActivated(const bool activated);

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);

        // -------------------------------- WinRT Events ---------------------------------
        TYPED_EVENT(TitleChanged, IInspectable, winrt::hstring);
        TYPED_EVENT(LastTabClosed, IInspectable, MTApp::LastTabClosedEventArgs);
        TYPED_EVENT(SetTitleBarContent, IInspectable, WUX::UIElement);
        TYPED_EVENT(FocusModeChanged, IInspectable, IInspectable);
        TYPED_EVENT(FullscreenChanged, IInspectable, IInspectable);
        TYPED_EVENT(ChangeMaximizeRequested, IInspectable, IInspectable);
        TYPED_EVENT(AlwaysOnTopChanged, IInspectable, IInspectable);
        TYPED_EVENT(RaiseVisualBell, IInspectable, IInspectable);
        TYPED_EVENT(SetTaskbarProgress, IInspectable, IInspectable);
        TYPED_EVENT(Initialized, IInspectable, WUX::RoutedEventArgs);
        TYPED_EVENT(IdentifyWindowsRequested, IInspectable, IInspectable);
        TYPED_EVENT(RenameWindowRequested, WF::IInspectable, MTApp::RenameWindowRequestedArgs);
        TYPED_EVENT(IsQuakeWindowChanged, IInspectable, IInspectable);
        TYPED_EVENT(SummonWindowRequested, IInspectable, IInspectable);
        TYPED_EVENT(CloseRequested, IInspectable, IInspectable);
        TYPED_EVENT(OpenSystemMenu, IInspectable, IInspectable);
        TYPED_EVENT(QuitRequested, IInspectable, IInspectable);
        TYPED_EVENT(ShowWindowChanged, IInspectable, MTControl::ShowWindowArgs)

        WINRT_OBSERVABLE_PROPERTY(WUXMedia::Brush, TitlebarBrush, _PropertyChangedHandlers, nullptr);

    private:
        friend struct TerminalPageT<TerminalPage>; // for Xaml to bind events
        std::optional<HWND> _hostingHwnd;

        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the app.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in App::_ApplyTheme. The roots currently is _tabRow
        // (which is a root when the tabs are in the titlebar.)
        MUXC::TabView _tabView{ nullptr };
        MTApp::TabRowControl _tabRow{ nullptr };
        WUXC::Grid _tabContent{ nullptr };
        MUXC::SplitButton _newTabButton{ nullptr };
        MTApp::ColorPickupFlyout _tabColorPicker{ nullptr };

        MTSM::CascadiaSettings _settings{ nullptr };

        WFC::IObservableVector<MTApp::TabBase> _tabs;
        WFC::IObservableVector<MTApp::TabBase> _mruTabs;
        static winrt::com_ptr<TerminalTab> _GetTerminalTabImpl(const MTApp::TabBase& tab);

        void _UpdateTabIndices();

        MTApp::SettingsTab _settingsTab{ nullptr };

        bool _isInFocusMode{ false };
        bool _isFullscreen{ false };
        bool _isMaximized{ false };
        bool _isAlwaysOnTop{ false };
        winrt::hstring _WindowName{};
        uint64_t _WindowId{ 0 };
        std::optional<uint32_t> _loadFromPersistedLayoutIdx{};
        uint64_t _numOpenWindows{ 0 };

        bool _maintainStateOnTabClose{ false };
        bool _rearranging{ false };
        std::optional<int> _rearrangeFrom{};
        std::optional<int> _rearrangeTo{};
        bool _removing{ false };

        bool _activated{ false };
        bool _visible{ true };

        std::vector<std::vector<MTSM::ActionAndArgs>> _previouslyClosedPanesAndTabs{};

        uint32_t _systemRowsToScroll{ DefaultRowsToScroll };

        // use a weak reference to prevent circular dependency with AppLogic
        winrt::weak_ref<MTApp::IDialogPresenter> _dialogPresenter;

        winrt::com_ptr<AppKeyBindings> _bindings{ winrt::make_self<implementation::AppKeyBindings>() };
        winrt::com_ptr<ShortcutActionDispatch> _actionDispatch{ winrt::make_self<implementation::ShortcutActionDispatch>() };

        WUXC::Grid::LayoutUpdated_revoker _layoutUpdatedRevoker;
        StartupState _startupState{ StartupState::NotInitialized };

        WFC::IVector<MTSM::ActionAndArgs> _startupActions;
        bool _shouldStartInboundListener{ false };
        bool _isEmbeddingInboundListener{ false };

        std::shared_ptr<Toast> _windowIdToast{ nullptr };
        std::shared_ptr<Toast> _windowRenameFailedToast{ nullptr };

        WUXC::TextBox::LayoutUpdated_revoker _renamerLayoutUpdatedRevoker;
        int _renamerLayoutCount{ 0 };
        bool _renamerPressedEnter{ false };

        WF::IAsyncOperation<WUXC::ContentDialogResult> _ShowDialogHelper(const std::wstring_view& name);

        void _ShowAboutDialog();
        WF::IAsyncOperation<WUXC::ContentDialogResult> _ShowQuitDialog();
        WF::IAsyncOperation<WUXC::ContentDialogResult> _ShowCloseWarningDialog();
        WF::IAsyncOperation<WUXC::ContentDialogResult> _ShowCloseReadOnlyDialog();
        WF::IAsyncOperation<WUXC::ContentDialogResult> _ShowMultiLinePasteWarningDialog();
        WF::IAsyncOperation<WUXC::ContentDialogResult> _ShowLargePasteWarningDialog();

        void _CreateNewTabFlyout();
        std::vector<WUXC::MenuFlyoutItemBase> _CreateNewTabFlyoutItems(WFC::IVector<MTSM::NewTabMenuEntry> entries);
        WUXC::IconElement _CreateNewTabFlyoutIcon(const winrt::hstring& icon);
        WUXC::MenuFlyoutItem _CreateNewTabFlyoutProfile(const MTSM::Profile profile, int profileIndex);

        void _OpenNewTabDropdown();
        HRESULT _OpenNewTab(const MTSM::NewTerminalArgs& newTerminalArgs, MTConnection::ITerminalConnection existingConnection = nullptr);
        void _CreateNewTabFromPane(std::shared_ptr<Pane> pane, uint32_t insertPosition = -1);
        MTConnection::ITerminalConnection _CreateConnectionFromSettings(MTSM::Profile profile, MTSM::TerminalSettings settings);

        winrt::fire_and_forget _OpenNewWindow(const MTSM::NewTerminalArgs newTerminalArgs);

        void _OpenNewTerminalViaDropdown(const MTSM::NewTerminalArgs newTerminalArgs);

        bool _displayingCloseDialog{ false };
        void _SettingsButtonOnClick(const IInspectable& sender, const WUX::RoutedEventArgs& eventArgs);
        void _CommandPaletteButtonOnClick(const IInspectable& sender, const WUX::RoutedEventArgs& eventArgs);
        void _AboutButtonOnClick(const IInspectable& sender, const WUX::RoutedEventArgs& eventArgs);
        void _ThirdPartyNoticesOnClick(const IInspectable& sender, const WUX::RoutedEventArgs& eventArgs);
        void _SendFeedbackOnClick(const IInspectable& sender, const WUXC::ContentDialogButtonClickEventArgs& eventArgs);

        void _KeyDownHandler(const WF::IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);
        static ::Microsoft::Terminal::Core::ControlKeyStates _GetPressedModifierKeys() noexcept;
        static void _ClearKeyboardState(const WORD vkey, const WORD scanCode) noexcept;
        void _HookupKeyBindings(const MTSM::IActionMapView& actionMap) noexcept;
        void _RegisterActionCallbacks();

        void _UpdateTitle(const TerminalTab& tab);
        void _UpdateTabIcon(TerminalTab& tab);
        void _UpdateTabView();
        void _UpdateTabWidthMode();
        void _UpdateCommandsForPalette();
        void _SetBackgroundImage(const MTSM::IAppearanceConfig& newAppearance);

        static WFC::IMap<winrt::hstring, MTSM::Command> _ExpandCommands(WFC::IMapView<winrt::hstring, MTSM::Command> commandsToExpand,
                                                                        WFC::IVectorView<MTSM::Profile> profiles,
                                                                        WFC::IMapView<winrt::hstring, MTSM::ColorScheme> schemes);

        void _DuplicateFocusedTab();
        void _DuplicateTab(const TerminalTab& tab);

        void _SplitTab(TerminalTab& tab);
        winrt::fire_and_forget _ExportTab(const TerminalTab& tab, winrt::hstring filepath);

        WF::IAsyncAction _HandleCloseTabRequested(MTApp::TabBase tab);
        void _CloseTabAtIndex(uint32_t index);
        void _RemoveTab(const MTApp::TabBase& tab);
        winrt::fire_and_forget _RemoveTabs(const std::vector<MTApp::TabBase> tabs);

        void _InitializeTab(winrt::com_ptr<TerminalTab> newTabImpl, uint32_t insertPosition = -1);
        void _RegisterTerminalEvents(MTControl::TermControl term);
        void _RegisterTabEvents(TerminalTab& hostingTab);

        void _DismissTabContextMenus();
        void _FocusCurrentTab(const bool focusAlways);
        bool _HasMultipleTabs() const;
        void _RemoveAllTabs();

        void _SelectNextTab(const bool bMoveRight, const WF::IReference<MTSM::TabSwitcherMode>& customTabSwitcherMode);
        bool _SelectTab(uint32_t tabIndex);
        bool _MoveFocus(const MTSM::FocusDirection& direction);
        bool _SwapPane(const MTSM::FocusDirection& direction);
        bool _MovePane(const uint32_t tabIdx);

        template<typename F>
        bool _ApplyToActiveControls(F f)
        {
            if (const auto tab{ _GetFocusedTabImpl() })
            {
                if (const auto activePane = tab->GetActivePane())
                {
                    activePane->WalkTree([&](auto p) {
                        if (const auto& control{ p->GetTerminalControl() })
                        {
                            f(control);
                        }
                    });

                    return true;
                }
            }
            return false;
        }

        MTControl::TermControl _GetActiveControl();
        std::optional<uint32_t> _GetFocusedTabIndex() const noexcept;
        MTApp::TabBase _GetFocusedTab() const noexcept;
        winrt::com_ptr<TerminalTab> _GetFocusedTabImpl() const noexcept;
        MTApp::TabBase _GetTabByTabViewItem(const MUXC::TabViewItem& tabViewItem) const noexcept;

        void _HandleClosePaneRequested(std::shared_ptr<Pane> pane);
        winrt::fire_and_forget _SetFocusedTab(const MTApp::TabBase tab);
        winrt::fire_and_forget _CloseFocusedPane();
        void _ClosePanes(weak_ref<TerminalTab> weakTab, std::vector<uint32_t> paneIds);
        WF::IAsyncOperation<bool> _PaneConfirmCloseReadOnly(std::shared_ptr<Pane> pane);
        void _AddPreviouslyClosedPaneOrTab(std::vector<MTSM::ActionAndArgs>&& args);

        winrt::fire_and_forget _RemoveOnCloseRoutine(MUXC::TabViewItem tabViewItem, winrt::com_ptr<TerminalPage> page);

        void _Scroll(ScrollDirection scrollDirection, const WF::IReference<uint32_t>& rowsToScroll);

        void _SplitPane(const MTSM::SplitDirection splitType,
                        const float splitSize,
                        std::shared_ptr<Pane> newPane);
        void _SplitPane(TerminalTab& tab,
                        const MTSM::SplitDirection splitType,
                        const float splitSize,
                        std::shared_ptr<Pane> newPane);
        void _ResizePane(const MTSM::ResizeDirection& direction);
        void _ToggleSplitOrientation();

        void _ScrollPage(ScrollDirection scrollDirection);
        void _ScrollToBufferEdge(ScrollDirection scrollDirection);
        void _SetAcceleratorForMenuItem(WUXC::MenuFlyoutItem& menuItem, const MTControl::KeyChord& keyChord);

        winrt::fire_and_forget _CopyToClipboardHandler(const IInspectable sender, const MTControl::CopyToClipboardEventArgs copiedData);
        winrt::fire_and_forget _PasteFromClipboardHandler(const IInspectable sender,
                                                          const MTControl::PasteFromClipboardEventArgs eventArgs);

        void _OpenHyperlinkHandler(const IInspectable sender, const MTControl::OpenHyperlinkEventArgs eventArgs);
        bool _IsUriSupported(const WF::Uri& parsedUri);

        void _ShowCouldNotOpenDialog(winrt::hstring reason, winrt::hstring uri);
        bool _CopyText(const bool singleLine, const WF::IReference<MTControl::CopyFormat>& formats);

        winrt::fire_and_forget _SetTaskbarProgressHandler(const IInspectable sender, const IInspectable eventArgs);

        void _PasteText();

        winrt::fire_and_forget _ControlNoticeRaisedHandler(const IInspectable sender, const MTControl::NoticeEventArgs eventArgs);
        void _ShowControlNoticeDialog(const winrt::hstring& title, const winrt::hstring& message);

        fire_and_forget _LaunchSettings(const MTSM::SettingsTarget target);

        void _TabDragStarted(const IInspectable& sender, const IInspectable& eventArgs);
        void _TabDragCompleted(const IInspectable& sender, const IInspectable& eventArgs);

        void _OnTabClick(const IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& eventArgs);
        void _OnTabSelectionChanged(const IInspectable& sender, const WUXC::SelectionChangedEventArgs& eventArgs);
        void _OnTabItemsChanged(const IInspectable& sender, const WFC::IVectorChangedEventArgs& eventArgs);
        void _OnTabCloseRequested(const IInspectable& sender, const MUXC::TabViewTabCloseRequestedEventArgs& eventArgs);
        void _OnFirstLayout(const IInspectable& sender, const IInspectable& eventArgs);
        void _UpdatedSelectedTab(const MTApp::TabBase& tab);
        void _UpdateBackground(const MTSM::Profile& profile);

        void _OnDispatchCommandRequested(const IInspectable& sender, const MTSM::Command& command);
        void _OnCommandLineExecutionRequested(const IInspectable& sender, const winrt::hstring& commandLine);
        void _OnSwitchToTabRequested(const IInspectable& sender, const MTApp::TabBase& tab);

        void _Find(const TerminalTab& tab);

        MTControl::TermControl _InitControl(const MTSM::TerminalSettingsCreateResult& settings,
                                            const MTConnection::ITerminalConnection& connection);

        std::shared_ptr<Pane> _MakePane(const MTSM::NewTerminalArgs& newTerminalArgs = nullptr,
                                        const MTApp::TabBase& sourceTab = nullptr,
                                        MTConnection::ITerminalConnection existingConnection = nullptr);

        void _RefreshUIForSettingsReload();

        void _SetNewTabButtonColor(const Windows::UI::Color& color, const Windows::UI::Color& accentColor);
        void _ClearNewTabButtonColor();

        void _StartInboundListener();

        void _CompleteInitialization();

        void _FocusActiveControl(IInspectable sender, IInspectable eventArgs);

        void _UnZoomIfNeeded();

        static int _ComputeScrollDelta(ScrollDirection scrollDirection, const uint32_t rowsToScroll);
        static uint32_t _ReadSystemRowsToScroll();

        void _UpdateMRUTab(const MTApp::TabBase& tab);

        void _TryMoveTab(const uint32_t currentTabIndex, const int32_t suggestedNewTabIndex);

        bool _shouldMouseVanish{ false };
        bool _isMouseHidden{ false };
        WUC::CoreCursor _defaultPointerCursor{ nullptr };
        void _HidePointerCursorHandler(const IInspectable& sender, const IInspectable& eventArgs);
        void _RestorePointerCursorHandler(const IInspectable& sender, const IInspectable& eventArgs);

        void _PreviewAction(const MTSM::ActionAndArgs& args);
        void _PreviewActionHandler(const IInspectable& sender, const MTSM::Command& args);
        void _EndPreview();
        void _RunRestorePreviews();
        void _PreviewColorScheme(const MTSM::SetColorSchemeArgs& args);
        void _PreviewAdjustOpacity(const MTSM::AdjustOpacityArgs& args);
        MTSM::ActionAndArgs _lastPreviewedAction{ nullptr };
        std::vector<std::function<void()>> _restorePreviewFuncs{};

        HRESULT _OnNewConnection(const MTConnection::ConptyConnection& connection);
        void _HandleToggleInboundPty(const IInspectable& sender, const MTSM::ActionEventArgs& args);

        void _WindowRenamerActionClick(const IInspectable& sender, const IInspectable& eventArgs);
        void _RequestWindowRename(const winrt::hstring& newName);
        void _WindowRenamerKeyDown(const IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);
        void _WindowRenamerKeyUp(const IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);

        void _UpdateTeachingTipTheme(WUX::FrameworkElement element);

        MTSM::Profile GetClosestProfileForDuplicationOfProfile(const MTSM::Profile& profile) const noexcept;

        bool _maybeElevate(const MTSM::NewTerminalArgs& newTerminalArgs,
                           const MTSM::TerminalSettingsCreateResult& controlSettings,
                           const MTSM::Profile& profile);
        void _OpenElevatedWT(MTSM::NewTerminalArgs newTerminalArgs);

        winrt::fire_and_forget _ConnectionStateChangedHandler(const WF::IInspectable& sender, const WF::IInspectable& args) const;
        void _CloseOnExitInfoDismissHandler(const WF::IInspectable& sender, const WF::IInspectable& args) const;
        void _KeyboardServiceWarningInfoDismissHandler(const WF::IInspectable& sender, const WF::IInspectable& args) const;
        void _SetAsDefaultDismissHandler(const WF::IInspectable& sender, const WF::IInspectable& args);
        void _SetAsDefaultOpenSettingsHandler(const WF::IInspectable& sender, const WF::IInspectable& args);
        static bool _IsMessageDismissed(const MTSM::InfoBarMessage& message);
        static void _DismissMessage(const MTSM::InfoBarMessage& message);

        void _updateThemeColors();
        void _updateTabCloseButton(const MUXC::TabViewItem& tabViewItem);

        winrt::fire_and_forget _ShowWindowChangedHandler(const IInspectable sender, const MTControl::ShowWindowArgs args);

#pragma region ActionHandlers
        // These are all defined in AppActionHandlers.cpp
#define ON_ALL_ACTIONS(action) DECLARE_ACTION_HANDLER(action);
        ALL_SHORTCUT_ACTIONS
#undef ON_ALL_ACTIONS
#pragma endregion

        friend class TerminalAppLocalTests::TabTests;
        friend class TerminalAppLocalTests::SettingsTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(TerminalPage);
}
