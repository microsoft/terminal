// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "App.h"
#include <shellapi.h>
#include <filesystem>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>

using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Settings;
using namespace winrt::Microsoft::Terminal::TerminalControl;
using namespace ::TerminalApp;

// Note: Generate GUID using TlgGuid.exe tool
TRACELOGGING_DEFINE_PROVIDER(
    g_hTerminalAppProvider,
    "Microsoft.Windows.Terminal.App",
    // {24a1622f-7da7-5c77-3303-d850bd1ab2ed}
    (0x24a1622f, 0x7da7, 0x5c77, 0x33, 0x03, 0xd8, 0x50, 0xbd, 0x1a, 0xb2, 0xed),
    TraceLoggingOptionMicrosoftTelemetry());

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

namespace winrt::TerminalApp::implementation
{

    App::App() :
        App(winrt::TerminalApp::XamlMetaDataProvider())
    {
    }

    App::App(Windows::UI::Xaml::Markup::IXamlMetadataProvider const& parentProvider) :
        base_type(parentProvider),
        _settings{  },
        _tabs{  },
        _loadedInitialSettings{ false }
    {
        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like App just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is App not
        // registered?" when it definitely is.
    }

    // Method Description:
    // - Build the UI for the terminal app. Before this method is called, it
    //   should not be assumed that the TerminalApp is usable. The Settings
    //   should be loaded before this is called, either with LoadSettings or
    //   GetLaunchDimensions (which will call LoadSettings)
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void App::Create()
    {
        // Assert that we've already loaded our settings. We have to do
        // this as a MTA, before the app is Create()'d
        WINRT_ASSERT(_loadedInitialSettings);
        TraceLoggingRegister(g_hTerminalAppProvider);
        _Create();
    }

    App::~App()
    {
        TraceLoggingUnregister(g_hTerminalAppProvider);
    }

    // Method Description:
    // - Create all of the initial UI elements of the Terminal app.
    //    * Creates the tab bar, initially hidden.
    //    * Creates the tab content area, which is where we'll display the tabs/panes.
    //    * Initializes the first terminal control, using the default profile,
    //      and adds it to our list of tabs.
    void App::_Create()
    {
        _tabView = MUX::Controls::TabView{};

        _tabView.SelectionChanged({ this, &App::_OnTabSelectionChanged });
        _tabView.TabClosing({ this, &App::_OnTabClosing });
        _tabView.Items().VectorChanged({ this, &App::_OnTabItemsChanged });

        _root = Controls::Grid{};

        _tabRow = Controls::Grid{};
        _tabRow.Name(L"Tab Row");
        _tabContent = Controls::Grid{};
        _tabContent.Name(L"Tab Content");

        // Set up two columns in the tabs row - one for the tabs themselves, and
        // another for the settings button.
        auto tabsColDef = Controls::ColumnDefinition();
        auto newTabBtnColDef = Controls::ColumnDefinition();

        newTabBtnColDef.Width(GridLengthHelper::Auto());

        _tabRow.ColumnDefinitions().Append(tabsColDef);
        _tabRow.ColumnDefinitions().Append(newTabBtnColDef);

        // Set up two rows - one for the tabs, the other for the tab content,
        // the terminal panes.
        auto tabBarRowDef = Controls::RowDefinition();
        tabBarRowDef.Height(GridLengthHelper::Auto());
        _root.RowDefinitions().Append(tabBarRowDef);
        _root.RowDefinitions().Append(Controls::RowDefinition{});

        if (_settings->GlobalSettings().GetShowTabsInTitlebar() == false)
        {
            _root.Children().Append(_tabRow);
            Controls::Grid::SetRow(_tabRow, 0);
        }
        _root.Children().Append(_tabContent);
        Controls::Grid::SetRow(_tabContent, 1);
        Controls::Grid::SetColumn(_tabView, 0);

        // Create the new tab button.
        _newTabButton = Controls::SplitButton{};
        Controls::SymbolIcon newTabIco{};
        newTabIco.Symbol(Controls::Symbol::Add);
        _newTabButton.Content(newTabIco);
        Controls::Grid::SetRow(_newTabButton, 0);
        Controls::Grid::SetColumn(_newTabButton, 1);
        _newTabButton.VerticalAlignment(VerticalAlignment::Stretch);
        _newTabButton.HorizontalAlignment(HorizontalAlignment::Left);

        // When the new tab button is clicked, open the default profile
        _newTabButton.Click([this](auto&&, auto&&){
            this->_OpenNewTab(std::nullopt);
        });

        // Populate the new tab button's flyout with entries for each profile
        _CreateNewTabFlyout();

        _tabRow.Children().Append(_tabView);
        _tabRow.Children().Append(_newTabButton);

        _tabContent.VerticalAlignment(VerticalAlignment::Stretch);
        _tabContent.HorizontalAlignment(HorizontalAlignment::Stretch);

        // Here, we're doing the equivalent of defining the _tabRow as the
        // following: <Grid Background="{ThemeResource
        // ApplicationPageBackgroundThemeBrush}"> We need to set the Background
        // to that ThemeResource, so it'll be colored appropriately regardless
        // of what theme the user has selected.
        // We're looking up the Style we've defined in App.xaml, and applying it
        // here. A ResourceDictionary is a Map<IInspectable, IInspectable>, so
        // you'll need to try_as to get the type we actually want.
        auto res = Resources();
        IInspectable key = winrt::box_value(L"BackgroundGridThemeStyle");
        if (res.HasKey(key))
        {
            IInspectable g = res.Lookup(key);
            winrt::Windows::UI::Xaml::Style style = g.try_as<winrt::Windows::UI::Xaml::Style>();
            _tabRow.Style(style);
        }

        // Apply the UI theme from our settings to our UI elements
        _ApplyTheme(_settings->GlobalSettings().GetRequestedTheme());

        _OpenNewTab(std::nullopt);
    }

