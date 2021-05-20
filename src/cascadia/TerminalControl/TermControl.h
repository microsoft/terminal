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

#include "ControlInteractivity.h"

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

        hstring GetProfileName() const;

        bool CopySelectionToClipboard(bool singleLine, const Windows::Foundation::IReference<CopyFormat>& formats);
        void PasteTextFromClipboard();
        void Close();
        Windows::Foundation::Size CharacterDimensions() const;
        Windows::Foundation::Size MinimumSize();
        float SnapDimensionToGrid(const bool widthOrHeight, const float dimension);

#pragma region ICoreState
        const size_t TaskbarState() const noexcept;
        const size_t TaskbarProgress() const noexcept;

        hstring Title();
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() noexcept;
        hstring WorkingDirectory() const;

        TerminalConnection::ConnectionState ConnectionState() const;

        int ScrollOffset();
        int ViewHeight() const;
        int BufferHeight() const;

        bool BracketedPasteEnabled() const noexcept;
#pragma endregion

        void ScrollViewport(int viewTop);

        void AdjustFontSize(int fontSizeDelta);
        void ResetFontSize();
        til::point GetFontSize() const;

        void SendInput(const winrt::hstring& input);
        void ToggleShaderEffects();

        winrt::fire_and_forget RenderEngineSwapChainChanged(IInspectable sender, IInspectable args);
        void _AttachDxgiSwapChainToXaml(HANDLE swapChainHandle);
        winrt::fire_and_forget _RendererEnteredErrorState(IInspectable sender, IInspectable args);

        void _RenderRetryButton_Click(IInspectable const& button, IInspectable const& args);
        winrt::fire_and_forget _RendererWarning(IInspectable sender,
                                                Control::RendererWarningArgs args);

        void CreateSearchBoxControl();

        void SearchMatch(const bool goForward);

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        bool OnMouseWheel(const Windows::Foundation::Point location, const int32_t delta, const bool leftButtonDown, const bool midButtonDown, const bool rightButtonDown);

        ~TermControl();

        Windows::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();
        ::Microsoft::Console::Types::IUiaData* GetUiaData() const;
        const Windows::UI::Xaml::Thickness GetPadding();

        IControlSettings Settings() const;
        void Settings(IControlSettings newSettings);

        static Windows::Foundation::Size GetProposedDimensions(IControlSettings const& settings, const uint32_t dpi);
        static Windows::Foundation::Size GetProposedDimensions(const winrt::Windows::Foundation::Size& initialSizeInChars,
                                                               const int32_t& fontSize,
                                                               const winrt::Windows::UI::Text::FontWeight& fontWeight,
                                                               const winrt::hstring& fontFace,
                                                               const ScrollbarState& scrollState,
                                                               const winrt::hstring& padding,
                                                               const uint32_t dpi);

        bool ReadOnly() const noexcept;
        void ToggleReadOnly();

        static ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState GetPressedMouseButtons(const winrt::Windows::UI::Input::PointerPoint point);
        static unsigned int GetPointerUpdateKind(const winrt::Windows::UI::Input::PointerPoint point);

        // -------------------------------- WinRT Events ---------------------------------
        // clang-format off
        WINRT_CALLBACK(FontSizeChanged, Control::FontSizeChangedEventArgs);

        FORWARDED_TYPED_EVENT(CopyToClipboard,        IInspectable, Control::CopyToClipboardEventArgs, _core, CopyToClipboard);
        FORWARDED_TYPED_EVENT(TitleChanged,           IInspectable, Control::TitleChangedEventArgs, _core, TitleChanged);
        FORWARDED_TYPED_EVENT(TabColorChanged,        IInspectable, IInspectable, _core, TabColorChanged);
        FORWARDED_TYPED_EVENT(SetTaskbarProgress,     IInspectable, IInspectable, _core, TaskbarProgressChanged);
        FORWARDED_TYPED_EVENT(ConnectionStateChanged, IInspectable, IInspectable, _core, ConnectionStateChanged);
        FORWARDED_TYPED_EVENT(PasteFromClipboard,     IInspectable, Control::PasteFromClipboardEventArgs, _interactivity, PasteFromClipboard);

        TYPED_EVENT(OpenHyperlink,             IInspectable, Control::OpenHyperlinkEventArgs);
        TYPED_EVENT(RaiseNotice,               IInspectable, Control::NoticeEventArgs);
        TYPED_EVENT(HidePointerCursor,         IInspectable, IInspectable);
        TYPED_EVENT(RestorePointerCursor,      IInspectable, IInspectable);
        TYPED_EVENT(ReadOnlyChanged,           IInspectable, IInspectable);
        TYPED_EVENT(FocusFollowMouseRequested, IInspectable, IInspectable);
        TYPED_EVENT(Initialized,               Control::TermControl, Windows::UI::Xaml::RoutedEventArgs);
        TYPED_EVENT(WarningBell,               IInspectable, IInspectable);
        // clang-format on

        WINRT_PROPERTY(IControlAppearance, UnfocusedAppearance);

    private:
        friend struct TermControlT<TermControl>; // friend our parent so it can bind private event handlers

        // NOTE: _uiaEngine must be ordered before _core.
        //
        // ControlCore::AttachUiaEngine receives a IRenderEngine as a raw pointer, which we own.
        // We must ensure that we first destroy the ControlCore before the UiaEngine instance
        // in order to safely resolve this unsafe pointer dependency. Otherwise a deallocated
        // IRenderEngine is accessed when ControlCore calls Renderer::TriggerTeardown.
        // (C++ class members are destroyed in reverse order.)
        // Further, the TermControlAutomationPeer must be destructed after _uiaEngine!
        winrt::Windows::UI::Xaml::Automation::Peers::AutomationPeer _automationPeer{ nullptr };
        std::unique_ptr<::Microsoft::Console::Render::UiaEngine> _uiaEngine;

        winrt::com_ptr<ControlCore> _core;
        winrt::com_ptr<ControlInteractivity> _interactivity;
        winrt::com_ptr<SearchBoxControl> _searchBox;

        IControlSettings _settings;
        std::atomic<bool> _closing;
        bool _focused;
        bool _initializedTerminal;

        std::shared_ptr<ThrottledFuncTrailing<>> _tsfTryRedrawCanvas;
        std::shared_ptr<ThrottledFuncTrailing<>> _updatePatternLocations;
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

        // Auto scroll occurs when user, while selecting, drags cursor outside viewport. View is then scrolled to 'follow' the cursor.
        double _autoScrollVelocity;
        std::optional<Windows::UI::Input::PointerPoint> _autoScrollingPointerPoint;
        Windows::UI::Xaml::DispatcherTimer _autoScrollTimer;
        std::optional<std::chrono::high_resolution_clock::time_point> _lastAutoScrollUpdateTime;

        std::optional<Windows::UI::Xaml::DispatcherTimer> _cursorTimer;
        std::optional<Windows::UI::Xaml::DispatcherTimer> _blinkTimer;

        event_token _coreOutputEventToken;

        winrt::Windows::UI::Xaml::Controls::SwapChainPanel::LayoutUpdated_revoker _layoutUpdatedRevoker;

        void _UpdateSettingsFromUIThread(IControlSettings newSettings);
        void _UpdateAppearanceFromUIThread(IControlAppearance newAppearance);
        void _ApplyUISettings(const IControlSettings&);

        void _InitializeBackgroundBrush();
        void _BackgroundColorChangedHandler(const IInspectable& sender, const IInspectable& args);
        winrt::fire_and_forget _changeBackgroundColor(const til::color bg);

        bool _InitializeTerminal();
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

        winrt::fire_and_forget _DragDropHandler(Windows::Foundation::IInspectable sender, Windows::UI::Xaml::DragEventArgs e);
        void _DragOverHandler(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::DragEventArgs const& e);

        winrt::fire_and_forget _HyperlinkHandler(Windows::Foundation::IInspectable sender, Control::OpenHyperlinkEventArgs e);

        void _CursorTimerTick(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);
        void _BlinkTimerTick(Windows::Foundation::IInspectable const& sender, Windows::Foundation::IInspectable const& e);

        void _SetEndSelectionPointAtCursor(Windows::Foundation::Point const& cursorPosition);

        void _SwapChainSizeChanged(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::SizeChangedEventArgs const& e);
        void _SwapChainScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel const& sender, Windows::Foundation::IInspectable const& args);

        void _TerminalTabColorChanged(const std::optional<til::color> color);

        void _ScrollPositionChanged(const IInspectable& sender, const Control::ScrollPositionChangedArgs& args);
        void _CursorPositionChanged(const IInspectable& sender, const IInspectable& args);

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

        const til::point _toTerminalOrigin(winrt::Windows::Foundation::Point cursorPosition);
        double _GetAutoScrollSpeed(double cursorDistanceFromBorder) const;

        void _Search(const winrt::hstring& text, const bool goForward, const bool caseSensitive);
        void _CloseSearchBoxControl(const winrt::Windows::Foundation::IInspectable& sender, Windows::UI::Xaml::RoutedEventArgs const& args);

        // TSFInputControl Handlers
        void _CompositionCompleted(winrt::hstring text);
        void _CurrentCursorPositionHandler(const IInspectable& sender, const CursorPositionEventArgs& eventArgs);
        void _FontInfoHandler(const IInspectable& sender, const FontInfoEventArgs& eventArgs);

        winrt::fire_and_forget _hoveredHyperlinkChanged(IInspectable sender, IInspectable args);

        void _coreFontSizeChanged(const int fontWidth,
                                  const int fontHeight,
                                  const bool isInitialChange);
        winrt::fire_and_forget _coreTransparencyChanged(IInspectable sender, Control::TransparencyChangedEventArgs args);
        void _coreReceivedOutput(const IInspectable& sender, const IInspectable& args);
        void _coreRaisedNotice(const IInspectable& s, const Control::NoticeEventArgs& args);
        void _coreWarningBell(const IInspectable& sender, const IInspectable& args);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(TermControl);
}
