// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppLogic.h"
#include "AppLogic.g.cpp"
#include "SettingsLoadEventArgs.h"

#include <LibraryResources.h>
#include <WtExeUtils.h>
#include <wil/token_helpers.h>

#include "../../types/inc/utils.hpp"

#include <winrt/Microsoft.CommandPalette.Extensions.h>
#include <wil/cppwinrt_authoring.h>

#include "fzf/fzf.h"

using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::DataTransfer;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal;
using namespace winrt::Microsoft::Terminal::Control;
using namespace winrt::Microsoft::Terminal::Settings::Model;
using namespace ::TerminalApp;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace MCP = Microsoft::CommandPalette::Extensions;
    using IInspectable = Windows::Foundation::IInspectable;
}

static winrt::com_ptr<winrt::TerminalApp::implementation::TerminalWindow> s_lastWindow;

struct StrIconData : winrt::implements<StrIconData, winrt::MCP::IIconData>
{
    StrIconData(winrt::hstring h) :
        _h{ h } {};
    winrt::hstring _h;
    winrt::hstring Icon() { return _h; }
    winrt::Windows::Storage::Streams::IRandomAccessStreamReference Data() { return nullptr; }
};

struct StrIconInfo : winrt::implements<StrIconInfo, winrt::MCP::IIconInfo>
{
    winrt::MCP::IIconData _i;
    StrIconInfo(winrt::hstring h) :
        _i{ winrt::make<StrIconData>(h) } {}
    winrt::MCP::IIconData Light() { return _i; }
    winrt::MCP::IIconData Dark() { return _i; }
};

struct CommandPaletteListItem : winrt::implements<CommandPaletteListItem, winrt::MCP::IListItem, winrt::MCP::ICommandItem, winrt::MCP::INotifyPropChanged>
{
    struct CommandPaletteInvokableCommand : winrt::implements<CommandPaletteInvokableCommand, winrt::MCP::IInvokableCommand, winrt::MCP::ICommand, winrt::MCP::INotifyPropChanged>
    {
        struct HideCommandResult : winrt::implements<HideCommandResult, winrt::MCP::ICommandResult>
        {
            winrt::MCP::CommandResultKind Kind() { return winrt::MCP::CommandResultKind::Hide; }
            winrt::MCP::ICommandResultArgs Args() { return nullptr; }
        };

        winrt::Microsoft::Terminal::Settings::Model::Command _c{ nullptr };
        CommandPaletteInvokableCommand(winrt::Microsoft::Terminal::Settings::Model::Command const& command) :
            _c{ command } {}
        ~CommandPaletteInvokableCommand() noexcept override = default;
        // Invokable
        winrt::MCP::ICommandResult Invoke(winrt::IInspectable const&)
        {
            auto lastPage{ s_lastWindow->GetRoot().as<winrt::TerminalApp::TerminalPage>() };
            auto pp{ winrt::get_self<winrt::TerminalApp::implementation::TerminalPage>(lastPage) };
            pp->Dispatcher().RunAsync(winrt::Windows::UI::Core::CoreDispatcherPriority::Normal, [=]() {
                pp->DispatchAction(_c.ActionAndArgs());
            });
            return winrt::make<HideCommandResult>();
        }
        // Command
        winrt::hstring Name() { return _c.Name(); }
        winrt::hstring Id() { return _c.ID(); }
        winrt::MCP::IIconInfo Icon() { return winrt::make<StrIconInfo>(_c.Icon().Resolved()); }
        //Notifers
        wil::typed_event<winrt::IInspectable, winrt::MCP::IPropChangedEventArgs> PropChanged;
    };

