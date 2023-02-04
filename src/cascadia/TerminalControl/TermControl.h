// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "TermControl.g.h"
#include "XamlLights.h"
#include "EventArgs.h"
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../buffer/out/search.h"
#include "SearchBoxControl.h"

#include "ControlInteractivity.h"
#include "ControlSettings.h"

namespace Microsoft::Console::VirtualTerminal
{
    struct MouseButtonState;
}

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct TermControl : TermControlT<TermControl>
    {
        TermControl(IControlSettings settings,
                    Control::IControlAppearance unfocusedAppearance,
                    TerminalConnection::ITerminalConnection connection);

        winrt::fire_and_forget UpdateControlSettings(Control::IControlSettings settings);
        winrt::fire_and_forget UpdateControlSettings(Control::IControlSettings settings, Control::IControlAppearance unfocusedAppearance);
        IControlSettings Settings() const;

        hstring GetProfileName() const;

        bool CopySelectionToClipboard(bool singleLine, const WF::IReference<CopyFormat>& formats);
        void PasteTextFromClipboard();
        void SelectAll();
        bool ToggleBlockSelection();
        void ToggleMarkMode();
        bool SwitchSelectionEndpoint();
        bool ExpandSelectionToWord();
        void Close();
        WF::Size CharacterDimensions() const;
        WF::Size MinimumSize();
        float SnapDimensionToGrid(const bool widthOrHeight, const float dimension);

        void WindowVisibilityChanged(const bool showOrHide);

        void ColorSelection(Control::SelectionColor fg, Control::SelectionColor bg, Core::MatchMode matchMode);

#pragma region ICoreState
        const uint64_t TaskbarState() const noexcept;
        const uint64_t TaskbarProgress() const noexcept;

        hstring Title();
        WF::IReference<winrt::Windows::UI::Color> TabColor() noexcept;
        hstring WorkingDirectory() const;

        TerminalConnection::ConnectionState ConnectionState() const;

        int ScrollOffset() const;
        int ViewHeight() const;
        int BufferHeight() const;

        bool BracketedPasteEnabled() const noexcept;

        double BackgroundOpacity() const;

        uint64_t OwningHwnd();
        void OwningHwnd(uint64_t owner);

        WFC::IVector<Control::ScrollMark> ScrollMarks() const;
        void AddMark(const Control::ScrollMark& mark);
        void ClearMark();
        void ClearAllMarks();
        void ScrollToMark(const Control::ScrollToMarkDirection& direction);

#pragma endregion

        void ScrollViewport(int viewTop);

        void AdjustFontSize(float fontSizeDelta);
        void ResetFontSize();
        til::point GetFontSize() const;

        void SendInput(const winrt::hstring& input);
        void ClearBuffer(Control::ClearBufferType clearType);

        void ToggleShaderEffects();

        winrt::fire_and_forget RenderEngineSwapChainChanged(IInspectable sender, IInspectable args);
        void _AttachDxgiSwapChainToXaml(HANDLE swapChainHandle);
        winrt::fire_and_forget _RendererEnteredErrorState(IInspectable sender, IInspectable args);

        void _RenderRetryButton_Click(const IInspectable& button, const IInspectable& args);
        winrt::fire_and_forget _RendererWarning(IInspectable sender,
                                                Control::RendererWarningArgs args);

        void CreateSearchBoxControl();

        void SearchMatch(const bool goForward);

        bool SearchBoxEditInFocus() const;

        bool OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down);

        bool OnMouseWheel(const WF::Point location, const int32_t delta, const bool leftButtonDown, const bool midButtonDown, const bool rightButtonDown);

        ~TermControl();

        WUX::Automation::Peers::AutomationPeer OnCreateAutomationPeer();
        const WUX::Thickness GetPadding();

        static WF::Size GetProposedDimensions(const IControlSettings& settings,
                                                               const uint32_t dpi,
                                                               int32_t commandlineCols,
                                                               int32_t commandlineRows);
        static WF::Size GetProposedDimensions(const IControlSettings& settings, const uint32_t dpi, const WF::Size& initialSizeInChars);

        void BellLightOn();

        bool ReadOnly() const noexcept;
        void ToggleReadOnly();

        static Control::MouseButtonState GetPressedMouseButtons(const winrt::Windows::UI::Input::PointerPoint point);
        static unsigned int GetPointerUpdateKind(const winrt::Windows::UI::Input::PointerPoint point);
        static WUX::Thickness ParseThicknessFromPadding(const hstring padding);

        hstring ReadEntireBuffer() const;

        MTCore::Scheme ColorScheme() const noexcept;
        void ColorScheme(const MTCore::Scheme& scheme) const noexcept;

        void AdjustOpacity(const double opacity, const bool relative);

        WINRT_CALLBACK(PropertyChanged, WUX::Data::PropertyChangedEventHandler);

        // -------------------------------- WinRT Events ---------------------------------
        // clang-format off
        WINRT_CALLBACK(FontSizeChanged, Control::FontSizeChangedEventArgs);

        PROJECTED_FORWARDED_TYPED_EVENT(CopyToClipboard,        IInspectable, Control::CopyToClipboardEventArgs, _core, CopyToClipboard);
        PROJECTED_FORWARDED_TYPED_EVENT(TitleChanged,           IInspectable, Control::TitleChangedEventArgs, _core, TitleChanged);
        PROJECTED_FORWARDED_TYPED_EVENT(TabColorChanged,        IInspectable, IInspectable, _core, TabColorChanged);
        PROJECTED_FORWARDED_TYPED_EVENT(SetTaskbarProgress,     IInspectable, IInspectable, _core, TaskbarProgressChanged);
        PROJECTED_FORWARDED_TYPED_EVENT(ConnectionStateChanged, IInspectable, IInspectable, _core, ConnectionStateChanged);
        PROJECTED_FORWARDED_TYPED_EVENT(ShowWindowChanged,      IInspectable, Control::ShowWindowArgs, _core, ShowWindowChanged);
        PROJECTED_FORWARDED_TYPED_EVENT(CloseTerminalRequested, IInspectable, IInspectable, _core, CloseTerminalRequested);

        PROJECTED_FORWARDED_TYPED_EVENT(PasteFromClipboard, IInspectable, Control::PasteFromClipboardEventArgs, _interactivity, PasteFromClipboard);

        TYPED_EVENT(OpenHyperlink,             IInspectable, Control::OpenHyperlinkEventArgs);
        TYPED_EVENT(RaiseNotice,               IInspectable, Control::NoticeEventArgs);
        TYPED_EVENT(HidePointerCursor,         IInspectable, IInspectable);
        TYPED_EVENT(RestorePointerCursor,      IInspectable, IInspectable);
        TYPED_EVENT(ReadOnlyChanged,           IInspectable, IInspectable);
        TYPED_EVENT(FocusFollowMouseRequested, IInspectable, IInspectable);
        TYPED_EVENT(Initialized,               Control::TermControl, WUX::RoutedEventArgs);
        TYPED_EVENT(WarningBell,               IInspectable, IInspectable);
        // clang-format on

        WINRT_OBSERVABLE_PROPERTY(WUXMedia::Brush, BackgroundBrush, _PropertyChangedHandlers, nullptr);

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
        Control::TermControlAutomationPeer _automationPeer{ nullptr };
        Control::ControlInteractivity _interactivity{ nullptr };
        Control::ControlCore _core{ nullptr };

        winrt::com_ptr<SearchBoxControl> _searchBox;

        bool _closing{ false };
        bool _focused{ false };
        bool _initializedTerminal{ false };

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
        WUX::DispatcherTimer _autoScrollTimer;
        std::optional<std::chrono::high_resolution_clock::time_point> _lastAutoScrollUpdateTime;
        bool _pointerPressedInBounds{ false };

        winrt::Windows::UI::Composition::ScalarKeyFrameAnimation _bellLightAnimation{ nullptr };
        winrt::Windows::UI::Composition::ScalarKeyFrameAnimation _bellDarkAnimation{ nullptr };
        WUX::DispatcherTimer _bellLightTimer{ nullptr };

        std::optional<WUX::DispatcherTimer> _cursorTimer;
        std::optional<WUX::DispatcherTimer> _blinkTimer;

        WUXC::SwapChainPanel::LayoutUpdated_revoker _layoutUpdatedRevoker;
        bool _showMarksInScrollbar{ false };

        bool _isBackgroundLight{ false };

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

        void _UpdateSettingsFromUIThread();
        void _UpdateAppearanceFromUIThread(Control::IControlAppearance newAppearance);
        void _ApplyUISettings();
        winrt::fire_and_forget UpdateAppearance(Control::IControlAppearance newAppearance);
        void _SetBackgroundImage(const IControlAppearance& newAppearance);

        void _InitializeBackgroundBrush();
        winrt::fire_and_forget _coreBackgroundColorChanged(const IInspectable& sender, const IInspectable& args);
        void _changeBackgroundColor(til::color bg);
        static bool _isColorLight(til::color bg) noexcept;
        void _changeBackgroundOpacity();

        bool _InitializeTerminal();
        void _SetFontSize(int fontSize);
        void _TappedHandler(const WF::IInspectable& sender, const WUX::Input::TappedRoutedEventArgs& e);
        void _KeyDownHandler(const WF::IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);
        void _KeyUpHandler(const WF::IInspectable& sender, const WUX::Input::KeyRoutedEventArgs& e);
        void _CharacterHandler(const WF::IInspectable& sender, const WUX::Input::CharacterReceivedRoutedEventArgs& e);
        void _PointerPressedHandler(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);
        void _PointerMovedHandler(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);
        void _PointerReleasedHandler(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);
        void _PointerExitedHandler(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);
        void _MouseWheelHandler(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);
        void _ScrollbarChangeHandler(const WF::IInspectable& sender, const WUXC::Primitives::RangeBaseValueChangedEventArgs& e);

        void _GotFocusHandler(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);
        void _LostFocusHandler(const WF::IInspectable& sender, const WUX::RoutedEventArgs& e);

        winrt::fire_and_forget _DragDropHandler(WF::IInspectable sender, WUX::DragEventArgs e);
        void _DragOverHandler(const WF::IInspectable& sender, const WUX::DragEventArgs& e);

        winrt::fire_and_forget _HyperlinkHandler(WF::IInspectable sender, Control::OpenHyperlinkEventArgs e);

        void _CursorTimerTick(const WF::IInspectable& sender, const WF::IInspectable& e);
        void _BlinkTimerTick(const WF::IInspectable& sender, const WF::IInspectable& e);
        void _BellLightOff(const WF::IInspectable& sender, const WF::IInspectable& e);

        void _SetEndSelectionPointAtCursor(const WF::Point& cursorPosition);

        void _SwapChainSizeChanged(const WF::IInspectable& sender, const WUX::SizeChangedEventArgs& e);
        void _SwapChainScaleChanged(const WUXC::SwapChainPanel& sender, const WF::IInspectable& args);

        void _TerminalTabColorChanged(const std::optional<til::color> color);

        void _ScrollPositionChanged(const IInspectable& sender, const Control::ScrollPositionChangedArgs& args);
        winrt::fire_and_forget _CursorPositionChanged(const IInspectable& sender, const IInspectable& args);

        bool _CapturePointer(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);
        bool _ReleasePointerCapture(const WF::IInspectable& sender, const WUX::Input::PointerRoutedEventArgs& e);

        void _TryStartAutoScroll(const Windows::UI::Input::PointerPoint& pointerPoint, const double scrollVelocity);
        void _TryStopAutoScroll(const uint32_t pointerId);
        void _UpdateAutoScroll(const WF::IInspectable& sender, const WF::IInspectable& e);

        void _KeyHandler(const WUX::Input::KeyRoutedEventArgs& e, const bool keyDown);
        static ::Microsoft::Terminal::Core::ControlKeyStates _GetPressedModifierKeys() noexcept;
        bool _TryHandleKeyBinding(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers) const;
        static void _ClearKeyboardState(const WORD vkey, const WORD scanCode) noexcept;
        bool _TrySendKeyEvent(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers, const bool keyDown);

        const til::point _toTerminalOrigin(WF::Point cursorPosition);
        double _GetAutoScrollSpeed(double cursorDistanceFromBorder) const;

        void _Search(const winrt::hstring& text, const bool goForward, const bool caseSensitive);
        void _CloseSearchBoxControl(const WF::IInspectable& sender, const WUX::RoutedEventArgs& args);

        // TSFInputControl Handlers
        void _CompositionCompleted(winrt::hstring text);
        void _CurrentCursorPositionHandler(const IInspectable& sender, const CursorPositionEventArgs& eventArgs);
        void _FontInfoHandler(const IInspectable& sender, const FontInfoEventArgs& eventArgs);

        winrt::fire_and_forget _hoveredHyperlinkChanged(IInspectable sender, IInspectable args);
        winrt::fire_and_forget _updateSelectionMarkers(IInspectable sender, Control::UpdateSelectionMarkersEventArgs args);

        void _coreFontSizeChanged(const int fontWidth,
                                  const int fontHeight,
                                  const bool isInitialChange);
        winrt::fire_and_forget _coreTransparencyChanged(IInspectable sender, Control::TransparencyChangedEventArgs args);
        void _coreRaisedNotice(const IInspectable& s, const Control::NoticeEventArgs& args);
        void _coreWarningBell(const IInspectable& sender, const IInspectable& args);
        void _coreFoundMatch(const IInspectable& sender, const Control::FoundResultsArgs& args);

        til::point _toPosInDips(const Core::Point terminalCellPos);
        void _throttledUpdateScrollbar(const ScrollBarUpdate& update);
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(TermControl);
}
