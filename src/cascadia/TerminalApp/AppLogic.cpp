// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AppLogic.h"
#include "../inc/WindowingBehavior.h"
#include "AppLogic.g.cpp"
#include "FindTargetWindowResult.g.cpp"
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
// TODO! This section probably should be in TerminalWindow with the warnings

// clang-format off
static const std::array settingsLoadErrorsLabels {
    USES_RESOURCE(L"NoProfilesText"),
    USES_RESOURCE(L"AllProfilesHiddenText")
};
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

// clang-format on

////////////////////////////////////////////////////////////////////////////////

static constexpr std::wstring_view StartupTaskName = L"StartTerminalOnLoginTask";

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
        return appLogic->GetSettings();
    }

    AppLogic::AppLogic() :
        _reloadState{ std::chrono::milliseconds(100), []() { ApplicationState::SharedInstance().Reload(); } }
    {
        // For your own sanity, it's better to do setup outside the ctor.
        // If you do any setup in the ctor that ends up throwing an exception,
        // then it might look like App just failed to activate, which will
        // cause you to chase down the rabbit hole of "why is App not
        // registered?" when it definitely is.

        // The TerminalPage has to be constructed during our construction, to
        // make sure that there's a terminal page for callers of
        // SetTitleBarContent
        _isElevated = ::Microsoft::Console::Utils::IsElevated();

        _reloadSettings = std::make_shared<ThrottledFuncTrailing<>>(winrt::Windows::System::DispatcherQueue::GetForCurrentThread(), std::chrono::milliseconds(100), [weakSelf = get_weak()]() {
            if (auto self{ weakSelf.get() })
            {
                self->ReloadSettings();
            }
        });

        _languageProfileNotifier = winrt::make_self<LanguageProfileNotifier>([this]() {
            _reloadSettings->Run();
        });
    }

    // Method Description:
    // - Called around the codebase to discover if this is a UWP where we need to turn off specific settings.
    // Arguments:
    // - <none> - reports internal state
    // Return Value:
    // - True if UWP, false otherwise.
    bool AppLogic::IsUwp() const noexcept
    {
        return _isUwp;
    }

    // Method Description:
    // - Called around the codebase to discover if Terminal is running elevated
    // Arguments:
    // - <none> - reports internal state
    // Return Value:
    // - True if elevated, false otherwise.
    bool AppLogic::IsElevated() const noexcept
    {
        return _isElevated;
    }

    // Method Description:
    // - Called by UWP context invoker to let us know that we may have to change some of our behaviors
    //   for being a UWP
    // Arguments:
    // - <none> (sets to UWP = true, one way change)
    // Return Value:
    // - <none>
    void AppLogic::RunAsUwp()
    {
        _isUwp = true;
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

        // In UWP mode, we cannot handle taking over the title bar for tabs,
        // so this setting is overridden to false no matter what the preference is.
        if (_isUwp)
        {
            _settings.GlobalSettings().ShowTabsInTitlebar(false);
        }

        // TODO! These used to be in `_root->Initialized`. Where do they belong now?
        {
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
        };

        _ApplyLanguageSettingChange();
        _ApplyStartupTaskStateChange();

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
            auto newSettings = _isUwp ? CascadiaSettings::LoadUniversal() : CascadiaSettings::LoadAll();

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

            _warnings.Clear();
            for (uint32_t i = 0; i < newSettings.Warnings().Size(); i++)
            {
                _warnings.Append(newSettings.Warnings().GetAt(i));
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
                    _warnings.Append(SettingsLoadWarnings::FailedToParseStartupActions);
                }
            }

            _settings = std::move(newSettings);
            hr = (_warnings.Size()) == 0 ? S_OK : S_FALSE;
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

                const winrt::hstring modifiedBasename{ std::filesystem::path{ fileModified }.filename().c_str() };

                if (modifiedBasename == settingsBasename)
                {
                    _reloadSettings->Run();
                }
                else if (ApplicationState::SharedInstance().IsStatePath(modifiedBasename))
                {
                    _reloadState();
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

    // Function Description:
    // Returns the current app package or nullptr.
    // TRANSITIONAL
    // Exists to work around a compiler bug. This function encapsulates the
    // exception handling that we used to keep around calls to Package::Current,
    // so that when it's called inside a coroutine and fails it doesn't explode
    // terribly.
    static winrt::Windows::ApplicationModel::Package GetCurrentPackageNoThrow() noexcept
    {
        try
        {
            return winrt::Windows::ApplicationModel::Package::Current();
        }
        catch (...)
        {
            // discard any exception -- literally pretend we're not in a package
        }
        return nullptr;
    }

    fire_and_forget AppLogic::_ApplyStartupTaskStateChange()
    try
    {
        // First, make sure we're running in a packaged context. This method
        // won't work, and will crash mysteriously if we're running unpackaged.
        const auto package{ GetCurrentPackageNoThrow() };
        if (package == nullptr)
        {
            co_return;
        }

        const auto tryEnableStartupTask = _settings.GlobalSettings().StartOnUserLogin();
        const auto task = co_await StartupTask::GetAsync(StartupTaskName);

        switch (task.State())
        {
        case StartupTaskState::Disabled:
            if (tryEnableStartupTask)
            {
                co_await task.RequestEnableAsync();
            }
            break;
        case StartupTaskState::DisabledByUser:
            // TODO: GH#6254: define UX for other StartupTaskStates
            break;
        case StartupTaskState::Enabled:
            if (!tryEnableStartupTask)
            {
                task.Disable();
            }
            break;
        }
    }
    CATCH_LOG();

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
                // TODO! Arg should be a SettingsLoadEventArgs{ result, warnings, error, settings}
                //
                // _SettingsChangedHandlers(*this, make_self<SettingsLoadEventArgs>( _settingsLoadedResult, warnings, _settingsLoadExceptionText, settings}))

                // const winrt::hstring titleKey = USES_RESOURCE(L"ReloadJsonParseErrorTitle");
                // const winrt::hstring textKey = USES_RESOURCE(L"ReloadJsonParseErrorText");
                // _ShowLoadErrorsDialog(titleKey, textKey, _settingsLoadedResult);
                // return;

                auto ev = winrt::make_self<SettingsLoadEventArgs>(true,
                                                                  static_cast<uint64_t>(_settingsLoadedResult),
                                                                  _settingsLoadExceptionText,
                                                                  _warnings,
                                                                  _settings);
                _SettingsChangedHandlers(*this, *ev);
                return;
            }
        }

        if (initialLoad)
        {
            // Register for directory change notification.
            _RegisterSettingsChange();
            return;
        }

        // if (_settingsLoadedResult == S_FALSE)
        // {
        //     _ShowLoadWarningsDialog();
        // }

        // Here, we successfully reloaded the settings, and created a new
        // TerminalSettings object.

        _ApplyLanguageSettingChange();
        _ApplyStartupTaskStateChange();
        _ProcessLazySettingsChanges();

        auto ev = winrt::make_self<SettingsLoadEventArgs>(!initialLoad,
                                                          _settingsLoadedResult,
                                                          _settingsLoadExceptionText,
                                                          _warnings,
                                                          _settings);
        _SettingsChangedHandlers(*this, *ev);
    }

    // Method Description:
    // - Returns a pointer to the global shared settings.
    [[nodiscard]] CascadiaSettings AppLogic::GetSettings() const noexcept
    {
        return _settings;
    }

    // Method Description:
    // - Triggers the setup of the listener for incoming console connections
    //   from the operating system.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void AppLogic::SetInboundListener()
    {
        // TODO!
        // _root->SetInboundListener(false);
    }

    bool AppLogic::ShouldImmediatelyHandoffToElevated()
    {
        // TODO! Merge that code in here.
        //   * Probably need to pass in the startupActions that the first window is being started with
        //   * Or like, a reference to the first TerminalWindow object, or something

        // return _root != nullptr ? _root->ShouldImmediatelyHandoffToElevated(_settings) : false;
        return false;
    }
    void AppLogic::HandoffToElevated()
    {
        // TODO! Merge that code in here.

        // if (_root)
        // {
        //     _root->HandoffToElevated(_settings);
        // }
    }

    // Method Description:
    // - Parse the given commandline args in an attempt to find the specified
    //   window. The rest of the args are ignored for now (they'll be handled
    //   whenever the commandline gets to the window it was intended for).
    // - Note that this function will only ever be called by the monarch. A
    //   return value of `0` in this case does not mean "run the commandline in
    //   _this_ process", rather it means "run the commandline in the current
    //   process", whoever that may be.
    // Arguments:
    // - args: an array of strings to process as a commandline. These args can contain spaces
    // Return Value:
    // - 0: We should handle the args "in the current window".
    // - WindowingBehaviorUseNew: We should handle the args in a new window
    // - WindowingBehaviorUseExisting: We should handle the args "in
    //   the current window ON THIS DESKTOP"
    // - WindowingBehaviorUseAnyExisting: We should handle the args "in the current
    //   window ON ANY DESKTOP"
    // - anything else: We should handle the commandline in the window with the given ID.
    TerminalApp::FindTargetWindowResult AppLogic::FindTargetWindow(array_view<const winrt::hstring> args)
    {
        if (!_loadedInitialSettings)
        {
            // Load settings if we haven't already
            ReloadSettings();
        }

        return AppLogic::_doFindTargetWindow(args, _settings.GlobalSettings().WindowingBehavior());
    }

    // The main body of this function is a static helper, to facilitate unit-testing
    TerminalApp::FindTargetWindowResult AppLogic::_doFindTargetWindow(array_view<const winrt::hstring> args,
                                                                      const Microsoft::Terminal::Settings::Model::WindowingMode& windowingBehavior)
    {
        ::TerminalApp::AppCommandlineArgs appArgs;
        const auto result = appArgs.ParseArgs(args);
        if (result == 0)
        {
            if (!appArgs.GetExitMessage().empty())
            {
                return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseNew);
            }

            const std::string parsedTarget{ appArgs.GetTargetWindow() };

            // If the user did not provide any value on the commandline,
            // then lookup our windowing behavior to determine what to do
            // now.
            if (parsedTarget.empty())
            {
                auto windowId = WindowingBehaviorUseNew;
                switch (windowingBehavior)
                {
                case WindowingMode::UseNew:
                    windowId = WindowingBehaviorUseNew;
                    break;
                case WindowingMode::UseExisting:
                    windowId = WindowingBehaviorUseExisting;
                    break;
                case WindowingMode::UseAnyExisting:
                    windowId = WindowingBehaviorUseAnyExisting;
                    break;
                }
                return winrt::make<FindTargetWindowResult>(windowId);
            }

            // Here, the user _has_ provided a window-id on the commandline.
            // What is it? Let's start by checking if it's an int, for the
            // window's ID:
            try
            {
                auto windowId = ::base::saturated_cast<int32_t>(std::stoi(parsedTarget));

                // If the user provides _any_ negative number, then treat it as
                // -1, for "use a new window".
                if (windowId < 0)
                {
                    windowId = -1;
                }

                // Hooray! This is a valid integer. The set of possible values
                // here is {-1, 0, ℤ+}. Let's return that window ID.
                return winrt::make<FindTargetWindowResult>(windowId);
            }
            catch (...)
            {
                // Value was not a valid int. It could be any other string to
                // use as a title though!
                //
                // First, check the reserved keywords:
                if (parsedTarget == "new")
                {
                    return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseNew);
                }
                else if (parsedTarget == "last")
                {
                    return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseExisting);
                }
                else
                {
                    // The string they provided wasn't an int, it wasn't "new"
                    // or "last", so whatever it is, that's the name they get.
                    winrt::hstring winrtName{ til::u8u16(parsedTarget) };
                    return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseName, winrtName);
                }
            }
        }

        // Any unsuccessful parse will be a new window. That new window will try
        // to handle the commandline itself, and find that the commandline
        // failed to parse. When that happens, the new window will display the
        // message box.
        //
        // This will also work for the case where the user specifies an invalid
        // commandline in conjunction with `-w 0`. This function will determine
        // that the commandline has a  parse error, and indicate that we should
        // create a new window. Then, in that new window, we'll try to  set the
        // StartupActions, which will again fail, returning the correct error
        // message.
        return winrt::make<FindTargetWindowResult>(WindowingBehaviorUseNew);
    }

    Windows::Foundation::Collections::IMapView<Microsoft::Terminal::Control::KeyChord, Microsoft::Terminal::Settings::Model::Command> AppLogic::GlobalHotkeys()
    {
        return _settings.GlobalSettings().ActionMap().GlobalHotkeys();
    }

    Microsoft::Terminal::Settings::Model::Theme AppLogic::Theme()
    {
        return _settings.GlobalSettings().CurrentTheme();
    }

    TerminalApp::TerminalWindow AppLogic::CreateNewWindow()
    {
        if (_settings == nullptr)
        {
            ReloadSettings();
        }
        auto window = winrt::make_self<implementation::TerminalWindow>(_settings);
        this->SettingsChanged({ window->get_weak(), &implementation::TerminalWindow::UpdateSettingsHandler });
        if (_hasSettingsStartupActions)
        {
            window->SetSettingsStartupArgs(_settingsAppArgs.GetStartupActions());
        }
        return *window;
    }
}
