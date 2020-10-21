// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TerminalPage.g.h"
#include "Tab.h"
#include "AppKeyBindings.h"
#include "TerminalSettings.h"

#include <winrt/Microsoft.Terminal.TerminalControl.h>

#include "AppCommandlineArgs.h"

// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class TabTests;
    class SettingsTests;
};

namespace winrt::TerminalApp::implementation
{
    enum StartupState : int
    {
        NotInitialized = 0,
        InStartup = 1,
        Initialized = 2
    };

    struct TerminalPage : TerminalPageT<TerminalPage>
    {
    public:
        TerminalPage();

        winrt::fire_and_forget SetSettings(Microsoft::Terminal::Settings::Model::CascadiaSettings settings, bool needRefreshUI);

        void Create();

        hstring Title();

        void TitlebarClicked();

        float CalcSnappedDimension(const bool widthOrHeight, const float dimension) const;

        winrt::hstring ApplicationDisplayName();
        winrt::hstring ApplicationVersion();

        winrt::hstring ThirdPartyNoticesLink();

        void CloseWindow();

        void ToggleFocusMode();
        void ToggleFullscreen();
        void ToggleAlwaysOnTop();
        bool FocusMode() const;
        bool Fullscreen() const;
        bool AlwaysOnTop() const;

        void SetStartupActions(std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs>& actions);
        static std::vector<Microsoft::Terminal::Settings::Model::ActionAndArgs> ConvertExecuteCommandlineToActions(const Microsoft::Terminal::Settings::Model::ExecuteCommandlineArgs& args);

