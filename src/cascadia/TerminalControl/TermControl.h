// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TermControl.g.h"
#include "EventArgs.h"
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../buffer/out/search.h"
#include "cppwinrt_utils.h"
#include "SearchBoxControl.h"
#include "ThrottledFunc.h"

namespace Microsoft::Console::VirtualTerminal
{
    struct MouseButtonState;
}

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct TermControl : TermControlT<TermControl>
    {
        TermControl(IControlSettings settings, TerminalConnection::ITerminalConnection connection);

        winrt::fire_and_forget UpdateSettings();
        winrt::fire_and_forget UpdateAppearance(const IControlAppearance newAppearance);

        hstring Title();
        hstring GetProfileName() const;
        hstring WorkingDirectory() const;

        bool BracketedPasteEnabled() const noexcept;
        bool CopySelectionToClipboard(bool singleLine, const Windows::Foundation::IReference<CopyFormat>& formats);
        void PasteTextFromClipboard();
        void Close();
        Windows::Foundation::Size CharacterDimensions() const;
        Windows::Foundation::Size MinimumSize();
        float SnapDimensionToGrid(const bool widthOrHeight, const float dimension);

        void ScrollViewport(int viewTop);
        int GetScrollOffset();
        int GetViewHeight() const;

        void AdjustFontSize(int fontSizeDelta);
        void ResetFontSize();

        void SendInput(const winrt::hstring& input);
        void ToggleShaderEffects();

        winrt::fire_and_forget RenderEngineSwapChainChanged();
        void _AttachDxgiSwapChainToXaml(IDXGISwapChain1* swapChain);
        winrt::fire_and_forget _RendererEnteredErrorState();
        void _RenderRetryButton_Click(IInspectable const& button, IInspectable const& args);
        winrt::fire_and_forget _RendererWarning(const HRESULT hr);

        void CreateSearchBoxControl();

        void SearchMatch(const bool goForward);

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        bool OnMouseWheel(const Windows::Foundation::Point location, const int32_t delta, const bool leftButtonDown, const bool midButtonDown, const bool rightButtonDown);

        void UpdatePatternLocations();

        ~TermControl();

        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();
        ::Microsoft::Console::Types::IUiaData* GetUiaData() const;
        const FontInfo GetActualFont() const;
        const Windows::UI::Xaml::Thickness GetPadding();

        TerminalConnection::ConnectionState ConnectionState() const;
        IControlSettings Settings() const;

        static Windows::Foundation::Size GetProposedDimensions(IControlSettings const& settings, const uint32_t dpi);
        static Windows::Foundation::Size GetProposedDimensions(const winrt::Windows::Foundation::Size& initialSizeInChars,
                                                               const int32_t& fontSize,
                                                               const winrt::Windows::UI::Text::FontWeight& fontWeight,
                                                               const winrt::hstring& fontFace,
                                                               const ScrollbarState& scrollState,
                                                               const winrt::hstring& padding,
                                                               const uint32_t dpi);

        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() noexcept;

        winrt::fire_and_forget TaskbarProgressChanged();
        const size_t TaskbarState() const noexcept;
        const size_t TaskbarProgress() const noexcept;

        bool ReadOnly() const noexcept;
        void ToggleReadOnly();

        // clang-format off
        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT(FontSizeChanged,          _fontSizeChangedHandlers,           Control::FontSizeChangedEventArgs);
        DECLARE_EVENT(ScrollPositionChanged,    _scrollPositionChangedHandlers,     Control::ScrollPositionChangedEventArgs);

        TYPED_EVENT(TitleChanged,       IInspectable, Control::TitleChangedEventArgs);
        TYPED_EVENT(PasteFromClipboard, IInspectable, Control::PasteFromClipboardEventArgs);
        TYPED_EVENT(CopyToClipboard,    IInspectable, Control::CopyToClipboardEventArgs);
        TYPED_EVENT(OpenHyperlink,      IInspectable, Control::OpenHyperlinkEventArgs);
        TYPED_EVENT(SetTaskbarProgress, IInspectable, IInspectable);
        TYPED_EVENT(RaiseNotice,        IInspectable, Control::NoticeEventArgs);

        TYPED_EVENT(WarningBell, IInspectable, IInspectable);
        TYPED_EVENT(ConnectionStateChanged, IInspectable, IInspectable);
        TYPED_EVENT(Initialized, Control::TermControl, Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(TabColorChanged, IInspectable, IInspectable);
        TYPED_EVENT(HidePointerCursor, IInspectable, IInspectable);
        TYPED_EVENT(RestorePointerCursor, IInspectable, IInspectable);
        TYPED_EVENT(ReadOnlyChanged, IInspectable, IInspectable);
        TYPED_EVENT(FocusFollowMouseRequested, IInspectable, IInspectable);
        // clang-format on

        WINRT_PROPERTY(IControlAppearance, UnfocusedAppearance);

    private:
        friend struct TermControlT<TermControl>; // friend our parent so it can bind private event handlers
        TerminalConnection::ITerminalConnection _connection;
        bool _initializedTerminal;

        winrt::com_ptr<SearchBoxControl> _searchBox;

        event_token _connectionOutputEventToken;
        TerminalConnection::ITerminalConnection::StateChanged_revoker _connectionStateChangedRevoker;

        std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;

        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;
        std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine;
        std::unique_ptr<::Microsoft::Console::Render::UiaEngine> _uiaEngine;

        IControlSettings _settings;
        bool _focused;
        std::atomic<bool> _closing;

        FontInfoDesired _desiredFont;
        FontInfo _actualFont;

        std::shared_ptr<ThrottledFunc<>> _tsfTryRedrawCanvas;

        std::shared_ptr<ThrottledFunc<>> _updatePatternLocations;

        std::shared_ptr<ThrottledFunc<>> _playWarningBell;

        struct ScrollBarUpdate
        {
            std::optional<double> newValue;
            double newMaximum;
            double newMinimum;
            double newViewportSize;
        };
        std::shared_ptr<ThrottledFunc<ScrollBarUpdate>> _updateScrollBar;
        bool _isInternalScrollBarUpdate;

        unsigned int _rowsToScroll;

        // Auto scroll occurs when user, while selecting, drags cursor outside viewport. View is then scrolled to 'follow' the cursor.
        double _autoScrollVelocity;
        std::optional<Windows::UI::Input::PointerPoint> _autoScrollingPointerPoint;
        Windows::UI::Xaml::DispatcherTimer _autoScrollTimer;
        std::optional<std::chrono::high_resolution_clock::time_point> _lastAutoScrollUpdateTime;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        std::optional<Windows::UI::Xaml::DispatcherTimer> _cursorTimer;
        std::optional<Windows::UI::Xaml::DispatcherTimer> _blinkTimer;

        // If this is set, then we assume we are in the middle of panning the
        //      viewport via touch input.
        std::optional<winrt::Windows::Foundation::Point> _touchAnchor;

        // Track the last cell we hovered over (used in pointerMovedHandler)
        std::optional<COORD> _lastHoveredCell;
        // Track the last hyperlink ID we hovered over
        uint16_t _lastHoveredId;

        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval;

        using Timestamp = uint64_t;

        // imported from WinUser
        // Used for PointerPoint.Timestamp Property (https://docs.microsoft.com/en-us/uwp/api/windows.ui.input.pointerpoint.timestamp#Windows_UI_Input_PointerPoint_Timestamp)
        Timestamp _multiClickTimer;
        unsigned int _multiClickCounter;
        Timestamp _lastMouseClickTimestamp;
        std::optional<winrt::Windows::Foundation::Point> _lastMouseClickPos;
        std::optional<winrt::Windows::Foundation::Point> _lastMouseClickPosNoSelection;
        std::optional<winrt::Windows::Foundation::Point> _singleClickTouchdownPos;
        // This field tracks whether the selection has changed meaningfully
        // since it was last copied. It's generally used to prevent copyOnSelect
        // from firing when the pointer _just happens_ to be released over the
        // terminal.
        bool _selectionNeedsToBeCopied;

        winrt::Windows::UI::Xaml::Controls::SwapChainPanel::LayoutUpdated_revoker _layoutUpdatedRevoker;

        void _UpdateSettingsFromUIThreadUnderLock(IControlSettings newSettings);
        void _UpdateAppearanceFromUIThreadUnderLock(IControlAppearance newAppearance);
        bool _isReadOnly{ false };

        void _ApplyUISettings(const IControlSettings&);
        void _UpdateSystemParameterSettings() noexcept;
        void _InitializeBackgroundBrush();
        winrt::fire_and_forget _BackgroundColorChanged(const til::color color);
        bool _InitializeTerminal();
        void _UpdateFont(const bool initialUpdate = false);
        void _SetFontSize(int fontSize);
        void _TappedHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::TappedRoutedEventArgs const& e);
        void _KeyDownHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void _KeyUpHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void _CharacterHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::CharacterReceivedRoutedEventArgs const& e);
        void _PointerPressedHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _PointerMovedHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _PointerReleasedHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _PointerExitedHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _MouseWheelHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _ScrollbarChangeHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);
        void _GotFocusHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void _LostFocusHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        winrt::fire_and_forget _DragDropHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::DragEventArgs const e);
        void _DragOverHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::DragEventArgs const& e);
        winrt::fire_and_forget _HyperlinkHandler(const std::wstring_view uri);

        void _CursorTimerTick(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);
        void _BlinkTimerTick(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);
        void _SetEndSelectionPointAtCursor(Windows::Foundation::Point const& cursorPosition);
        void _SendInputToConnection(const winrt::hstring& wstr);
        void _SendInputToConnection(std::wstring_view wstr);
        void _SendPastedTextToConnection(const std::wstring& wstr);
        void _SwapChainSizeChanged(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e);
        void _SwapChainScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel const& sender, Windows::Foundation::IInspectable const& args);
        void _DoResizeUnderLock(const double newWidth, const double newHeight);
        void _RefreshSizeUnderLock();
        void _TerminalWarningBell();
        void _TerminalTitleChanged(const std::wstring_view& wstr);
        void _TerminalTabColorChanged(const std::optional<til::color> color);
        void _CopyToClipboard(const std::wstring_view& wstr);
        void _TerminalScrollPositionChanged(const int viewTop, const int viewHeight, const int bufferSize);
        void _TerminalCursorPositionChanged();

        void _MouseScrollHandler(const double mouseDelta, const Windows::Foundation::Point point, const bool isLeftButtonPressed);
        void _MouseZoomHandler(const double delta);
        void _MouseTransparencyHandler(const double delta);
        bool _DoMouseWheel(const Windows::Foundation::Point point, const ::Microsoft::Terminal::Core::ControlKeyStates modifiers, const int32_t delta, const ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState state);

        bool _CapturePointer(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        bool _ReleasePointerCapture(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void _TryStartAutoScroll(Windows::UI::Input::PointerPoint const& pointerPoint, const double scrollVelocity);
        void _TryStopAutoScroll(const uint32_t pointerId);
        void _UpdateAutoScroll(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);

        static Windows::UI::Xaml::Thickness _ParseThicknessFromPadding(const hstring padding);

        void _KeyHandler(Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e, const bool keyDown);
        ::Microsoft::Terminal::Core::ControlKeyStates _GetPressedModifierKeys() const;
        bool _TryHandleKeyBinding(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers) const;
        void _ClearKeyboardState(const WORD vkey, const WORD scanCode) const noexcept;
        bool _TrySendKeyEvent(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers, const bool keyDown);
        bool _TrySendMouseEvent(Windows::UI::Input::PointerPoint const& point);
        bool _CanSendVTMouseInput();

        const COORD _GetTerminalPosition(winrt::Windows::Foundation::Point cursorPosition);
        const unsigned int _NumberOfClicks(winrt::Windows::Foundation::Point clickPos, Timestamp clickTime);
        double _GetAutoScrollSpeed(double cursorDistanceFromBorder) const;

        void _Search(const winrt::hstring& text, const bool goForward, const bool caseSensitive);
        void _CloseSearchBoxControl(const winrt::Windows::Foundation::IInspectable& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

        // TSFInputControl Handlers
        void _CompositionCompleted(winrt::hstring text);
        void _CurrentCursorPositionHandler(const IInspectable& sender, const CursorPositionEventArgs& eventArgs);
        void _FontInfoHandler(const IInspectable& sender, const FontInfoEventArgs& eventArgs);
        winrt::fire_and_forget _AsyncCloseConnection();

        winrt::fire_and_forget _RaiseReadOnlyWarning();

        void _UpdateHoveredCell(const std::optional<COORD>& terminalPosition);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(TermControl);
}
