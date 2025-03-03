// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "SearchBoxControl.h"
#include "TermControl.g.h"
#include "../../buffer/out/search.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../tsf/Handle.h"

#include "ControlInteractivity.h"
#include "ControlSettings.h"

namespace Microsoft::Console::VirtualTerminal
{
    struct MouseButtonState;
}

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct TermControl;

    struct TsfDataProvider : ::Microsoft::Console::TSF::IDataProvider
    {
        explicit TsfDataProvider(TermControl* termControl) noexcept;
        virtual ~TsfDataProvider() = default;

        STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) noexcept override;
        ULONG STDMETHODCALLTYPE AddRef() noexcept override;
        ULONG STDMETHODCALLTYPE Release() noexcept override;

        HWND GetHwnd() override;
        RECT GetViewport() override;
        RECT GetCursorPosition() override;
        void HandleOutput(std::wstring_view text) override;
        ::Microsoft::Console::Render::Renderer* GetRenderer() override;

    private:
        ControlCore* _getCore() const noexcept;

        TermControl* _termControl = nullptr;
        HWND _hwnd = nullptr;
    };

    struct TermControl : TermControlT<TermControl>
    {
        TermControl(Control::ControlInteractivity content);

        TermControl(IControlSettings settings, Control::IControlAppearance unfocusedAppearance, TerminalConnection::ITerminalConnection connection);

        static Control::TermControl NewControlByAttachingContent(Control::ControlInteractivity content, const Microsoft::Terminal::Control::IKeyBindings& keyBindings);

        void UpdateControlSettings(Control::IControlSettings settings);
        void UpdateControlSettings(Control::IControlSettings settings, Control::IControlAppearance unfocusedAppearance);
        IControlSettings Settings() const;

        uint64_t ContentId() const;

        hstring GetProfileName() const;

        bool CopySelectionToClipboard(bool dismissSelection, bool singleLine, bool withControlSequences, const Windows::Foundation::IReference<CopyFormat>& formats);
        void PasteTextFromClipboard();
        void SelectAll();
        bool ToggleBlockSelection();
        void ToggleMarkMode();
        bool SwitchSelectionEndpoint();
        bool ExpandSelectionToWord();
        void RestoreFromPath(winrt::hstring path);
        void PersistToPath(const winrt::hstring& path) const;
        void OpenCWD();
        void Close();
        Windows::Foundation::Size CharacterDimensions() const;
        Windows::Foundation::Size MinimumSize();
        float SnapDimensionToGrid(const bool widthOrHeight, const float dimension);
        void PreviewInput(const winrt::hstring& text);

        Windows::Foundation::Point CursorPositionInDips();
        double QuickFixButtonWidth();
        double QuickFixButtonCollapsedWidth();

        void WindowVisibilityChanged(const bool showOrHide);

        void ColorSelection(Control::SelectionColor fg, Control::SelectionColor bg, Core::MatchMode matchMode);

#pragma region ICoreState
        const uint64_t TaskbarState() const noexcept;
        const uint64_t TaskbarProgress() const noexcept;

        hstring Title();
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() noexcept;
        hstring WorkingDirectory() const;

        TerminalConnection::ConnectionState ConnectionState() const;

        int ScrollOffset() const;
        int ViewHeight() const;
        int BufferHeight() const;

        bool HasSelection() const;
        bool HasMultiLineSelection() const;
        winrt::hstring SelectedText(bool trimTrailingWhitespace) const;

        bool BracketedPasteEnabled() const noexcept;

        float BackgroundOpacity() const;

        uint64_t OwningHwnd();
        void OwningHwnd(uint64_t owner);

        Windows::Foundation::Collections::IVector<Control::ScrollMark> ScrollMarks() const;
        void AddMark(const Control::ScrollMark& mark);
        void ClearMark();
        void ClearAllMarks();
        void ScrollToMark(const Control::ScrollToMarkDirection& direction);
        void SelectCommand(const bool goUp);
        void SelectOutput(const bool goUp);

        winrt::hstring CurrentWorkingDirectory() const;
#pragma endregion

        void ScrollViewport(int viewTop);

        void AdjustFontSize(float fontSizeDelta);
        void ResetFontSize();
        winrt::Windows::Foundation::Size GetFontSize() const;

        void SendInput(const winrt::hstring& input);
        void ClearBuffer(Control::ClearBufferType clearType);

        void ToggleShaderEffects();

        void RenderEngineSwapChainChanged(IInspectable sender, IInspectable args);
        void _AttachDxgiSwapChainToXaml(HANDLE swapChainHandle);
        safe_void_coroutine _RendererEnteredErrorState(IInspectable sender, IInspectable args);

        void _RenderRetryButton_Click(const IInspectable& button, const IInspectable& args);
        safe_void_coroutine _RendererWarning(IInspectable sender,
                                             Control::RendererWarningArgs args);

        void CreateSearchBoxControl();

        void SearchMatch(const bool goForward);

        bool SearchBoxEditInFocus() const;

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        bool OnMouseWheel(const Windows::Foundation::Point location, const int32_t delta, const bool leftButtonDown, const bool midButtonDown, const bool rightButtonDown);

        ~TermControl();

        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();
        const Windows::UI::Xaml::Thickness GetPadding();

        static Windows::Foundation::Size GetProposedDimensions(const IControlSettings& settings,
                                                               const uint32_t dpi,
                                                               int32_t commandlineCols,
                                                               int32_t commandlineRows);
        static Windows::Foundation::Size GetProposedDimensions(const IControlSettings& settings, const uint32_t dpi, const winrt::Windows::Foundation::Size& initialSizeInChars);

        winrt::Windows::Foundation::Size GetNewDimensions(const winrt::Windows::Foundation::Size& initialSizeInChars);

        void BellLightOn();

        bool ReadOnly() const noexcept;
        void ToggleReadOnly();
        void SetReadOnly(const bool readOnlyState);

        static Control::MouseButtonState GetPressedMouseButtons(const winrt::Windows::UI::Input::PointerPoint point);
        static unsigned int GetPointerUpdateKind(const winrt::Windows::UI::Input::PointerPoint point);
        static Windows::UI::Xaml::Thickness ParseThicknessFromPadding(const hstring padding);

        hstring ReadEntireBuffer() const;
        Control::CommandHistoryContext CommandHistory() const;
        void UpdateWinGetSuggestions(Windows::Foundation::Collections::IVector<hstring> suggestions);

        winrt::Microsoft::Terminal::Core::Scheme ColorScheme() const noexcept;
        void ColorScheme(const winrt::Microsoft::Terminal::Core::Scheme& scheme) const noexcept;

        void AdjustOpacity(const float opacity, const bool relative);

        bool RawWriteKeyEvent(const WORD vkey, const WORD scanCode, const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers, const bool keyDown);
        bool RawWriteChar(const wchar_t character, const WORD scanCode, const winrt::Microsoft::Terminal::Core::ControlKeyStates modifiers);
        void RawWriteString(const winrt::hstring& text);

        void ShowContextMenu();
        bool OpenQuickFixMenu();
        void RefreshQuickFixMenu();
        void ClearQuickFix();

        void Detach();

        TerminalConnection::ITerminalConnection Connection();
        void Connection(const TerminalConnection::ITerminalConnection& connection);

        Control::CursorDisplayState CursorVisibility() const noexcept;
        void CursorVisibility(Control::CursorDisplayState cursorVisibility);

        // -------------------------------- WinRT Events ---------------------------------
        // clang-format off
        til::property_changed_event PropertyChanged;

        til::typed_event<IInspectable, Control::OpenHyperlinkEventArgs> OpenHyperlink;
        til::typed_event<IInspectable, Control::NoticeEventArgs> RaiseNotice;
        til::typed_event<> HidePointerCursor;
        til::typed_event<> RestorePointerCursor;
        til::typed_event<> ReadOnlyChanged;
        til::typed_event<IInspectable, IInspectable> FocusFollowMouseRequested;
        til::typed_event<Control::TermControl, Windows::UI::Xaml::RoutedEventArgs> Initialized;
        til::typed_event<> WarningBell;
        til::typed_event<IInspectable, Control::KeySentEventArgs> KeySent;
        til::typed_event<IInspectable, Control::CharSentEventArgs> CharSent;
        til::typed_event<IInspectable, Control::StringSentEventArgs> StringSent;
        til::typed_event<IInspectable, Control::SearchMissingCommandEventArgs> SearchMissingCommand;
        til::typed_event<IInspectable, Control::WindowSizeChangedEventArgs> WindowSizeChanged;

        // UNDER NO CIRCUMSTANCES SHOULD YOU ADD A (PROJECTED_)FORWARDED_TYPED_EVENT HERE
        // Those attach the handler to the core directly, and will explode if
        // the core ever gets detached & reattached to another window.
        BUBBLED_FORWARDED_TYPED_EVENT(TitleChanged,             IInspectable, Control::TitleChangedEventArgs);
        BUBBLED_FORWARDED_TYPED_EVENT(TabColorChanged,          IInspectable, IInspectable);
        BUBBLED_FORWARDED_TYPED_EVENT(SetTaskbarProgress,       IInspectable, IInspectable);
        BUBBLED_FORWARDED_TYPED_EVENT(ConnectionStateChanged,   IInspectable, IInspectable);
        BUBBLED_FORWARDED_TYPED_EVENT(ShowWindowChanged,        IInspectable, Control::ShowWindowArgs);
        BUBBLED_FORWARDED_TYPED_EVENT(CloseTerminalRequested,   IInspectable, IInspectable);
        BUBBLED_FORWARDED_TYPED_EVENT(CompletionsChanged,       IInspectable, Control::CompletionsChangedEventArgs);
        BUBBLED_FORWARDED_TYPED_EVENT(RestartTerminalRequested, IInspectable, IInspectable);

        BUBBLED_FORWARDED_TYPED_EVENT(PasteFromClipboard, IInspectable, Control::PasteFromClipboardEventArgs);

        // clang-format on

        WINRT_OBSERVABLE_PROPERTY(winrt::Windows::UI::Xaml::Media::Brush, BackgroundBrush, PropertyChanged.raise, nullptr);

    private:
        friend struct TermControlT<TermControl>; // friend our parent so it can bind private event handlers
        friend struct TsfDataProvider;

        // NOTE: _uiaEngine must be ordered before _core.
        //
        // ControlCore::AttachUiaEngine receives a IRenderEngine as a raw pointer, which we own.
        // We must ensure that we first destroy the ControlCore before the UiaEngine instance
        // in order to safely resolve this unsafe pointer dependency. Otherwise a deallocated
        // IRenderEngine is accessed when ControlCore calls Renderer::TriggerTeardown.
        // (C++ class members are destroyed in reverse order.)
        // Further, the TermControlAutomationPeer must be destructed after _uiaEngine!
        Control::TermControlAutomationPeer _automationPeer{ nullptr };
        Control::ControlInteractivity _interactivity{ nullptr };
        Control::ControlCore _core{ nullptr };
        TsfDataProvider _tsfDataProvider{ this };
        winrt::com_ptr<SearchBoxControl> _searchBox;

        enum class AltNumpadEncoding
        {
            OEM,
            ANSI,
            Unicode,
        };
        struct AltNumpadState
        {
            struct CachedKey
            {
                WORD vkey;
                WORD scanCode;
                ::Microsoft::Terminal::Core::ControlKeyStates modifiers;
                bool keyDown;
            };
            AltNumpadEncoding encoding = AltNumpadEncoding::OEM;
            uint32_t accumulator = 0;
            // Checking for accumulator != 0 to see if we have an ongoing Alt+Numpad composition is insufficient.
            // The state can be active, while the accumulator is 0, if the user pressed Alt+Numpad0 which enabled
            // the OEM encoding mode (= active), and then pressed Numpad0 again (= accumulator is still 0).
            bool active = false;
            til::small_vector<CachedKey, 4> cachedKeyEvents;
        };
        AltNumpadState _altNumpadState;

        bool _closing{ false };
        bool _focused{ false };
        bool _initializedTerminal{ false };
        bool _quickFixButtonCollapsible{ false };
        bool _quickFixesAvailable{ false };
        til::CoordType _quickFixBufferPos{};

        std::shared_ptr<ThrottledFuncLeading> _playWarningBell;

        struct ScrollBarUpdate
        {
            std::optional<double> newValue;
            double newMaximum;
            double newMinimum;
            double newViewportSize;
        };

        std::shared_ptr<ThrottledFuncTrailing<ScrollBarUpdate>> _updateScrollBar;

        bool _isInternalScrollBarUpdate;

        // Auto scroll occurs when user, while selecting, drags cursor outside
        // viewport. View is then scrolled to 'follow' the cursor.
        double _autoScrollVelocity;
        std::optional<Windows::UI::Input::PointerPoint> _autoScrollingPointerPoint;
        SafeDispatcherTimer _autoScrollTimer;
        std::optional<std::chrono::high_resolution_clock::time_point> _lastAutoScrollUpdateTime;
        bool _pointerPressedInBounds{ false };

        winrt::Windows::UI::Composition::ScalarKeyFrameAnimation _bellLightAnimation{ nullptr };
        winrt::Windows::UI::Composition::ScalarKeyFrameAnimation _bellDarkAnimation{ nullptr };
        SafeDispatcherTimer _bellLightTimer;

        SafeDispatcherTimer _cursorTimer;
        SafeDispatcherTimer _blinkTimer;

        winrt::Windows::UI::Xaml::Controls::SwapChainPanel::LayoutUpdated_revoker _layoutUpdatedRevoker;
        winrt::hstring _restorePath;
        bool _showMarksInScrollbar{ false };

        bool _isBackgroundLight{ false };
        bool _detached{ false };
        til::CoordType _searchScrollOffset = 0;

        Windows::Foundation::Collections::IObservableVector<Windows::UI::Xaml::Controls::ICommandBarElement> _originalPrimaryElements{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Windows::UI::Xaml::Controls::ICommandBarElement> _originalSecondaryElements{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Windows::UI::Xaml::Controls::ICommandBarElement> _originalSelectedPrimaryElements{ nullptr };
        Windows::Foundation::Collections::IObservableVector<Windows::UI::Xaml::Controls::ICommandBarElement> _originalSelectedSecondaryElements{ nullptr };

        Control::CursorDisplayState _cursorVisibility{ Control::CursorDisplayState::Default };

        inline bool _IsClosing() const noexcept
        {
#ifndef NDEBUG
            // _closing isn't atomic and may only be accessed from the main thread.
            if (const auto dispatcher = Dispatcher())
            {
                assert(dispatcher.HasThreadAccess());
            }
#endif
            return _closing;
        }

        void _initializeForAttach(const Microsoft::Terminal::Control::IKeyBindings& keyBindings);

        void _UpdateSettingsFromUIThread();
        void _UpdateAppearanceFromUIThread(Control::IControlAppearance newAppearance);

        void _ApplyUISettings();
        void UpdateAppearance(Control::IControlAppearance newAppearance);
        void _SetBackgroundImage(const IControlAppearance& newAppearance);

        void _InitializeBackgroundBrush();
        safe_void_coroutine _coreBackgroundColorChanged(const IInspectable& sender, const IInspectable& args);
        void _changeBackgroundColor(til::color bg);
        static bool _isColorLight(til::color bg) noexcept;
        void _changeBackgroundOpacity();

        enum InitializeReason : bool
        {
            Create,
            Reattach
        };
        bool _InitializeTerminal(const InitializeReason reason);
        safe_void_coroutine _restoreInBackground();
        void _SetFontSize(int fontSize);
        void _TappedHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::TappedRoutedEventArgs& e);
        void _KeyDownHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _KeyUpHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e);
        void _CharacterHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::CharacterReceivedRoutedEventArgs& e);
        void _PointerPressedHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _PointerMovedHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _PointerReleasedHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _PointerExitedHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _MouseWheelHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _ScrollbarChangeHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs& e);

        void _QuickFixButton_PointerEntered(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        void _QuickFixButton_PointerExited(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);

        void _GotFocusHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void _LostFocusHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);

        safe_void_coroutine _DragDropHandler(Windows::Foundation::IInspectable sender, Windows::UI::Xaml::DragEventArgs e);
        void _DragOverHandler(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::DragEventArgs& e);

        safe_void_coroutine _HyperlinkHandler(Windows::Foundation::IInspectable sender, Control::OpenHyperlinkEventArgs e);

        void _CursorTimerTick(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& e);
        void _BlinkTimerTick(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& e);
        void _BellLightOff(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& e);

        void _SetEndSelectionPointAtCursor(const Windows::Foundation::Point& cursorPosition);

        void _SwapChainSizeChanged(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::SizeChangedEventArgs& e);
        void _SwapChainScaleChanged(const Windows::UI::Xaml::Controls::SwapChainPanel& sender, const Windows::Foundation::IInspectable& args);

        void _TerminalTabColorChanged(const std::optional<til::color> color);

        void _ScrollPositionChanged(const IInspectable& sender, const Control::ScrollPositionChangedArgs& args);

        bool _CapturePointer(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);
        bool _ReleasePointerCapture(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& e);

        void _TryStartAutoScroll(const Windows::UI::Input::PointerPoint& pointerPoint, const double scrollVelocity);
        void _TryStopAutoScroll(const uint32_t pointerId);
        void _UpdateAutoScroll(const Windows::Foundation::IInspectable& sender, const Windows::Foundation::IInspectable& e);

        void _KeyHandler(const Windows::UI::Xaml::Input::KeyRoutedEventArgs& e, const bool keyDown);
        bool _KeyHandler(WORD vkey, WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers, bool keyDown);
        static ::Microsoft::Terminal::Core::ControlKeyStates _GetPressedModifierKeys() noexcept;
        bool _TryHandleKeyBinding(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers) const;
        static void _ClearKeyboardState(const WORD vkey, const WORD scanCode) noexcept;
        bool _TrySendKeyEvent(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers, const bool keyDown);

        winrt::Windows::Foundation::Point _toControlOrigin(const til::point terminalPosition);
        Core::Point _toTerminalOrigin(winrt::Windows::Foundation::Point cursorPosition);

        double _GetAutoScrollSpeed(double cursorDistanceFromBorder) const;

        void _Search(const winrt::hstring& text, const bool goForward, const bool caseSensitive, const bool regularExpression);
        void _SearchChanged(const winrt::hstring& text, const bool goForward, const bool caseSensitive, const bool regularExpression);
        void _CloseSearchBoxControl(const winrt::Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& args);
        void _refreshSearch();
        void _handleSearchResults(SearchResults results);

        void _hoveredHyperlinkChanged(const IInspectable& sender, const IInspectable& args);
        safe_void_coroutine _updateSelectionMarkers(IInspectable sender, Control::UpdateSelectionMarkersEventArgs args);

        void _coreFontSizeChanged(const IInspectable& s, const Control::FontSizeChangedArgs& args);
        void _coreTransparencyChanged(IInspectable sender, Control::TransparencyChangedEventArgs args);
        void _coreRaisedNotice(const IInspectable& s, const Control::NoticeEventArgs& args);
        void _coreWarningBell(const IInspectable& sender, const IInspectable& args);
        void _coreOutputIdle(const IInspectable& sender, const IInspectable& args);

        winrt::Windows::Foundation::Point _toPosInDips(const Core::Point terminalCellPos);
        void _throttledUpdateScrollbar(const ScrollBarUpdate& update);

        void _pasteTextWithBroadcast(const winrt::hstring& text);

        void _contextMenuHandler(IInspectable sender, Control::ContextMenuRequestedEventArgs args);
        void _showContextMenuAt(const winrt::Windows::Foundation::Point& controlRelativePos);

        void _bubbleSearchMissingCommand(const IInspectable& sender, const Control::SearchMissingCommandEventArgs& args);
        winrt::fire_and_forget _bubbleWindowSizeChanged(const IInspectable& sender, Control::WindowSizeChangedEventArgs args);
        til::CoordType _calculateSearchScrollOffset() const;

        void _PasteCommandHandler(const IInspectable& sender, const IInspectable& args);
        void _CopyCommandHandler(const IInspectable& sender, const IInspectable& args);
        void _SearchCommandHandler(const IInspectable& sender, const IInspectable& args);

        void _SelectCommandHandler(const IInspectable& sender, const IInspectable& args);
        void _SelectOutputHandler(const IInspectable& sender, const IInspectable& args);
        bool _displayCursorWhileBlurred() const noexcept;

        struct Revokers
        {
            Control::ControlCore::ScrollPositionChanged_revoker coreScrollPositionChanged;
            Control::ControlCore::WarningBell_revoker WarningBell;
            Control::ControlCore::RendererEnteredErrorState_revoker RendererEnteredErrorState;
            Control::ControlCore::BackgroundColorChanged_revoker BackgroundColorChanged;
            Control::ControlCore::FontSizeChanged_revoker FontSizeChanged;
            Control::ControlCore::TransparencyChanged_revoker TransparencyChanged;
            Control::ControlCore::RaiseNotice_revoker RaiseNotice;
            Control::ControlCore::HoveredHyperlinkChanged_revoker HoveredHyperlinkChanged;
            Control::ControlCore::OutputIdle_revoker OutputIdle;
            Control::ControlCore::UpdateSelectionMarkers_revoker UpdateSelectionMarkers;
            Control::ControlCore::OpenHyperlink_revoker coreOpenHyperlink;
            Control::ControlCore::TitleChanged_revoker TitleChanged;
            Control::ControlCore::TabColorChanged_revoker TabColorChanged;
            Control::ControlCore::TaskbarProgressChanged_revoker TaskbarProgressChanged;
            Control::ControlCore::ConnectionStateChanged_revoker ConnectionStateChanged;
            Control::ControlCore::ShowWindowChanged_revoker ShowWindowChanged;
            Control::ControlCore::CloseTerminalRequested_revoker CloseTerminalRequested;
            Control::ControlCore::CompletionsChanged_revoker CompletionsChanged;
            Control::ControlCore::RestartTerminalRequested_revoker RestartTerminalRequested;
            Control::ControlCore::SearchMissingCommand_revoker SearchMissingCommand;
            Control::ControlCore::RefreshQuickFixUI_revoker RefreshQuickFixUI;
            Control::ControlCore::WindowSizeChanged_revoker WindowSizeChanged;

            // These are set up in _InitializeTerminal
            Control::ControlCore::RendererWarning_revoker RendererWarning;
            Control::ControlCore::SwapChainChanged_revoker SwapChainChanged;

            Control::ControlInteractivity::OpenHyperlink_revoker interactivityOpenHyperlink;
            Control::ControlInteractivity::ScrollPositionChanged_revoker interactivityScrollPositionChanged;
            Control::ControlInteractivity::PasteFromClipboard_revoker PasteFromClipboard;
            Control::ControlInteractivity::ContextMenuRequested_revoker ContextMenuRequested;
        } _revokers{};
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(TermControl);
}