    winrt::Microsoft::Terminal::Settings::Model::Command _c{ nullptr };
    CommandPaletteListItem(winrt::Microsoft::Terminal::Settings::Model::Command const& command) :
        _c{ command } {}
    ~CommandPaletteListItem() noexcept override = default;
    // ListItem
    winrt::com_array<winrt::MCP::ITag> Tags() { return {}; }
    winrt::MCP::IDetails Details() { return nullptr; }
    winrt::hstring Section() { return {}; }
    winrt::hstring TextToSuggest() { return {}; }
    // CommandItem
    winrt::MCP::ICommand Command()
    {
        return winrt::make<CommandPaletteInvokableCommand>(_c);
    }
    winrt::com_array<winrt::MCP::IContextItem> MoreCommands() { return {}; }
    winrt::MCP::IIconInfo Icon() { return winrt::make<StrIconInfo>(_c.Icon().Resolved()); }
    winrt::hstring Title() { return _c.Name(); }
    winrt::hstring Subtitle() { return _c.LanguageNeutralName(); }
    //Notifers
    wil::typed_event<winrt::IInspectable, winrt::MCP::IPropChangedEventArgs> PropChanged;
};

struct CommandPaletteProviderTopLevelPage : winrt::implements<CommandPaletteProviderTopLevelPage, winrt::MCP::IDynamicListPage, winrt::MCP::IListPage, winrt::MCP::IPage, winrt::MCP::ICommand, winrt::MCP::INotifyPropChanged, winrt::MCP::INotifyItemsChanged>
{
    struct ItemsChangedEventArgs : winrt::implements<ItemsChangedEventArgs, winrt::MCP::IItemsChangedEventArgs>
    {
        int32_t nr;
        ItemsChangedEventArgs(int32_t n) :
            nr(n) {}
        int32_t TotalItems() const { return nr; }
    };
    CommandPaletteProviderTopLevelPage() { _items = _getItems(); }
    winrt::hstring _searchText{};
    // (Dynamic)ListPage
    void SearchText(winrt::hstring st)
    {
        _searchText = st;
        p = fzf::matcher::ParsePattern(_searchText);
        _items = _getItems();
        ItemsChanged.invoke(*this, winrt::make<ItemsChangedEventArgs>(static_cast<int32_t>(_items.size())));
    }
    winrt::hstring SearchText() { return _searchText; }
    winrt::hstring PlaceholderText() { return L"Find something new"; }
    bool ShowDetails() { return false; }
    winrt::MCP::IFilters Filters() { return nullptr; }
    winrt::MCP::IGridProperties GridProperties() { return nullptr; }
    bool HasMoreItems() { return false; }
    winrt::MCP::ICommandItem EmptyContent() { return nullptr; }
    void LoadMore() {}

    std::vector<winrt::MCP::IListItem> _items;
    std::vector<winrt::MCP::IListItem> _getItems()
    {
        std::vector<winrt::MCP::IListItem> items;
        const auto settings{ winrt::TerminalApp::implementation::AppLogic::CurrentAppSettings() };
        struct mr
        {
            int32_t Score;
            winrt::Microsoft::Terminal::Settings::Model::Command Command;
        };
        std::vector<mr> matches;
        for (const auto& v : settings.ActionMap().AllCommands())
        {
            auto res{ fzf::matcher::Match(v.Name(), p) };
            if (_searchText.empty() || (res && res->Score > 0))
            {
                matches.emplace_back(mr{ res->Score, v });
            }
        }
        std::sort(std::begin(matches), std::end(matches), [](auto&& left, auto&& right) {
            return left.Score > right.Score;
        });
        for (const auto& [_, v] : matches)
        {
            items.emplace_back(winrt::make<CommandPaletteListItem>(v));
        }
        return items;
    }

    winrt::com_array<winrt::MCP::IListItem> GetItems()
    {
        return { std::begin(_items), std::end(_items) };
    }

    // Page
    winrt::hstring Title() { return L"Windows Terminal Command Palette"; }
    bool IsLoading() { return false; }
    winrt::MCP::OptionalColor AccentColor()
    {
        return { .HasValue = false };
    }
    // Command
    winrt::hstring Name() { return L"Windows Terminal Commands"; }
    winrt::hstring Id() { return L"doit"; }
    winrt::MCP::IIconInfo Icon() { return nullptr; }
    // Notifiers
    wil::typed_event<winrt::IInspectable, winrt::MCP::IItemsChangedEventArgs> ItemsChanged;
    wil::typed_event<winrt::IInspectable, winrt::MCP::IPropChangedEventArgs> PropChanged;

private:
    fzf::matcher::Pattern p;
};

