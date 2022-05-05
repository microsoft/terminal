// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - ControlCore.h
//
// Abstract:
// - This encapsulates a `Terminal` instance, a `DxEngine` and `Renderer`, and
//   an `ITerminalConnection`. This is intended to be everything that someone
//   might need to stand up a terminal instance in a control, but without any
//   regard for how the UX works.
//
// Author:
// - Mike Griese (zadjii-msft) 01-Apr-2021

#pragma once

#include "ControlCore.g.h"
#include "ControlSettings.h"
#include "../../renderer/base/Renderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../buffer/out/search.h"

#include <til/ticket_lock.h>

namespace ControlUnitTests
{
    class ControlCoreTests;
    class ControlInteractivityTests;
};

#define RUNTIME_SETTING(type, name, setting)                      \
private:                                                          \
    std::optional<type> _runtime##name{ std::nullopt };           \
    void name(const type newValue) { _runtime##name = newValue; } \
                                                                  \
public:                                                           \
    type name() const { return til::coalesce_value(_runtime##name, setting); }

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlCore : ControlCoreT<ControlCore>
    {
    public:
        ControlCore(Control::IControlSettings settings,
                    Control::IControlAppearance unfocusedAppearance,
                    TerminalConnection::ITerminalConnection connection);
        ~ControlCore();

        bool Initialize(const double actualWidth,
                        const double actualHeight,
                        const double compositionScale);
        void EnablePainting();

        void UpdateSettings(const Control::IControlSettings& settings, const IControlAppearance& newAppearance);
        void ApplyAppearance(const bool& focused);
        Control::IControlSettings Settings() { return *_settings; };
        Control::IControlAppearance FocusedAppearance() const { return *_settings->FocusedAppearance(); };
        Control::IControlAppearance UnfocusedAppearance() const { return *_settings->UnfocusedAppearance(); };
        bool HasUnfocusedAppearance() const;

        winrt::Microsoft::Terminal::Core::Scheme ColorScheme() const noexcept;
        void ColorScheme(const winrt::Microsoft::Terminal::Core::Scheme& scheme);

        void SizeChanged(const double width, const double height);
        void ScaleChanged(const double scale);
        uint64_t SwapChainHandle() const;

        void AdjustFontSize(int fontSizeDelta);
        void ResetFontSize();
        FontInfo GetFont() const;
        til::size FontSizeInDips() const;

        winrt::Windows::Foundation::Size FontSize() const noexcept;
        winrt::hstring FontFaceName() const noexcept;
        uint16_t FontWeight() const noexcept;

        til::color BackgroundColor() const;

        void SendInput(const winrt::hstring& wstr);
        void PasteText(const winrt::hstring& hstr);
        bool CopySelectionToClipboard(bool singleLine, const Windows::Foundation::IReference<CopyFormat>& formats);

        void GotFocus();
        void LostFocus();

        void ToggleShaderEffects();
        void AdjustOpacity(const double adjustment);
        void ResumeRendering();

        void UpdatePatternLocations();
        void SetHoveredCell(Core::Point terminalPosition);
        void ClearHoveredCell();
        winrt::hstring GetHyperlink(const Core::Point position) const;
        winrt::hstring HoveredUriText() const;
        Windows::Foundation::IReference<Core::Point> HoveredCell() const;

        ::Microsoft::Console::Types::IUiaData* GetUiaData() const;

        void Close();

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

#pragma region ITerminalInput
        bool TrySendKeyEvent(const WORD vkey,
                             const WORD scanCode,
                             const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                             const bool keyDown);
        bool SendCharEvent(const wchar_t ch,
                           const WORD scanCode,
                           const ::Microsoft::Terminal::Core::ControlKeyStates modifiers);
        bool SendMouseEvent(const til::point viewportPos,
                            const unsigned int uiButton,
                            const ::Microsoft::Terminal::Core::ControlKeyStates states,
                            const short wheelDelta,
                            const ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState state);
        void UserScrollViewport(const int viewTop);

        void ClearBuffer(Control::ClearBufferType clearType);

#pragma endregion

        void BlinkAttributeTick();
        void BlinkCursor();
        bool CursorOn() const;
        void CursorOn(const bool isCursorOn);

        bool IsVtMouseModeEnabled() const;
        bool ShouldSendAlternateScroll(const unsigned int uiButton, const int32_t delta) const;
        Core::Point CursorPosition() const;

        bool HasSelection() const;
        bool CopyOnSelect() const;
        Windows::Foundation::Collections::IVector<winrt::hstring> SelectedText(bool trimTrailingWhitespace) const;
        void SetSelectionAnchor(const til::point& position);
        void SetEndSelectionPoint(const til::point& position);

        void Search(const winrt::hstring& text,
                    const bool goForward,
                    const bool caseSensitive);

        void LeftClickOnTerminal(const til::point terminalPosition,
                                 const int numberOfClicks,
                                 const bool altEnabled,
                                 const bool shiftEnabled,
                                 const bool isOnOriginalPosition,
                                 bool& selectionNeedsToBeCopied);

        void AttachUiaEngine(::Microsoft::Console::Render::IRenderEngine* const pEngine);

        bool IsInReadOnlyMode() const;
        void ToggleReadOnlyMode();

        hstring ReadEntireBuffer() const;

        static bool IsVintageOpacityAvailable() noexcept;

        void AdjustOpacity(const double opacity, const bool relative);

        void WindowVisibilityChanged(const bool showOrHide);

        // TODO:GH#1256 - When a tab can be torn out or otherwise reparented to
        // another window, this value will need a custom setter, so that we can
        // also update the connection.
        WINRT_PROPERTY(uint64_t, OwningHwnd, 0);

        RUNTIME_SETTING(double, Opacity, _settings->Opacity());
        RUNTIME_SETTING(bool, UseAcrylic, _settings->UseAcrylic());

        // -------------------------------- WinRT Events ---------------------------------
        // clang-format off
        WINRT_CALLBACK(FontSizeChanged, Control::FontSizeChangedEventArgs);

        TYPED_EVENT(CopyToClipboard,           IInspectable, Control::CopyToClipboardEventArgs);
        TYPED_EVENT(TitleChanged,              IInspectable, Control::TitleChangedEventArgs);
        TYPED_EVENT(WarningBell,               IInspectable, IInspectable);
        TYPED_EVENT(TabColorChanged,           IInspectable, IInspectable);
        TYPED_EVENT(BackgroundColorChanged,    IInspectable, IInspectable);
        TYPED_EVENT(ScrollPositionChanged,     IInspectable, Control::ScrollPositionChangedArgs);
        TYPED_EVENT(CursorPositionChanged,     IInspectable, IInspectable);
        TYPED_EVENT(TaskbarProgressChanged,    IInspectable, IInspectable);
        TYPED_EVENT(ConnectionStateChanged,    IInspectable, IInspectable);
        TYPED_EVENT(HoveredHyperlinkChanged,   IInspectable, IInspectable);
        TYPED_EVENT(RendererEnteredErrorState, IInspectable, IInspectable);
        TYPED_EVENT(SwapChainChanged,          IInspectable, IInspectable);
        TYPED_EVENT(RendererWarning,           IInspectable, Control::RendererWarningArgs);
        TYPED_EVENT(RaiseNotice,               IInspectable, Control::NoticeEventArgs);
        TYPED_EVENT(TransparencyChanged,       IInspectable, Control::TransparencyChangedEventArgs);
        TYPED_EVENT(ReceivedOutput,            IInspectable, IInspectable);
        TYPED_EVENT(FoundMatch,                IInspectable, Control::FoundResultsArgs);
        TYPED_EVENT(ShowWindowChanged,         IInspectable, Control::ShowWindowArgs);
        // clang-format on

    private:
        bool _initializedTerminal{ false };
        bool _closing{ false };

        TerminalConnection::ITerminalConnection _connection{ nullptr };
        event_token _connectionOutputEventToken;
        TerminalConnection::ITerminalConnection::StateChanged_revoker _connectionStateChangedRevoker;

        winrt::com_ptr<ControlSettings> _settings{ nullptr };

        std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal{ nullptr };

        // NOTE: _renderEngine must be ordered before _renderer.
        //
        // As _renderer has a dependency on _renderEngine (through a raw pointer)
        // we must ensure the _renderer is deallocated first.
        // (C++ class members are destroyed in reverse order.)
        std::unique_ptr<::Microsoft::Console::Render::IRenderEngine> _renderEngine{ nullptr };
        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer{ nullptr };

        FontInfoDesired _desiredFont;
        FontInfo _actualFont;
        winrt::hstring _actualFontFaceName;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate{ std::nullopt };

        std::optional<til::point> _lastHoveredCell{ std::nullopt };
        // Track the last hyperlink ID we hovered over
        uint16_t _lastHoveredId{ 0 };

        bool _isReadOnly{ false };

        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval{ std::nullopt };

        // These members represent the size of the surface that we should be
        // rendering to.
        double _panelWidth{ 0 };
        double _panelHeight{ 0 };
        double _compositionScale{ 0 };

        winrt::Windows::System::DispatcherQueue _dispatcher{ nullptr };
        std::shared_ptr<ThrottledFuncTrailing<>> _tsfTryRedrawCanvas;
        std::shared_ptr<ThrottledFuncTrailing<>> _updatePatternLocations;
        std::shared_ptr<ThrottledFuncTrailing<Control::ScrollPositionChangedArgs>> _updateScrollBar;

        winrt::fire_and_forget _asyncCloseConnection();

        bool _setFontSizeUnderLock(int fontSize);
        void _updateFont(const bool initialUpdate = false);
        void _refreshSizeUnderLock();

        void _sendInputToConnection(std::wstring_view wstr);

#pragma region TerminalCoreCallbacks
        void _terminalCopyToClipboard(std::wstring_view wstr);
        void _terminalWarningBell();
        void _terminalTitleChanged(std::wstring_view wstr);
        void _terminalTabColorChanged(const std::optional<til::color> color);
        void _terminalScrollPositionChanged(const int viewTop,
                                            const int viewHeight,
                                            const int bufferSize);
        void _terminalCursorPositionChanged();
        void _terminalTaskbarProgressChanged();
        void _terminalShowWindowChanged(bool showOrHide);
#pragma endregion

#pragma region RendererCallbacks
        void _rendererWarning(const HRESULT hr);
        void _renderEngineSwapChainChanged();
        void _rendererBackgroundColorChanged();
#pragma endregion

        void _raiseReadOnlyWarning();
        void _updateAntiAliasingMode();
        void _connectionOutputHandler(const hstring& hstr);
        void _updateHoveredCell(const std::optional<til::point> terminalPosition);
        void _setOpacity(const double opacity);

        bool _isBackgroundTransparent();

        inline bool _IsClosing() const noexcept
        {
#ifndef NDEBUG
            if (_dispatcher)
            {
                // _closing isn't atomic and may only be accessed from the main thread.
                //
                // Though, the unit tests don't actually run in TAEF's main
                // thread, so we don't care when we're running in tests.
                assert(_inUnitTests || _dispatcher.HasThreadAccess());
            }
#endif
            return _closing;
        }

        friend class ControlUnitTests::ControlCoreTests;
        friend class ControlUnitTests::ControlInteractivityTests;
        bool _inUnitTests{ false };
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(ControlCore);
}