        winrt::TerminalApp::IDialogPresenter DialogPresenter() const;
        void DialogPresenter(winrt::TerminalApp::IDialogPresenter dialogPresenter);

        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(TitleChanged, _titleChangeHandlers, winrt::Windows::Foundation::IInspectable, winrt::hstring);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(LastTabClosed, _lastTabClosedHandlers, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(SetTitleBarContent, _setTitleBarContentHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::UIElement);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(FocusModeChanged, _focusModeChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(FullscreenChanged, _fullscreenChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(AlwaysOnTopChanged, _alwaysOnTopChangedHandlers, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable);
        TYPED_EVENT(Initialized, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::RoutedEventArgs);

    private:
        friend struct TerminalPageT<TerminalPage>; // for Xaml to bind events

        // If you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the app.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in App::_ApplyTheme. The roots currently is _tabRow
        // (which is a root when the tabs are in the titlebar.)
        Microsoft::UI::Xaml::Controls::TabView _tabView{ nullptr };
        TerminalApp::TabRowControl _tabRow{ nullptr };
        Windows::UI::Xaml::Controls::Grid _tabContent{ nullptr };
        Microsoft::UI::Xaml::Controls::SplitButton _newTabButton{ nullptr };

        Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };

        Windows::Foundation::Collections::IObservableVector<TerminalApp::Tab> _tabs;
        winrt::com_ptr<Tab> _GetStrongTabImpl(const uint32_t index) const;
        winrt::com_ptr<Tab> _GetStrongTabImpl(const ::winrt::TerminalApp::Tab& tab) const;
        void _UpdateTabIndices();

        bool _isInFocusMode{ false };
        bool _isFullscreen{ false };
        bool _isAlwaysOnTop{ false };

        bool _rearranging;
        std::optional<int> _rearrangeFrom;
        std::optional<int> _rearrangeTo;

        // use a weak reference to prevent circular dependency with AppLogic
        winrt::weak_ref<winrt::TerminalApp::IDialogPresenter> _dialogPresenter;

        winrt::com_ptr<AppKeyBindings> _bindings{ winrt::make_self<implementation::AppKeyBindings>() };
        winrt::com_ptr<ShortcutActionDispatch> _actionDispatch{ winrt::make_self<implementation::ShortcutActionDispatch>() };

        winrt::Windows::UI::Xaml::Controls::Grid::LayoutUpdated_revoker _layoutUpdatedRevoker;
        StartupState _startupState{ StartupState::NotInitialized };

        Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::ActionAndArgs> _startupActions;
        winrt::fire_and_forget _ProcessStartupActions(Windows::Foundation::Collections::IVector<Microsoft::Terminal::Settings::Model::ActionAndArgs> actions, const bool initial);

        void _ShowAboutDialog();
        void _ShowCloseWarningDialog();
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Xaml::Controls::ContentDialogResult> _ShowMultiLinePasteWarningDialog();
        winrt::Windows::Foundation::IAsyncOperation<winrt::Windows::UI::Xaml::Controls::ContentDialogResult> _ShowLargePasteWarningDialog();

        void _CreateNewTabFlyout();
        void _OpenNewTabDropdown();
        void _OpenNewTab(const Microsoft::Terminal::Settings::Model::NewTerminalArgs& newTerminalArgs);
        void _CreateNewTabFromSettings(GUID profileGuid, TerminalApp::TerminalSettings settings);
        winrt::Microsoft::Terminal::TerminalConnection::ITerminalConnection _CreateConnectionFromSettings(GUID profileGuid, TerminalApp::TerminalSettings settings);

        void _SettingsButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _FeedbackButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _AboutButtonOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);
        void _CloseWarningPrimaryButtonOnClick(Windows::UI::Xaml::Controls::ContentDialog sender, Windows::UI::Xaml::Controls::ContentDialogButtonClickEventArgs eventArgs);
        void _ThirdPartyNoticesOnClick(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        void _HookupKeyBindings(const Microsoft::Terminal::Settings::Model::KeyMapping& keymap) noexcept;
        void _RegisterActionCallbacks();

        void _UpdateTitle(const Tab& tab);
        void _UpdateTabIcon(Tab& tab);
        void _UpdateTabView();
        void _UpdateTabWidthMode();
        void _UpdateCommandsForPalette();
        static winrt::Windows::Foundation::Collections::IMap<winrt::hstring, Microsoft::Terminal::Settings::Model::Command> _ExpandCommands(Windows::Foundation::Collections::IMapView<winrt::hstring, Microsoft::Terminal::Settings::Model::Command> commandsToExpand,
                                                                                                                                            Windows::Foundation::Collections::IVectorView<Microsoft::Terminal::Settings::Model::Profile> profiles,
                                                                                                                                            Windows::Foundation::Collections::IMapView<winrt::hstring, Microsoft::Terminal::Settings::Model::ColorScheme> schemes);

        void _DuplicateTabViewItem();
        void _RemoveTabViewItem(const Microsoft::UI::Xaml::Controls::TabViewItem& tabViewItem);
        void _RemoveTabViewItemByIndex(uint32_t tabIndex);

        void _RegisterTerminalEvents(Microsoft::Terminal::TerminalControl::TermControl term, Tab& hostingTab);

        void _SelectNextTab(const bool bMoveRight);
        bool _SelectTab(const uint32_t tabIndex);
        void _MoveFocus(const Microsoft::Terminal::Settings::Model::Direction& direction);

        winrt::Microsoft::Terminal::TerminalControl::TermControl _GetActiveControl();
        std::optional<uint32_t> _GetFocusedTabIndex() const noexcept;
        winrt::com_ptr<Tab> _GetFocusedTab();
        winrt::fire_and_forget _SetFocusedTabIndex(const uint32_t tabIndex);
        void _CloseFocusedTab();
        void _CloseFocusedPane();
        void _CloseAllTabs();

        winrt::fire_and_forget _RemoveOnCloseRoutine(Microsoft::UI::Xaml::Controls::TabViewItem tabViewItem, winrt::com_ptr<TerminalPage> page);

        // Todo: add more event implementations here
        // MSFT:20641986: Add keybindings for New Window
        void _Scroll(int delta);
        void _SplitPane(const Microsoft::Terminal::Settings::Model::SplitState splitType, const Microsoft::Terminal::Settings::Model::SplitType splitMode = Microsoft::Terminal::Settings::Model::SplitType::Manual, const Microsoft::Terminal::Settings::Model::NewTerminalArgs& newTerminalArgs = nullptr);
        void _ResizePane(const Microsoft::Terminal::Settings::Model::Direction& direction);
        void _ScrollPage(int delta);
        void _SetAcceleratorForMenuItem(Windows::UI::Xaml::Controls::MenuFlyoutItem& menuItem, const winrt::Microsoft::Terminal::TerminalControl::KeyChord& keyChord);

        winrt::fire_and_forget _CopyToClipboardHandler(const IInspectable sender, const winrt::Microsoft::Terminal::TerminalControl::CopyToClipboardEventArgs copiedData);
        winrt::fire_and_forget _PasteFromClipboardHandler(const IInspectable sender,
                                                          const Microsoft::Terminal::TerminalControl::PasteFromClipboardEventArgs eventArgs);

        void _OpenHyperlinkHandler(const IInspectable sender, const Microsoft::Terminal::TerminalControl::OpenHyperlinkEventArgs eventArgs);
        void _ShowCouldNotOpenDialog(winrt::hstring reason, winrt::hstring uri);
        bool _CopyText(const bool singleLine, const Windows::Foundation::IReference<Microsoft::Terminal::TerminalControl::CopyFormat>& formats);

        void _PasteText();

        fire_and_forget _LaunchSettings(const Microsoft::Terminal::Settings::Model::SettingsTarget target);

        void _OnTabClick(const IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs);
        void _OnTabSelectionChanged(const IInspectable& sender, const Windows::UI::Xaml::Controls::SelectionChangedEventArgs& eventArgs);
        void _OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs);
        void _OnContentSizeChanged(const IInspectable& /*sender*/, Windows::UI::Xaml::SizeChangedEventArgs const& e);
        void _OnTabCloseRequested(const IInspectable& sender, const Microsoft::UI::Xaml::Controls::TabViewTabCloseRequestedEventArgs& eventArgs);
        void _OnFirstLayout(const IInspectable& sender, const IInspectable& eventArgs);
        void _UpdatedSelectedTab(const int32_t index);

        void _Find();

        winrt::fire_and_forget _RefreshUIForSettingsReload();

        void _SetNonClientAreaColors(const Windows::UI::Color& selectedTabColor);
        void _ClearNonClientAreaColors();
        void _SetNewTabButtonColor(const Windows::UI::Color& color, const Windows::UI::Color& accentColor);
        void _ClearNewTabButtonColor();

        void _CompleteInitialization();

        void _CommandPaletteClosed(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        void _UnZoomIfNeeded();

#pragma region ActionHandlers
        // These are all defined in AppActionHandlers.cpp
        void _HandleOpenNewTabDropdown(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleDuplicateTab(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleCloseTab(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleClosePane(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleScrollUp(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleScrollDown(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleNextTab(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandlePrevTab(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleSendInput(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleSplitPane(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleTogglePaneZoom(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleScrollUpPage(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleScrollDownPage(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleOpenSettings(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandlePasteText(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleNewTab(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleSwitchToTab(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleResizePane(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleMoveFocus(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleCopyText(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleCloseWindow(const IInspectable&, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleAdjustFontSize(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleFind(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleResetFontSize(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleToggleRetroEffect(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleToggleFocusMode(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleToggleFullscreen(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleToggleAlwaysOnTop(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleSetColorScheme(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleSetTabColor(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleOpenTabColorPicker(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleRenameTab(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleExecuteCommandline(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleToggleCommandPalette(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleCloseOtherTabs(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleCloseTabsAfter(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        void _HandleOpenTabSearch(const IInspectable& sender, const Microsoft::Terminal::Settings::Model::ActionEventArgs& args);
        // Make sure to hook new actions up in _RegisterActionCallbacks!
#pragma endregion

        friend class TerminalAppLocalTests::TabTests;
        friend class TerminalAppLocalTests::SettingsTests;
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    struct TerminalPage : TerminalPageT<TerminalPage, implementation::TerminalPage>
    {
    };
}