struct CommandPaletteProviderTopLevelCommandItem : winrt::implements<CommandPaletteProviderTopLevelCommandItem, winrt::MCP::ICommandItem, winrt::MCP::INotifyPropChanged>
{
    winrt::MCP::ICommand Command() { return winrt::make<CommandPaletteProviderTopLevelPage>(); }
    winrt::com_array<winrt::MCP::IContextItem> MoreCommands() { return {}; }
    winrt::MCP::IIconInfo Icon() { return nullptr; }
    winrt::hstring Title() { return L"Browse Windows Terminal Commands"; }
    winrt::hstring Subtitle() { return L"Find out if it works, live"; }
    wil::typed_event<winrt::IInspectable, winrt::MCP::IPropChangedEventArgs> PropChanged;
};

struct CommandPaletteProvider : winrt::implements<CommandPaletteProvider, winrt::MCP::ICommandProvider, winrt::Windows::Foundation::IClosable, winrt::MCP::INotifyItemsChanged>
{
    CommandPaletteProvider()
    {
        _theOneCommand = winrt::make<CommandPaletteProviderTopLevelCommandItem>();
    }
    using hstring = ::winrt::hstring;
    hstring Id() { return L"WindowsTerminalDev"; }
    hstring DisplayName() { return L"Windows Terminal Dev"; }
    winrt::MCP::IIconInfo Icon() { return nullptr; }
    winrt::MCP::ICommandSettings Settings()
    {
        return nullptr;
    }
    bool Frozen() { return false; }
    winrt::com_array<winrt::MCP::ICommandItem> TopLevelCommands()
    {
        std::vector<winrt::MCP::ICommandItem> v;
        v.emplace_back(_theOneCommand);
        return { v.begin(), v.end() };
    }
    winrt::com_array<winrt::MCP::IFallbackCommandItem> FallbackCommands() { return {}; }
    winrt::MCP::ICommand GetCommand(const hstring& id)
    {
        (void)id;
        return nullptr;
    }
    wil::typed_event<winrt::IInspectable, winrt::MCP::IItemsChangedEventArgs> ItemsChanged;
    void InitializeWithHost(const winrt::MCP::IExtensionHost& host)
    {
        _host = host;
    }
    void Close()
    {
    }
    winrt::MCP::IExtensionHost _host{ nullptr };
    winrt::MCP::ICommandItem _theOneCommand;
};

struct CommandPaletteExtension : winrt::implements<CommandPaletteExtension, winrt::MCP::IExtension>
{
    static winrt::MCP::ICommandProvider _getPaletteProvider()
    {
        static auto foo = winrt::make<CommandPaletteProvider>();
        return foo;
    }
    winrt::IInspectable GetProvider(winrt::MCP::ProviderType type)
    {
        switch (type)
        {
        case winrt::MCP::ProviderType::Commands:
            return _getPaletteProvider();
        default:
            FAIL_FAST();
        }
    }
    void Dispose() {}
};

struct CPXFactory : winrt::implements<CPXFactory, IClassFactory>
{
    HRESULT __stdcall CreateInstance(IUnknown* outer, GUID const& iid, void** result)
    {
        *result = nullptr;
        if (outer)
            return CLASS_E_NOAGGREGATION;
        return winrt::make_self<CommandPaletteExtension>()->QueryInterface(iid, result);
    }
    HRESULT __stdcall LockServer(BOOL) noexcept final { return S_OK; }
};

////////////////////////////////////////////////////////////////////////////////
// Error message handling. This is in this file rather than with the warnings in
// TerminalWindow, because the error text might be:
// * A error we defined here
// * An error from deserializing the json
// * Any other fatal error loading the settings
// So all we pass on is the actual text of the error, rather than the
// combination of things that might have caused an error.

// !!! IMPORTANT !!!
// Make sure that these keys are in the same order as the
// SettingsLoadWarnings/Errors enum is!
static const std::array settingsLoadErrorsLabels{
    USES_RESOURCE(L"NoProfilesText"),
    USES_RESOURCE(L"AllProfilesHiddenText")
};

static_assert(settingsLoadErrorsLabels.size() == static_cast<size_t>(SettingsLoadErrors::ERRORS_SIZE));