    // Method Description:
    // - Get the size in pixels of the client area we'll need to launch this
    //   terminal app. This method will use the default profile's settings to do
    //   this calculation, as well as the _system_ dpi scaling. See also
    //   TermControl::GetProposedDimensions.
    // Arguments:
    // - <none>
    // Return Value:
    // - a point containing the requested dimensions in pixels.
    winrt::Windows::Foundation::Point App::GetLaunchDimensions(uint32_t dpi)
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            LoadSettings();
        }

        // Use the default profile to determine how big of a window we need.
        TerminalSettings settings = _settings->MakeSettings(std::nullopt);

        // TODO MSFT:21150597 - If the global setting "Always show tab bar" is
        // set, then we'll need to add the height of the tab bar here.

        return TermControl::GetProposedDimensions(settings, dpi);
    }


    bool App::GetShowTabsInTitlebar()
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            LoadSettings();
        }

        return _settings->GlobalSettings().GetShowTabsInTitlebar();
    }

    // Method Description:
    // - Builds the flyout (dropdown) attached to the new tab button, and
    //   attaches it to the button. Populates the flyout with one entry per
    //   Profile, displaying the profile's name. Clicking each flyout item will
    //   open a new tab with that profile.
    //   Below the profiles are the static menu items: settings, feedback
    void App::_CreateNewTabFlyout()
    {
        auto newTabFlyout = Controls::MenuFlyout{};
        for (int profileIndex = 0; profileIndex < _settings->GetProfiles().size(); profileIndex++)
        {
            const auto& profile = _settings->GetProfiles()[profileIndex];
            auto profileMenuItem = Controls::MenuFlyoutItem{};

            auto profileName = profile.GetName();
            winrt::hstring hName{ profileName };
            profileMenuItem.Text(hName);

            // If there's an icon set for this profile, set it as the icon for
            // this flyout item.
            if (profile.HasIcon())
            {
                profileMenuItem.Icon(_GetIconFromProfile(profile));
            }

            profileMenuItem.Click([this, profileIndex](auto&&, auto&&){
                this->_OpenNewTab({ profileIndex });
            });
            newTabFlyout.Items().Append(profileMenuItem);
        }

        // add menu separator
        auto separatorItem = Controls::MenuFlyoutSeparator{};
        newTabFlyout.Items().Append(separatorItem);

        // add static items
        {
            // Create the settings button.
            auto settingsItem = Controls::MenuFlyoutItem{};
            settingsItem.Text(L"Settings");

            Controls::SymbolIcon ico{};
            ico.Symbol(Controls::Symbol::Setting);
            settingsItem.Icon(ico);

            settingsItem.Click({ this, &App::_SettingsButtonOnClick });
            newTabFlyout.Items().Append(settingsItem);

            // Create the feedback button.
            auto feedbackFlyout = Controls::MenuFlyoutItem{};
            feedbackFlyout.Text(L"Feedback");

            Controls::FontIcon feedbackIco{};
            feedbackIco.Glyph(L"\xE939");
            feedbackIco.FontFamily(Media::FontFamily{ L"Segoe MDL2 Assets" });
            feedbackFlyout.Icon(feedbackIco);

            feedbackFlyout.Click({ this, &App::_FeedbackButtonOnClick });
            newTabFlyout.Items().Append(feedbackFlyout);
        }

        _newTabButton.Flyout(newTabFlyout);
    }

    // Function Description:
    // - Called when the settings button is clicked. ShellExecutes the settings
    //   file, as to open it in the default editor for .json files. Does this in
    //   a background thread, as to not hang/crash the UI thread.
    fire_and_forget LaunchSettings()
    {
        // This will switch the execution of the function to a background (not
        // UI) thread. This is IMPORTANT, because the Windows.Storage API's
        // (used for retrieving the path to the file) will crash on the UI
        // thread, because the main thread is a STA.
        co_await winrt::resume_background();

        const auto settingsPath = CascadiaSettings::GetSettingsPath();
        ShellExecute(nullptr, L"open", settingsPath.c_str(), nullptr, nullptr, SW_SHOW);
    }

    // Method Description:
    // - Called when the settings button is clicked. Launches a background
    //   thread to open the settings file in the default JSON editor.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void App::_SettingsButtonOnClick(const IInspectable&,
                                         const RoutedEventArgs&)
    {
        LaunchSettings();
    }

    // Method Description:
    // - Called when the feedback button is clicked. Launches the feedback hub
    //   to the list of all feedback for the Terminal app.
    void App::_FeedbackButtonOnClick(const IInspectable&,
                                     const RoutedEventArgs&)
    {
        // If you want this to go to the new feedback page automatically, use &newFeedback=true
        winrt::Windows::System::Launcher::LaunchUriAsync({ L"feedback-hub://?tabid=2&appid=Microsoft.WindowsTerminal_8wekyb3d8bbwe!App" });

    }

    // Method Description:
    // - Register our event handlers with the given keybindings object. This
    //   should be done regardless of what the events are actually bound to -
    //   this simply ensures the AppKeyBindings object will call us correctly
    //   for each event.
    // Arguments:
    // - bindings: A AppKeyBindings object to wire up with our event handlers
    void App::_HookupKeyBindings(TerminalApp::AppKeyBindings bindings) noexcept
    {
        // Hook up the KeyBinding object's events to our handlers.
        // They should all be hooked up here, regardless of whether or not
        //      there's an actual keychord for them.
        bindings.NewTab([this]() { _OpenNewTab(std::nullopt); });
        bindings.CloseTab([this]() { _CloseFocusedTab(); });
        bindings.NewTabWithProfile([this](const auto index) { _OpenNewTab({ index }); });
        bindings.ScrollUp([this]() { _DoScroll(-1); });
        bindings.ScrollDown([this]() { _DoScroll(1); });
        bindings.NextTab([this]() { _SelectNextTab(true); });
        bindings.PrevTab([this]() { _SelectNextTab(false); });
    }

    // Method Description:
    // - Initialized our settings. See CascadiaSettings for more details.
    //      Additionally hooks up our callbacks for keybinding events to the
    //      keybindings object.
    // NOTE: This must be called from a MTA if we're running as a packaged
    //      application. The Windows.Storage APIs require a MTA. If this isn't
    //      happening during startup, it'll need to happen on a background thread.
    void App::LoadSettings()
    {
        _settings = CascadiaSettings::LoadAll();

        _HookupKeyBindings(_settings->GetKeybindings());

        _loadedInitialSettings = true;

        // Register for directory change notification.
        _RegisterSettingsChange();
    }

    // Method Description:
    // - Registers for changes to the settings folder and upon a updated settings
    //      profile calls _ReloadSettings().
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void App::_RegisterSettingsChange()
    {
        // Make sure this hstring has a stack-local reference. If we don't it
        // might get cleaned up before we parse the path.
        const auto localPathCopy = CascadiaSettings::GetSettingsPath();

        // Getting the containing folder.
        std::filesystem::path fileParser = localPathCopy.c_str();
        const auto folder = fileParser.parent_path();

        _reader.create(folder.c_str(), false, wil::FolderChangeEvents::All,
            [this](wil::FolderChangeEvent event, PCWSTR fileModified)
        {
            // We want file modifications, AND when files are renamed to be
            // profiles.json. This second case will ofentimes happen with text
            // editors, who will write a temp file, then rename it to be the
            // actual file you wrote. So listen for that too.
            if (!(event == wil::FolderChangeEvent::Modified ||
                  event == wil::FolderChangeEvent::RenameNewName))
            {
                return;
            }

            const auto localPathCopy = CascadiaSettings::GetSettingsPath();
            std::filesystem::path settingsParser = localPathCopy.c_str();
            std::filesystem::path modifiedParser = fileModified;

            // Getting basename (filename.ext)
            const auto settingsBasename = settingsParser.filename();
            const auto modifiedBasename = modifiedParser.filename();

            if (settingsBasename == modifiedBasename)
            {
                this->_ReloadSettings();
            }
        });
    }

    // Method Description:
    // - Reloads the settings from the profile.json.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void App::_ReloadSettings()
    {
        _settings = CascadiaSettings::LoadAll();
        // Re-wire the keybindings to their handlers, as we'll have created a
        // new AppKeyBindings object.
        _HookupKeyBindings(_settings->GetKeybindings());

        auto profiles = _settings->GetProfiles();

        for (auto &profile : profiles)
        {
            const GUID profileGuid = profile.GetGuid();
            TerminalSettings settings = _settings->MakeSettings(profileGuid);

            for (auto &tab : _tabs)
            {
                const auto term = tab->GetTerminalControl();
                const GUID tabProfile = tab->GetProfile();

                if (profileGuid == tabProfile)
                {
                    term.UpdateSettings(settings);

                    // Update the icons of the tabs with this profile open.
                    auto tabViewItem = tab->GetTabViewItem();
                    tabViewItem.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [profile, tabViewItem]() {
                        // _GetIconFromProfile has to run on the main thread
                        tabViewItem.Icon(App::_GetIconFromProfile(profile));
                    });
                }
            }
        }


        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
            // Refresh the UI theme
            _ApplyTheme(_settings->GlobalSettings().GetRequestedTheme());

            // repopulate the new tab button's flyout with entries for each
            // profile, which might have changed
            _CreateNewTabFlyout();
        });

    }

    // Method Description:
    // - Update the current theme of the application. This will manually update
    //   all of the elements in our UI to match the given theme.
    // Arguments:
    // - newTheme: The ElementTheme to apply to our elements.
    void App::_ApplyTheme(const Windows::UI::Xaml::ElementTheme& newTheme)
    {
        _root.RequestedTheme(newTheme);
        _tabRow.RequestedTheme(newTheme);
    }

    UIElement App::GetRoot() noexcept
    {
        return _root;
    }

    UIElement App::GetTabs() noexcept
    {
        return _tabRow;
    }

    void App::_SetFocusedTabIndex(int tabIndex)
    {
        auto tab = _tabs.at(tabIndex);
        _tabView.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [tab, this](){
            auto tabViewItem = tab->GetTabViewItem();
            _tabView.SelectedItem(tabViewItem);
        });
    }

    // Method Description:
    // - Handle changes in tab layout.
    void App::_UpdateTabView()
    {
        // Show tabs when there's more than 1, or the user has chosen to always
        // show the tab bar.
        const bool isVisible = _settings->GlobalSettings().GetShowTabsInTitlebar() ||
                               (_tabs.size() > 1) ||
                               _settings->GlobalSettings().GetAlwaysShowTabs();

        // collapse/show the tabs themselves
        _tabView.Visibility(isVisible ? Visibility::Visible : Visibility::Collapsed);

        // collapse/show the row that the tabs are in.
        // NaN is the special value XAML uses for "Auto" sizing.
        _tabRow.Height(isVisible ? NAN : 0);
    }

    // Method Description:
    // - Open a new tab. This will create the TerminalControl hosting the
    //      terminal, and add a new Tab to our list of tabs. The method can
    //      optionally be provided a profile index, which will be used to create
    //      a tab using the profile in that index.
    //      If no index is provided, the default profile will be used.
    // Arguments:
    // - profileIndex: an optional index into the list of profiles to use to
    //      initialize this tab up with.
    void App::_OpenNewTab(std::optional<int> profileIndex)
    {
        GUID profileGuid;

        if (profileIndex)
        {
            const auto realIndex = profileIndex.value();
            const auto profiles = _settings->GetProfiles();

            // If we don't have that many profiles, then do nothing.
            if (realIndex >= profiles.size())
            {
                return;
            }

            const auto& selectedProfile = profiles[realIndex];
            profileGuid = selectedProfile.GetGuid();
        }
        else
        {
            // Getting Guid for default profile
            const auto globalSettings = _settings->GlobalSettings();
            profileGuid = globalSettings.GetDefaultProfile();
        }

        TerminalSettings settings = _settings->MakeSettings(profileGuid);
        _CreateNewTabFromSettings(profileGuid, settings);

        const int tabCount = static_cast<int>(_tabs.size());
        TraceLoggingWrite(
            g_hTerminalAppProvider, // handle to TerminalApp tracelogging provider
            "TabInformation",
            TraceLoggingDescription("Event emitted upon new tab creation in TerminalApp"),
            TraceLoggingInt32(tabCount, "TabCount", "Count of tabs curently opened in TerminalApp"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServicePerformance));
    }

    // Function Description:
    // - Copies and processes the text data from the Windows Clipboard.
    //   Does some of this in a background thread, as to not hang/crash the UI thread.
    // Arguments:
    // - eventArgs: the PasteFromClipboard event sent from the TermControl
    fire_and_forget PasteFromClipboard(PasteFromClipboardEventArgs eventArgs)
    {
        const DataPackageView data = Clipboard::GetContent();

        // This will switch the execution of the function to a background (not
        // UI) thread. This is IMPORTANT, because the getting the clipboard data
        // will crash on the UI thread, because the main thread is a STA.
        co_await winrt::resume_background();

        hstring text = L"";
        if (data.Contains(StandardDataFormats::Text()))
        {
            text = co_await data.GetTextAsync();
        }
        eventArgs.HandleClipboardData(text);
    }

    // Method Description:
    // - Creates a new tab with the given settings. If the tab bar is not being
    //      currently displayed, it will be shown.
    // Arguments:
    // - settings: the TerminalSettings object to use to create the TerminalControl with.
    void App::_CreateNewTabFromSettings(GUID profileGuid, TerminalSettings settings)
    {
        // Initialize the new tab
        TermControl term{ settings };

        // Add an event handler when the terminal's selection wants to be copied.
        // When the text buffer data is retrieved, we'll copy the data into the Clipboard
        term.CopyToClipboard([=](auto copiedData) {
            _root.Dispatcher().RunAsync(CoreDispatcherPriority::High, [copiedData]() {
                DataPackage dataPack = DataPackage();
                dataPack.RequestedOperation(DataPackageOperation::Copy);
                dataPack.SetText(copiedData);
                Clipboard::SetContent(dataPack);

                // TODO: MSFT 20642290 and 20642291
                // rtf copy and html copy
            });
        });

        // Add an event handler when the terminal wants to paste data from the Clipboard.
        term.PasteFromClipboard([=](auto /*sender*/, auto eventArgs) {
            _root.Dispatcher().RunAsync(CoreDispatcherPriority::High, [eventArgs]() {
                PasteFromClipboard(eventArgs);
            });
        });

        // Add the new tab to the list of our tabs.
        auto newTab = _tabs.emplace_back(std::make_shared<Tab>(profileGuid, term));

        // Add an event handler when the terminal's title changes. When the
        // title changes, we'll bubble it up to listeners of our own title
        // changed event, so they can handle it.
        newTab->GetTerminalControl().TitleChanged([=](auto newTitle){
            // Only bubble the change if this tab is the focused tab.
            if (_settings->GlobalSettings().GetShowTitleInTitlebar() &&
                newTab->IsFocused())
            {
                _titleChangeHandlers(newTitle);
            }
        });

        auto tabViewItem = newTab->GetTabViewItem();
        _tabView.Items().Append(tabViewItem);

        const auto* const profile = _settings->FindProfile(profileGuid);

        // Set this profile's tab to the icon the user specified
        if (profile != nullptr && profile->HasIcon())
        {
            tabViewItem.Icon(_GetIconFromProfile(*profile));
        }

        // Add an event handler when the terminal's connection is closed.
        newTab->GetTerminalControl().ConnectionClosed([=]() {
            _tabView.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [newTab, tabViewItem, this]() {
                const GUID tabProfile = newTab->GetProfile();
                // Don't just capture this pointer, because the profile might
                // get destroyed before this is called (case in point -
                // reloading settings)
                const auto* const p = _settings->FindProfile(tabProfile);

                // TODO: MSFT:21268795: Need a better story for what should happen when the last tab is closed.
                if (p != nullptr && p->GetCloseOnExit() && _tabs.size() > 1)
                {
                    _RemoveTabViewItem(tabViewItem);
                }
            });
        });

        tabViewItem.PointerPressed({ this, &App::_OnTabClick });

        // This is one way to set the tab's selected background color.
        //   tabViewItem.Resources().Insert(winrt::box_value(L"TabViewItemHeaderBackgroundSelected"), a Brush?);

        // This kicks off TabView::SelectionChanged, in response to which we'll attach the terminal's
        // Xaml control to the Xaml root.
        _tabView.SelectedItem(tabViewItem);
    }

    // Method Description:
    // - Returns the index in our list of tabs of the currently focused tab. If
    //      no tab is currently selected, returns -1.
    // Return Value:
    // - the index of the currently focused tab if there is one, else -1
    int App::_GetFocusedTabIndex() const
    {
        return _tabView.SelectedIndex();
    }

    // Method Description:
    // - Close the currently focused tab. Focus will move to the left, if possible.
    void App::_CloseFocusedTab()
    {
        if (_tabs.size() > 1)
        {
            int focusedTabIndex = _GetFocusedTabIndex();
            std::shared_ptr<Tab> focusedTab{ _tabs[focusedTabIndex] };

            // We're not calling _FocusTab here because it makes an async dispatch
            // that is practically guaranteed to not happen before we delete the tab.
            _tabView.SelectedIndex((focusedTabIndex > 0) ? focusedTabIndex - 1 : 1);
            _tabView.Items().RemoveAt(focusedTabIndex);
            _tabs.erase(_tabs.begin() + focusedTabIndex);
        }
    }

    // Method Description:
    // - Move the viewport of the terminal of the currently focused tab up or
    //      down a number of lines. Negative values of `delta` will move the
    //      view up, and positive values will move the viewport down.
    // Arguments:
    // - delta: a number of lines to move the viewport relative to the current viewport.
    void App::_DoScroll(int delta)
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        _tabs[focusedTabIndex]->Scroll(delta);
    }

    // Method Description:
    // - Copy text from the focused terminal to the Windows Clipboard
    // Arguments:
    // - trimTrailingWhitespace: enable removing any whitespace from copied selection
    //    and get text to appear on separate lines.
    void App::_CopyText(const bool trimTrailingWhitespace)
    {
        const int focusedTabIndex = _GetFocusedTabIndex();
        std::shared_ptr<Tab> focusedTab{ _tabs[focusedTabIndex] };

        const auto control = focusedTab->GetTerminalControl();
        control.CopySelectionToClipboard(trimTrailingWhitespace);
    }

    // Method Description:
    // - Sets focus to the tab to the right or left the currently selected tab.
    void App::_SelectNextTab(const bool bMoveRight)
    {
        int focusedTabIndex = _GetFocusedTabIndex();
        auto tabCount = _tabs.size();
        // Wraparound math. By adding tabCount and then calculating modulo tabCount,
        // we clamp the values to the range [0, tabCount) while still supporting moving
        // leftward from 0 to tabCount - 1.
        _SetFocusedTabIndex(
            static_cast<int>((tabCount + focusedTabIndex + (bMoveRight ? 1 : -1)) % tabCount)
        );
    }

    // Method Description:
    // - Responds to the TabView control's Selection Changed event (to move a
    //      new terminal control into focus.)
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void App::_OnTabSelectionChanged(const IInspectable& sender, const Controls::SelectionChangedEventArgs& eventArgs)
    {
        auto tabView = sender.as<MUX::Controls::TabView>();
        auto selectedIndex = tabView.SelectedIndex();

        // Unfocus all the tabs.
        for (auto tab : _tabs)
        {
            tab->SetFocused(false);
        }

        if (selectedIndex >= 0)
        {
            try
            {
                auto tab = _tabs.at(selectedIndex);
                auto control = tab->GetTerminalControl().GetControl();

                _tabContent.Children().Clear();
                _tabContent.Children().Append(control);

                tab->SetFocused(true);
                _titleChangeHandlers(GetTitle());
            }
            CATCH_LOG();
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
    void App::_OnTabClosing(const IInspectable& sender, const MUX::Controls::TabViewTabClosingEventArgs& eventArgs)
    {
        // Don't allow the user to close the last tab ..
        // .. yet.
        if (_tabs.size() > 1)
        {
            const auto tabViewItem = eventArgs.Item();
            _RemoveTabViewItem(tabViewItem);
        }
        // If we don't cancel the event, the TabView will remove the item itself.
        eventArgs.Cancel(true);
    }

    // Method Description:
    // - Responds to changes in the TabView's item list by changing the tabview's
    //      visibility.
    // Arguments:
    // - sender: the control that originated this event
    // - eventArgs: the event's constituent arguments
    void App::_OnTabItemsChanged(const IInspectable& sender, const Windows::Foundation::Collections::IVectorChangedEventArgs& eventArgs)
    {
        _UpdateTabView();
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Windows Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Windows Terminal"
    hstring App::GetTitle()
    {
        if (_settings->GlobalSettings().GetShowTitleInTitlebar())
        {
            auto selectedIndex = _tabView.SelectedIndex();
            if (selectedIndex >= 0)
            {
                try
                {
                    auto tab = _tabs.at(selectedIndex);
                    return tab->GetTerminalControl().Title();
                }
                CATCH_LOG();
            }
        }
        return { L"Windows Terminal" };
    }

    // Method Description:
    // - Additional responses to clicking on a TabView's item. Currently, just remove tab with middle click
    // Arguments:
    // - sender: the control that originated this event (TabViewItem)
    // - eventArgs: the event's constituent arguments
    void App::_OnTabClick(const IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& eventArgs)
    {
        if (eventArgs.GetCurrentPoint(_root).Properties().IsMiddleButtonPressed())
        {
            _RemoveTabViewItem(sender);
            eventArgs.Handled(true);
        }
    }

    // Method Description:
    // - Removes the tab (both TerminalControl and XAML)
    // Arguments:
    // - tabViewItem: the TabViewItem in the TabView that is being removed.
    void App::_RemoveTabViewItem(const IInspectable& tabViewItem)
    {
        uint32_t tabIndexFromControl = 0;
        _tabView.Items().IndexOf(tabViewItem, tabIndexFromControl);

        if (tabIndexFromControl == _GetFocusedTabIndex())
        {
            _tabView.SelectedIndex((tabIndexFromControl > 0) ? tabIndexFromControl - 1 : 1);
        }

        // Removing the tab from the collection will destroy its control and disconnect its connection.
        _tabs.erase(_tabs.begin() + tabIndexFromControl);
        _tabView.Items().RemoveAt(tabIndexFromControl);
    }

    // Method Description:
    // - Gets a colored IconElement for the profile in question. If the profile
    //   has an `icon` set in the settings, this will return an icon with that
    //   image in it. Otherwise it will return a nullptr-initialized
    //   IconElement.
    // Arguments:
    // - profile: the profile to get the icon from
    // Return Value:
    // - an IconElement for the profile's icon, if it has one.
    Controls::IconElement App::_GetIconFromProfile(const Profile& profile)
    {
        if (profile.HasIcon())
        {
            auto path = profile.GetIconPath();
            winrt::hstring iconPath{ path };
            winrt::Windows::Foundation::Uri iconUri{ iconPath };
            Controls::BitmapIconSource iconSource;
            // Make sure to set this to false, so we keep the RGB data of the
            // image. Otherwise, the icon will be white for all the
            // non-transparent pixels in the image.
            iconSource.ShowAsMonochrome(false);
            iconSource.UriSource(iconUri);
            Controls::IconSourceElement elem;
            elem.IconSource(iconSource);
            return elem;
        }
        else
        {
            return { nullptr };
        }
    }

    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT(App, TitleChanged, _titleChangeHandlers, TerminalControl::TitleChangedEventArgs);
}
