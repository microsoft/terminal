// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

 once

 "AppLogic.g.h"
 "FindTargetWindowResult.g.h"
 "TerminalPage.h"
 "Jumplist.h"
 "../../cascadia/inc/cppwinrt_utils.h"

 UNIT_TESTING
// unittest classes
 TerminalAppLocalTests
{
    CommandlineTest;
};


  winrt TerminalApp implementation
{
     FindTargetWindowResult  FindTargetWindowResultT FindTargetWindowResult
    {
        WINRT_PROPERTY(int32_t, WindowId, -1);
        WINRT_PROPERTY(winrt::hstring, WindowName, L"");

  
        FindTargetWindowResult(const int32_t id, const winrt::hstring& name) :
            _WindowId{ id }, _WindowName{ name } {};

        FindTargetWindowResult(const int32_t id) :
            FindTargetWindowResult(id, L""){};
    };

    
    AppLogic : AppLogicT<AppLogic, IInitializeWithWindow>
    {
    
         AppLogic* Current() noexcept;
          Microsoft Terminal Settings Model CascadiaSettings CurrentAppSettings();

        AppLogic();
        ~AppLogic() default;

        STDMETHODIMP Initialize(HWND hwnd);

         Create();
         IsUwp()  ;
         RunAsUwp();
         IsElevated()  ;
         LoadSettings();
        [[nodiscard]] Microsoft Terminal Settings Model CascadiaSettings GetSettings() ;

        int32_t SetStartupCommandline(array_view winrt::hstring actions);
        int32_t ExecuteCommandline(array_view winrt::hstring actions, const winrt::hstring& cwd);
        TerminalApp::FindTargetWindowResult FindTargetWindow(array_view<const winrt::hstring> actions);
        winrt::hstring ParseCommandlineMessage();
         ShouldExitEarly();

        FocusMode() ;
        Fullscreen() ;
         AlwaysOnTop() ;

        IdentifyWindow();
        RenameFailed();
        winrt hstring WindowName();
        WindowName( winrt hstring& name);
        uint64_t WindowId();
        WindowId(uint64_t& id);
         IsQuakeWindow()  noexcept;

        Windows Foundation GetLaunchDimensions(uint32_ dpi);
         CenterOnLaunch();
        TerminalApp InitialPosition GetInitialPosition(int64_t defaultInitialX, int64_t defaultInitialY);
        winrt Windows UI Xaml ElementTheme GetRequestedTheme();
        Microsoft Terminal Settings Model LaunchMode GetLaunchMode();
         GetShowTabsInTitlebar();
         GetInitialAlwaysOnTop();
         CalcSnappedDimension( widthOrHeight, dimension) ;

        Windows UI Xaml UIElement GetRoot() ;

         SetInboundListener();

        hstring Title();
         TitlebarClicked();
         OnDirectKeyEvent( uint32_t vkey,  uint8_t scanCode,  down);

          WindowCloseButtonClicked();

        size_t GetLastActiveControlTaskbarState();
        size_t GetLastActiveControlTaskbarProgress();

        winrt Windows Foundation IAsyncOperationwinrt Windows UI Xaml Controls ContentDialogResult ShowDialog(winrt Windows UI Xaml Controls ContentDialog dialog);

        Windows Foundation Collections IMapView Microsoft Terminal Control KeyChord, Microsoft Terminal Settings Model Command GlobalHotkeys();

        // -------------------------------- WinRT Events ---------------------------------
        TYPED_EVENT(RequestedThemeChanged, winrt Windows Foundation IInspectable, winrt Windows UI Xaml ElementTheme);
        TYPED_EVENT(SettingsChanged, winrt Windows Foundation IInspectable, winrt Windows Foundation IInspectable);

    
         _isUwp{ false };
         _isElevated{ false };

        // you add controls here, but forget to null them either here or in
        // the ctor, you're going to have a bad time. It'll mysteriously fail to
        // activate the AppLogic.
        // ALSO: If you add any UIElements as roots here, make sure they're
        // updated in _ApplyTheme. The root currently is _root.
        winrt::com_ptr<TerminalPage> _root{ nullptr };

        Microsoft::Terminal::Settings::Model::CascadiaSettings _settings{ nullptr };

        HRESULT _settingsLoadedResult;
        winrt::hstring _settingsLoadExceptionText{};

        bool _loadedInitialSettings;

        wil::unique_folder_change_reader_nothrow _reader;

        std::shared_mutex _dialogLock;

        std::atomic<bool> _settingsReloadQueued{ false };

        ::TerminalApp::AppCommandlineArgs _appArgs;
        ::TerminalApp::AppCommandlineArgs _settingsAppArgs;
        _ParseArgs(winrt::array_view<const hstring>& args);
         TerminalApp::FindTargetWindowResult _doFindTargetWindow(winrt::array_view<const hstring> args,
                                                                       const Microsoft::Terminal::Settings::Model::WindowingMode& windowingBehavior);

         _ShowLoadErrorsDialog(const winrt::hstring& titleKey, const winrt::hstring& contentKey, HRESULT settingsLoadedResult);
        _ShowLoadWarningsDialog();
        _IsKeyboardServiceEnabled();
        _ShowKeyboardServiceDisabledDialog();

        fire_and_forget _LoadErrorsDialogRoutine();
        fire_and_forget _ShowLoadWarningsDialogRoutine();
        fire_and_forget _RefreshThemeRoutine();
        fire_and_forget _ApplyStartupTaskStateChange();

        void _OnLoaded(const IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& eventArgs);

        [[nodiscard]] HRESULT _TryLoadSettings() noexcept;
        void _RegisterSettingsChange();
        fire_and_forget _DispatchReloadSettings();
        void _ReloadSettings();

        void _ApplyTheme(const Windows::UI::Xaml::ElementTheme& newTheme);

        bool _hasCommandLineArguments{ false };
        bool _hasSettingsStartupActions{ false };
        std::vector<Microsoft::Terminal::Settings::Model::SettingsLoadWarnings> _warnings;

        // These are events that are handled by the TerminalPage, but are
        // exposed through the AppLogic. This macro is used to forward the event
        // directly to them.
        FORWARDED_TYPED_EVENT(SetTitleBarContent, winrt::Windows::Foundation::IInspectable, winrt::Windows::UI::Xaml::UIElement, _root, SetTitleBarContent);
        FORWARDED_TYPED_EVENT(TitleChanged, winrt::Windows::Foundation::IInspectable, winrt::hstring, _root, TitleChanged);
        FORWARDED_TYPED_EVENT(LastTabClosed, winrt::Windows::Foundation::IInspectable, winrt::TerminalApp::LastTabClosedEventArgs, _root, LastTabClosed);
        FORWARDED_TYPED_EVENT(FocusModeChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, FocusModeChanged);
        FORWARDED_TYPED_EVENT(FullscreenChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, FullscreenChanged);
        FORWARDED_TYPED_EVENT(AlwaysOnTopChanged, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, AlwaysOnTopChanged);
        FORWARDED_TYPED_EVENT(RaiseVisualBell, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, RaiseVisualBell);
        FORWARDED_TYPED_EVENT(SetTaskbarProgress, winrt::Windows::Foundation::IInspectable, winrt::Windows::Foundation::IInspectable, _root, SetTaskbarProgress);
        FORWARDED_TYPED_EVENT(IdentifyWindowsRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, IdentifyWindowsRequested);
        FORWARDED_TYPED_EVENT(RenameWindowRequested, Windows::Foundation::IInspectable, winrt::TerminalApp::RenameWindowRequestedArgs, _root, RenameWindowRequested);
        FORWARDED_TYPED_EVENT(IsQuakeWindowChanged, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, IsQuakeWindowChanged);
        FORWARDED_TYPED_EVENT(SummonWindowRequested, Windows::Foundation::IInspectable, Windows::Foundation::IInspectable, _root, SummonWindowRequested);

#ifdef UNIT_TESTING
        friend class TerminalAppLocalTests::CommandlineTest;
#endif
    };
}

 winrt::TerminalApp::factory_implementation
{
     AppLogic : AppLogicT<AppLogic, implementation::AppLogic>
    {
    };
}