// Function Description:
// - General-purpose helper for looking up a localized string for a
//   warning/error. First will look for the given key in the provided map of
//   keys->strings, where the values in the map are ResourceKeys. If it finds
//   one, it will lookup the localized string from that ResourceKey.
// - If it does not find a key, it'll return an empty string
// Arguments:
// - key: the value to use to look for a resource key in the given map
// - map: A map of keys->Resource keys.
// Return Value:
// - the localized string for the given type, if it exists.
template<typename T>
winrt::hstring _GetMessageText(uint32_t index, const T& keys)
{
    if (index < keys.size())
    {
        return GetLibraryResourceString(til::at(keys, index));
    }
    return {};
}
// Function Description:
// - Gets the text from our ResourceDictionary for the given
//   SettingsLoadError. If there is no such text, we'll return nullptr.
// - The warning should have an entry in settingsLoadErrorsLabels.
// Arguments:
// - error: the SettingsLoadErrors value to get the localized text for.
// Return Value:
// - localized text for the given error
static winrt::hstring _GetErrorText(SettingsLoadErrors error)
{
    return _GetMessageText(static_cast<uint32_t>(error), settingsLoadErrorsLabels);
}

namespace winrt::TerminalApp::implementation
{
    // Function Description:
    // - Get the AppLogic for the current active Xaml application, or null if there isn't one.
    // Return value:
    // - A pointer (bare) to the AppLogic, or nullptr. The app logic outlives all other objects,
    //   unless the application is in a terrible way, so this is "safe."
    AppLogic* AppLogic::Current() noexcept
    try
    {
        if (auto currentXamlApp{ winrt::Windows::UI::Xaml::Application::Current().try_as<winrt::TerminalApp::App>() })
        {
            if (auto appLogicPointer{ winrt::get_self<AppLogic>(currentXamlApp.Logic()) })
            {
                return appLogicPointer;
            }
        }
        return nullptr;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return nullptr;
    }

    // Method Description:
    // - Returns the settings currently in use by the entire Terminal application.
    // - IMPORTANT! This can throw! Make sure to try/catch this, so that the
    //   LocalTests don't crash (because their Application::Current() won't be a
    //   AppLogic)
    // Throws:
    // - HR E_INVALIDARG if the app isn't up and running.
    const CascadiaSettings AppLogic::CurrentAppSettings()
    {
        auto appLogic{ ::winrt::TerminalApp::implementation::AppLogic::Current() };
        THROW_HR_IF_NULL(E_INVALIDARG, appLogic);
        return appLogic->Settings();
    }

    AppLogic::AppLogic()
    {
        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like App just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is App not
        // registered?" when it definitely is.

        // The TerminalPage has to be constructed during our construction, to
        // make sure that there's a terminal page for callers of
        // SetTitleBarContent

        _isElevated = ::Microsoft::Console::Utils::IsRunningElevated();
        _canDragDrop = ::Microsoft::Console::Utils::CanUwpDragDrop();

        _reloadSettings = std::make_shared<ThrottledFunc<>>(
            DispatcherQueue::GetForCurrentThread(),
            til::throttled_func_options{
                .delay = std::chrono::milliseconds{ 100 },
                .debounce = true,
                .trailing = true,
            },
            [weakSelf = get_weak()]() {
                if (auto self{ weakSelf.get() })
                {
                    self->ReloadSettings();
                }
            });

        _languageProfileNotifier = winrt::make_self<LanguageProfileNotifier>([this]() {
            _reloadSettings->Run();
        });

        // Do this here, rather than at the top of main. This will prevent us from
        // including this variable in the vars we serialize in the
        // Remoting::CommandlineArgs up in HandleCommandlineArgs.
        _setupFolderPathEnvVar();

        static auto ext = winrt::make_self<CommandPaletteExtension>();
        // {C1C5E6A5-07DB-4277-9108-D28C9129FCAF}
        static const GUID cpxg = { 0xc1c5e6a5, 0x7db, 0x4277, { 0x91, 0x8, 0xd2, 0x8c, 0x91, 0x29, 0xfc, 0xaf } };

        DWORD c;
        CoRegisterClassObject(cpxg, winrt::make<CPXFactory>().get(), CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &c);
    }

