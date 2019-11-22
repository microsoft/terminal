// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TermControl.g.h"
#include "CopyToClipboardEventArgs.g.h"
#include "PasteFromClipboardEventArgs.g.h"
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include <winrt/Microsoft.Terminal.Settings.h>
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    struct CopyToClipboardEventArgs :
        public CopyToClipboardEventArgsT<CopyToClipboardEventArgs>
    {
    public:
        CopyToClipboardEventArgs(hstring text, hstring html, hstring rtf) :
            _text(text),
            _html(html),
            _rtf(rtf) {}

        hstring Text() { return _text; };
        hstring Html() { return _html; };
        hstring Rtf() { return _rtf; };

    private:
        hstring _text;
        hstring _html;
        hstring _rtf;
    };

    struct PasteFromClipboardEventArgs :
        public PasteFromClipboardEventArgsT<PasteFromClipboardEventArgs>
    {
    public:
        PasteFromClipboardEventArgs(std::function<void(std::wstring)> clipboardDataHandler) :
            m_clipboardDataHandler(clipboardDataHandler) {}

        void HandleClipboardData(hstring value)
        {
            m_clipboardDataHandler(static_cast<std::wstring>(value));
        };

    private:
        std::function<void(std::wstring)> m_clipboardDataHandler;
    };

    struct TermControl : TermControlT<TermControl>
    {
        TermControl();
        TermControl(Settings::IControlSettings settings, TerminalConnection::ITerminalConnection connection);

        void UpdateSettings(Settings::IControlSettings newSettings);

        hstring Title();

        bool CopySelectionToClipboard(bool trimTrailingWhitespace);
        void PasteTextFromClipboard();
        void Close();
        bool ShouldCloseOnExit() const noexcept;
        Windows::Foundation::Size CharacterDimensions() const;
        Windows::Foundation::Size MinimumSize() const;

        void ScrollViewport(int viewTop);
        void KeyboardScrollViewport(int viewTop);
        int GetScrollOffset();
        int GetViewHeight() const;

        void AdjustFontSize(int fontSizeDelta);

        void SwapChainChanged();
        ~TermControl();

        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();
        ::Microsoft::Console::Types::IUiaData* GetUiaData() const;
        const FontInfo GetActualFont() const;
        const Windows::UI::Xaml::Thickness GetPadding() const;

        static Windows::Foundation::Point GetProposedDimensions(Microsoft::Terminal::Settings::IControlSettings const& settings, const uint32_t dpi);

        // clang-format off
        // -------------------------------- WinRT Events ---------------------------------
        DECLARE_EVENT(TitleChanged,             _titleChangedHandlers,              TerminalControl::TitleChangedEventArgs);
        DECLARE_EVENT(ConnectionClosed,         _connectionClosedHandlers,          TerminalControl::ConnectionClosedEventArgs);
        DECLARE_EVENT(ScrollPositionChanged,    _scrollPositionChangedHandlers,     TerminalControl::ScrollPositionChangedEventArgs);

        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(PasteFromClipboard,  _clipboardPasteHandlers,    TerminalControl::TermControl, TerminalControl::PasteFromClipboardEventArgs);
        DECLARE_EVENT_WITH_TYPED_EVENT_HANDLER(CopyToClipboard,     _clipboardCopyHandlers,     TerminalControl::TermControl, TerminalControl::CopyToClipboardEventArgs);
        // clang-format on

    private:
        TerminalConnection::ITerminalConnection _connection;
        bool _initializedTerminal;

        Windows::UI::Xaml::Controls::Grid _root;
        Windows::UI::Xaml::Controls::Image _bgImageLayer;
        Windows::UI::Xaml::Controls::SwapChainPanel _swapChainPanel;
        Windows::UI::Xaml::Controls::Primitives::ScrollBar _scrollBar;
        TSFInputControl _tsfInputControl;

        event_token _connectionOutputEventToken;

        std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal;

        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer;
        std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine;

        Settings::IControlSettings _settings;
        bool _focused;
        std::atomic<bool> _closing;

        FontInfoDesired _desiredFont;
        FontInfo _actualFont;

        std::optional<int> _lastScrollOffset;

        // Auto scroll occurs when user, while selecting, drags cursor outside viewport. View is then scrolled to 'follow' the cursor.
        double _autoScrollVelocity;
        std::optional<Windows::UI::Input::PointerPoint> _autoScrollingPointerPoint;
        Windows::UI::Xaml::DispatcherTimer _autoScrollTimer;
        std::optional<std::chrono::high_resolution_clock::time_point> _lastAutoScrollUpdateTime;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate;

        std::optional<Windows::UI::Xaml::DispatcherTimer> _cursorTimer;

        // If this is set, then we assume we are in the middle of panning the
        //      viewport via touch input.
        std::optional<winrt::Windows::Foundation::Point> _touchAnchor;

        using Timestamp = uint64_t;

        // imported from WinUser
        // Used for PointerPoint.Timestamp Property (https://docs.microsoft.com/en-us/uwp/api/windows.ui.input.pointerpoint.timestamp#Windows_UI_Input_PointerPoint_Timestamp)
        Timestamp _multiClickTimer;
        Timestamp _lastMouseClick;
        unsigned int _multiClickCounter;
        std::optional<winrt::Windows::Foundation::Point> _lastMouseClickPos;

        // Event revokers -- we need to deregister ourselves before we die,
        // lest we get callbacks afterwards.
        winrt::Windows::UI::Xaml::Controls::Control::SizeChanged_revoker _sizeChangedRevoker;
        winrt::Windows::UI::Xaml::Controls::SwapChainPanel::CompositionScaleChanged_revoker _compositionScaleChangedRevoker;
        winrt::Windows::UI::Xaml::Controls::SwapChainPanel::LayoutUpdated_revoker _layoutUpdatedRevoker;
        winrt::Windows::UI::Xaml::UIElement::LostFocus_revoker _lostFocusRevoker;
        winrt::Windows::UI::Xaml::UIElement::GotFocus_revoker _gotFocusRevoker;

        void _Create();
        void _ApplyUISettings();
        void _InitializeBackgroundBrush();
        void _BackgroundColorChanged(const uint32_t color);
        bool _InitializeTerminal();
        void _UpdateFont();
        void _KeyDownHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);
        void _CharacterHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::CharacterReceivedRoutedEventArgs const& e);
        void _PointerPressedHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _PointerMovedHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _PointerReleasedHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _MouseWheelHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        void _ScrollbarChangeHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs const& e);
        void _GotFocusHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);
        void _LostFocusHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e);

        void _BlinkCursor(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);
        void _SetEndSelectionPointAtCursor(Windows::Foundation::Point const& cursorPosition);
        void _SendInputToConnection(const std::wstring& wstr);
        void _SendPastedTextToConnection(const std::wstring& wstr);
        void _SwapChainSizeChanged(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e);
        void _SwapChainScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel const& sender, Windows::Foundation::IInspectable const& args);
        void _DoResize(const double newWidth, const double newHeight);
        void _TerminalTitleChanged(const std::wstring_view& wstr);
        void _TerminalScrollPositionChanged(const int viewTop, const int viewHeight, const int bufferSize);

        void _MouseScrollHandler(const double delta, Windows::UI::Input::PointerPoint const& pointerPoint);
        void _MouseZoomHandler(const double delta);
        void _MouseTransparencyHandler(const double delta);

        bool _CapturePointer(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);
        bool _ReleasePointerCapture(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& e);

        void _TryStartAutoScroll(Windows::UI::Input::PointerPoint const& pointerPoint, const double scrollVelocity);
        void _TryStopAutoScroll(const uint32_t pointerId);
        void _UpdateAutoScroll(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);

        void _ScrollbarUpdater(Windows::UI::Xaml::Controls::Primitives::ScrollBar scrollbar, const int viewTop, const int viewHeight, const int bufferSize);
        static Windows::UI::Xaml::Thickness _ParseThicknessFromPadding(const hstring padding);

        ::Microsoft::Terminal::Core::ControlKeyStates _GetPressedModifierKeys() const;
        bool _TrySendKeyEvent(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers);

        const COORD _GetTerminalPosition(winrt::Windows::Foundation::Point cursorPosition);
        const unsigned int _NumberOfClicks(winrt::Windows::Foundation::Point clickPos, Timestamp clickTime);
        double _GetAutoScrollSpeed(double cursorDistanceFromBorder) const;

        // TSFInputControl Handlers
        void _CompositionCompleted(winrt::hstring text);
        void _CurrentCursorPositionHandler(const IInspectable& /*sender*/, const CursorPositionEventArgs& eventArgs);
        void _FontInfoHandler(const IInspectable& /*sender*/, const FontInfoEventArgs& eventArgs);
    };
}

namespace winrt::Microsoft::Terminal::TerminalControl::factory_implementation
{
    struct TermControl : TermControlT<TermControl, implementation::TermControl>
    {
    };
}
