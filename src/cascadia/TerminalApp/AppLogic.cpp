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
    using IInspectable = Windows::Foundation::IInspectable;
}

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

        _reloadSettings = std::make_shared<ThrottledFuncTrailing<>>(winrt::Windows::System::DispatcherQueue::GetForCurrentThread(), std::chrono::milliseconds(100), [weakSelf = get_weak()]() {
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
    // - S_OK if we successfully parsed the settings, otherwise an appropriate HRESULT.
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
        if (!IsPackaged())
        {
            return;
        }

        using ApplicationLanguages = winrt::Windows::Globalization::ApplicationLanguages;

        // NOTE: PrimaryLanguageOverride throws if this instance is unpackaged.
        const auto primaryLanguageOverride = ApplicationLanguages::PrimaryLanguageOverride();
        const auto language = _settings.GlobalSettings().Language();

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
                                                                  warnings,
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
                                                          warnings,
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
                                                          warnings,
                                                          _settings);

        auto window = winrt::make_self<implementation::TerminalWindow>(*ev, _contentManager);

        if (_hasSettingsStartupActions)
        {
            window->SetSettingsStartupArgs(_settingsAppArgs.GetStartupActions());
        }
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