    // Method Description:
    // - Called around the codebase to discover if Terminal is running elevated
    // Arguments:
    // - <none> - reports internal state
    // Return Value:
    // - True if elevated, false otherwise.
    bool AppLogic::IsRunningElevated() const noexcept
    {
        return _isElevated;
    }
    bool AppLogic::CanDragDrop() const noexcept
    {
        return _canDragDrop;
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
    void AppLogic::Create()
    {
        // Assert that we've already loaded our settings. We have to do
        // this as a MTA, before the app is Create()'d
        WINRT_ASSERT(_loadedInitialSettings);

        TraceLoggingWrite(
            g_hTerminalAppProvider,
            "AppCreated",
            TraceLoggingDescription("Event emitted when the application is started"),
            TraceLoggingBool(_settings.GlobalSettings().ShowTabsInTitlebar(), "TabsInTitlebar"),
            TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
            TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
    }

    // Method Description:
    // - Attempt to load the settings. If we fail for any reason, returns an error.
    // Return Value:
    // - S_OK if we successfully parsed the settings; otherwise, an appropriate HRESULT.
    [[nodiscard]] HRESULT AppLogic::_TryLoadSettings() noexcept
    {
        auto hr = E_FAIL;

        try
        {
            auto newSettings = CascadiaSettings::LoadAll();

            if (newSettings.GetLoadingError())
            {
                _settingsLoadExceptionText = _GetErrorText(newSettings.GetLoadingError().Value());
                return E_INVALIDARG;
            }
            else if (!newSettings.GetSerializationErrorMessage().empty())
            {
                _settingsLoadExceptionText = newSettings.GetSerializationErrorMessage();
                return E_INVALIDARG;
            }

            _warnings.clear();
            for (uint32_t i = 0; i < newSettings.Warnings().Size(); i++)
            {
                _warnings.push_back(newSettings.Warnings().GetAt(i));
            }

            _hasSettingsStartupActions = false;
            const auto startupActions = newSettings.GlobalSettings().StartupActions();
            if (!startupActions.empty())
            {
                _settingsAppArgs.FullResetState();

                ExecuteCommandlineArgs args{ newSettings.GlobalSettings().StartupActions() };
                auto result = _settingsAppArgs.ParseArgs(args);
                if (result == 0)
                {
                    _hasSettingsStartupActions = true;

                    // Validation also injects new-tab command if implicit new-tab was provided.
                    _settingsAppArgs.ValidateStartupCommands();
                }
                else
                {
                    _warnings.push_back(SettingsLoadWarnings::FailedToParseStartupActions);
                }
            }

            _settings = std::move(newSettings);

            hr = _warnings.empty() ? S_OK : S_FALSE;
        }
        catch (const winrt::hresult_error& e)
        {
            hr = e.code();
            _settingsLoadExceptionText = e.message();
            LOG_HR(hr);
        }
        catch (...)
        {
            hr = wil::ResultFromCaughtException();
            LOG_HR(hr);
        }
        return hr;
    }

    bool AppLogic::HasSettingsStartupActions() const noexcept
    {
        return _hasSettingsStartupActions;
    }

    // Call this function after loading your _settings.
    // It handles any CPU intensive settings updates (like updating the Jumplist)
    // which should thus only occur if the settings file actually changed.
    void AppLogic::_ProcessLazySettingsChanges()
    {
        const auto hash = _settings.Hash();
        const auto applicationState = ApplicationState::SharedInstance();
        const auto cachedHash = applicationState.SettingsHash();

        // The hash might be empty if LoadAll() failed and we're dealing with the defaults settings object.
        // In that case we can just wait until the user fixed their settings or CascadiaSettings fixed
        // itself and either will soon trigger a settings reload.
        if (hash.empty() || hash == cachedHash)
        {
            return;
        }

        Jumplist::UpdateJumplist(_settings);
        applicationState.SettingsHash(hash);
    }

    // Method Description:
    // - Registers for changes to the settings folder and upon a updated settings
    //      profile calls ReloadSettings().
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AppLogic::_RegisterSettingsChange()
    {
        const std::filesystem::path settingsPath{ std::wstring_view{ CascadiaSettings::SettingsPath() } };
        _reader.create(
            settingsPath.parent_path().c_str(),
            false,
            // We want file modifications, AND when files are renamed to be
            // settings.json. This second case will oftentimes happen with text
            // editors, who will write a temp file, then rename it to be the
            // actual file you wrote. So listen for that too.
            wil::FolderChangeEvents::FileName | wil::FolderChangeEvents::LastWriteTime,
            [this, settingsBasename = settingsPath.filename()](wil::FolderChangeEvent, PCWSTR fileModified) {
                // DO NOT create a static reference to ApplicationState::SharedInstance here.
                //
                // ApplicationState::SharedInstance already caches its own
                // static ref. If _we_ keep a static ref to the member in
                // AppState, then our reference will keep ApplicationState alive
                // after the `ActionToStringMap` gets cleaned up. Then, when we
                // try to persist the actions in the window state, we won't be
                // able to. We'll try to look up the action and the map just
                // won't exist. We'll explode, even though the Terminal is
                // tearing down anyways. So we'll just die, but still invoke
                // WinDBG's post-mortem debugger, who won't be able to attach to
                // the process that's already exiting.
                //
                // So DON'T ~give a mouse a cookie~ take a static ref here.

                const auto modifiedBasename = std::filesystem::path{ fileModified }.filename();

                if (modifiedBasename == settingsBasename)
                {
                    _reloadSettings->Run();
                }
            });
    }

    void AppLogic::_ApplyLanguageSettingChange() noexcept
    try
    {
        const auto language = _settings.GlobalSettings().Language();

        if (!IsPackaged())
        {
            if (!language.empty())
            {
                // We cannot use the packaged app API, PrimaryLanguageOverride, but we *can* tell the resource loader
                // to set the Language for all loaded resources to the user's preferred language.
                winrt::Windows::ApplicationModel::Resources::Core::ResourceContext::SetGlobalQualifierValue(L"Language", language);
            }
            return;
        }

        using ApplicationLanguages = winrt::Windows::Globalization::ApplicationLanguages;

        // NOTE: PrimaryLanguageOverride throws if this instance is unpackaged.
        const auto primaryLanguageOverride = ApplicationLanguages::PrimaryLanguageOverride();
        if (primaryLanguageOverride != language)
        {
            ApplicationLanguages::PrimaryLanguageOverride(language);
        }
    }
    CATCH_LOG()

    // Method Description:
    // - Reloads the settings from the settings.json file.
    // - When this is called the first time, this initializes our settings. See
    //   CascadiaSettings for more details. Additionally hooks up our callbacks
    //   for keybinding events to the keybindings object.
    //   - NOTE: when called initially, this must be called from a MTA if we're
    //     running as a packaged application. The Windows.Storage APIs require a
    //     MTA. If this isn't happening during startup, it'll need to happen on
    //     a background thread.
    void AppLogic::ReloadSettings()
    {
        // Attempt to load our settings.
        // If it fails,
        //  - don't change the settings (and don't actually apply the new settings)
        //  - don't persist them.
        //  - display a loading error
        _settingsLoadedResult = _TryLoadSettings();

        const auto initialLoad = !_loadedInitialSettings;
        _loadedInitialSettings = true;

        if (FAILED(_settingsLoadedResult))
        {
            if (initialLoad)
            {
                _settings = CascadiaSettings::LoadDefaults();
            }
            else
            {
                auto warnings{ winrt::multi_threaded_vector<SettingsLoadWarnings>() };
                for (auto&& warn : _warnings)
                {
                    warnings.Append(warn);
                }
                auto ev = winrt::make_self<SettingsLoadEventArgs>(true,
                                                                  static_cast<uint64_t>(_settingsLoadedResult),
                                                                  _settingsLoadExceptionText,
                                                                  warnings.GetView(),
                                                                  _settings);
                SettingsChanged.raise(*this, *ev);
                return;
            }
        }
        else
        {
            _settings.LogSettingChanges(true);
        }

        _ApplyLanguageSettingChange();
        _ProcessLazySettingsChanges();

        if (initialLoad)
        {
            // Register for directory change notification.
            _RegisterSettingsChange();
            return;
        }

        // Here, we successfully reloaded the settings, and created a new
        // TerminalSettings object.

        auto warnings{ winrt::multi_threaded_vector<SettingsLoadWarnings>() };
        for (auto&& warn : _warnings)
        {
            warnings.Append(warn);
        }
        auto ev = winrt::make_self<SettingsLoadEventArgs>(!initialLoad,
                                                          _settingsLoadedResult,
                                                          _settingsLoadExceptionText,
                                                          warnings.GetView(),
                                                          _settings);
        SettingsChanged.raise(*this, *ev);
    }

    // This is a continuation of AppLogic::Create() and includes the more expensive parts.
    void AppLogic::NotifyRootInitialized()
    {
        if (_notifyRootInitializedCalled.exchange(true, std::memory_order_relaxed))
        {
            return;
        }

        // Both LoadSettings and ReloadSettings are supposed to call this function,
        // but LoadSettings skips it, so that the UI starts up faster.
        // Now that the UI is present we can do them with a less significant UX impact.
        _ProcessLazySettingsChanges();

        FILETIME creationTime, exitTime, kernelTime, userTime, now;
        if (GetThreadTimes(GetCurrentThread(), &creationTime, &exitTime, &kernelTime, &userTime))
        {
            static constexpr auto asInteger = [](const FILETIME& f) {
                ULARGE_INTEGER i;
                i.LowPart = f.dwLowDateTime;
                i.HighPart = f.dwHighDateTime;
                return i.QuadPart;
            };
            static constexpr auto asSeconds = [](uint64_t v) {
                return v * 1e-7f;
            };

            GetSystemTimeAsFileTime(&now);

            const auto latency = asSeconds(asInteger(now) - asInteger(creationTime));

            TraceLoggingWrite(
                g_hTerminalAppProvider,
                "AppInitialized",
                TraceLoggingDescription("Event emitted once the app is initialized"),
                TraceLoggingFloat32(latency, "latency"),
                TraceLoggingKeyword(MICROSOFT_KEYWORD_MEASURES),
                TelemetryPrivacyDataTag(PDT_ProductAndServiceUsage));
        }
    }

    // Method Description:
    // - Returns a pointer to the global shared settings.
    [[nodiscard]] CascadiaSettings AppLogic::Settings() const noexcept
    {
        return _settings;
    }

    Windows::Foundation::Collections::IMapView<Microsoft::Terminal::Control::KeyChord, Microsoft::Terminal::Settings::Model::Command> AppLogic::GlobalHotkeys()
    {
        return _settings.GlobalSettings().ActionMap().GlobalHotkeys();
    }

    TerminalApp::TerminalWindow AppLogic::CreateNewWindow()
    {
        auto warnings{ winrt::multi_threaded_vector<SettingsLoadWarnings>() };
        for (auto&& warn : _warnings)
        {
            warnings.Append(warn);
        }
        auto ev = winrt::make_self<SettingsLoadEventArgs>(false,
                                                          _settingsLoadedResult,
                                                          _settingsLoadExceptionText,
                                                          warnings.GetView(),
                                                          _settings);

        auto window = winrt::make_self<implementation::TerminalWindow>(*ev, _contentManager);

        if (_hasSettingsStartupActions)
        {
            window->SetSettingsStartupArgs(_settingsAppArgs.GetStartupActions());
        }
        s_lastWindow = window;
        return *window;
    }

    winrt::TerminalApp::ContentManager AppLogic::ContentManager()
    {
        return _contentManager;
    }

    // Function Description
    // * Adds a `WT_SETTINGS_DIR` env var to our own environment block, that
    //   points at our settings directory. This allows portable installs to
    //   refer to files in the portable install using %WT_SETTINGS_DIR%
    void AppLogic::_setupFolderPathEnvVar()
    {
        std::wstring path{ CascadiaSettings::SettingsPath() };
        auto folderPath = path.substr(0, path.find_last_of(L"\\"));
        SetEnvironmentVariableW(L"WT_SETTINGS_DIR", folderPath.c_str());
    }
}
