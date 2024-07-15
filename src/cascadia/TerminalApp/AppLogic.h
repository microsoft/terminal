// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "AppLogic.g.h"
#include "FindTargetWindowResult.g.h"

#include "Jumplist.h"
#include "LanguageProfileNotifier.h"
#include "AppCommandlineArgs.h"
#include "TerminalWindow.h"
#include "ContentManager.h"

#include <inc/cppwinrt_utils.h>
#include <ThrottledFunc.h>

#ifdef UNIT_TESTING
// fwdecl unittest classes
namespace TerminalAppLocalTests
{
    class CommandlineTest;
};
#endif

namespace winrt::TerminalApp::implementation
{
    struct FindTargetWindowResult : FindTargetWindowResultT<FindTargetWindowResult>
    {
        WINRT_PROPERTY(int32_t, WindowId, -1);
        WINRT_PROPERTY(winrt::hstring, WindowName, L"");

    public:
        FindTargetWindowResult(const int32_t id, const winrt::hstring& name) :
            _WindowId{ id }, _WindowName{ name } {};

        FindTargetWindowResult(const int32_t id) :
            FindTargetWindowResult(id, L""){};
    };

    struct AppLogic : AppLogicT<AppLogic>
    {
    public:
        static AppLogic* Current() noexcept;
        static const Microsoft::Terminal::Settings::Model::CascadiaSettings CurrentAppSettings();

        AppLogic();

        void Create();
        bool IsRunningElevated() const noexcept;
        bool CanDragDrop() const noexcept;
        void ReloadSettings();
        void NotifyRootInitialized();

        bool HasSettingsStartupActions() const noexcept;
        bool ShouldUsePersistedLayout() const;

        [[nodiscard]] Microsoft::Terminal::Settings::Model::CascadiaSettings GetSettings() const noexcept;

        TerminalApp::FindTargetWindowResult FindTargetWindow(array_view<const winrt::hstring> actions);

        Windows::Foundation::Collections::IMapView<Microsoft::Terminal::Control::KeyChord, Microsoft::Terminal::Settings::Model::Command> GlobalHotkeys();

        Microsoft::Terminal::Settings::Model::Theme Theme();
        bool IsolatedMode();
        bool AllowHeadless();
        bool RequestsTrayIcon();

        TerminalApp::TerminalWindow CreateNewWindow();

        winrt::TerminalApp::ContentManager ContentManager();

        TerminalApp::ParseCommandlineResult GetParseCommandlineMessage(array_view<const winrt::hstring> args);

        til::typed_event<winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::SettingsLoadEventArgs> SettingsChanged;

    private:
        bool _isElevated{ false };
        bool _canDragDrop{ false };
        std::atomic<bool> _notifyRootInitializedCalled{ false };

        Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };

        winrt::hstring _settingsLoadExceptionText;
        HRESULT _settingsLoadedResult = S_OK;
        bool _loadedInitialSettings = false;

        bool _hasSettingsStartupActions{ false };
        ::TerminalApp::AppCommandlineArgs _settingsAppArgs;

        std::shared_ptr<ThrottledFuncTrailing<>> _reloadSettings;

        std::vector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> _warnings{};

        // These fields invoke _reloadSettings and must be destroyed before _reloadSettings.
        // (C++ destroys members in reverse-declaration-order.)
        winrt::com_ptr<LanguageProfileNotifier> _languageProfileNotifier;
        wil::unique_folder_change_reader_nothrow _reader;

        TerminalApp::ContentManager _contentManager{ winrt::make<implementation::ContentManager>() };

        static TerminalApp::FindTargetWindowResult _doFindTargetWindow(winrt::array_view<const hstring> args,
                                                                       const Microsoft::Terminal::Settings::Model::WindowingMode& windowingBehavior);

        void _ApplyLanguageSettingChange() noexcept;
        fire_and_forget _ApplyStartupTaskStateChange();

        [[nodiscard]] HRESULT _TryLoadSettings() noexcept;
        void _ProcessLazySettingsChanges();
        void _RegisterSettingsChange();
        fire_and_forget _DispatchReloadSettings();

        void _setupFolderPathEnvVar();

#ifdef UNIT_TESTING
        friend class TerminalAppLocalTests::CommandlineTest;
#endif
    };
}

namespace winrt::TerminalApp::factory_implementation
{
    BASIC_FACTORY(AppLogic);
}
