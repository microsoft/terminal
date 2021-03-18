// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EventArgs.h"
#include "ControlCore.g.h"
#include <winrt/Microsoft.Terminal.TerminalConnection.h>
#include "../../renderer/base/Renderer.hpp"
#include "../../renderer/dx/DxRenderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../buffer/out/search.h"
#include "cppwinrt_utils.h"
#include "ThrottledFunc.h"

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct ControlCore : ControlCoreT<ControlCore>
    {
    public:
        ControlCore(IControlSettings settings,
                    TerminalConnection::ITerminalConnection connection);

        bool InitializeTerminal(const double actualWidth,
                                const double actualHeight,
                                const double compositionScaleX,
                                const double compositionScaleY);

        void UpdateSettings(const IControlSettings& settings);
        void SizeChanged(const double width, const double height);
        void ScaleChanged(const double scaleX, const double scaleY);
        float RendererScale() const;
        HANDLE GetSwapChainHandle() const;

        void AdjustFontSize(int fontSizeDelta);
        void ResetFontSize();
        FontInfo GetFont() const;

        til::color BackgroundColor() const;
        void SetBackgroundOpacity(const float opacity);

        void SendInput(const winrt::hstring& wstr);
        void PasteText(const winrt::hstring& hstr);
        bool CopySelectionToClipboard(bool singleLine, const Windows::Foundation::IReference<CopyFormat>& formats);

        void ToggleShaderEffects();
        void ResumeRendering();

        void UpdatePatternLocations();
        void UpdateHoveredCell(const std::optional<COORD>& terminalPosition);
        winrt::hstring GetHyperlink(const til::point position) const;
        winrt::hstring GetHoveredUriText() const;
        std::optional<COORD> GetHoveredCell() const;

        void SetSelectionAnchor(winrt::Windows::Foundation::Point const& position);
        void SetEndSelectionPoint(winrt::Windows::Foundation::Point const& position);

        ::Microsoft::Console::Types::IUiaData* GetUiaData() const;

        winrt::fire_and_forget _AsyncCloseConnection();
        void Close();

#pragma region ICoreState
        // existing
        Windows::Foundation::IReference<winrt::Windows::UI::Color> TabColor() noexcept;
        const size_t TaskbarState() const noexcept;
        const size_t TaskbarProgress() const noexcept;
        hstring Title();
        hstring WorkingDirectory() const;
        TerminalConnection::ConnectionState ConnectionState() const;
        // new
        int ScrollOffset();
        int ViewHeight() const;
        int BufferHeight() const;
#pragma endregion

#pragma region ITerminalInputButNotReally
        bool TrySendKeyEvent(const WORD vkey,
                             const WORD scanCode,
                             const ::Microsoft::Terminal::Core::ControlKeyStates modifiers,
                             const bool eitherWinPressed,
                             const bool keyDown);
        bool SendCharEvent(const wchar_t ch,
                           const WORD scanCode,
                           const ::Microsoft::Terminal::Core::ControlKeyStates modifiers);
        bool SendMouseEvent(const COORD viewportPos,
                            const unsigned int uiButton,
                            const ::Microsoft::Terminal::Core::ControlKeyStates states,
                            const short wheelDelta,
                            const ::Microsoft::Console::VirtualTerminal::TerminalInput::MouseButtonState state);
        void UserScrollViewport(const int viewTop);
#pragma endregion

        void BlinkAttributeTick();
        void BlinkCursor();
        bool CursorOn() const;
        void CursorOn(const bool isCursorOn);

        bool IsVtMouseModeEnabled() const;
        til::point CursorPosition() const;

        bool HasSelection() const;
        std::vector<std::wstring> SelectedText(bool trimTrailingWhitespace) const;

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

        // -------------------------------- WinRT Events ---------------------------------
        // clang-format off
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
        // clang-format on

    private:
        bool _initializedTerminal{ false };

        TerminalConnection::ITerminalConnection _connection{ nullptr };
        event_token _connectionOutputEventToken;
        TerminalConnection::ITerminalConnection::StateChanged_revoker _connectionStateChangedRevoker;

        std::unique_ptr<::Microsoft::Terminal::Core::Terminal> _terminal{ nullptr };

        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer{ nullptr };
        std::unique_ptr<::Microsoft::Console::Render::DxEngine> _renderEngine{ nullptr };

        IControlSettings _settings{ nullptr }; // ? Might be able to get away with only retrieving pieces

        FontInfoDesired _desiredFont;
        FontInfo _actualFont;

        // storage location for the leading surrogate of a utf-16 surrogate pair
        std::optional<wchar_t> _leadingSurrogate{ std::nullopt };

        bool _isReadOnly{ false }; // Probably belongs in Interactivity

        std::optional<COORD> _lastHoveredCell{ std::nullopt };
        // Track the last hyperlink ID we hovered over
        uint16_t _lastHoveredId{ 0 };

        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval{ std::nullopt };

        double _panelWidth{ 0 };
        double _panelHeight{ 0 };
        double _compositionScaleX{ 0 };
        double _compositionScaleY{ 0 };
        til::color _backgroundColor; // This is _in_ Terminal already!

        void _SetFontSize(int fontSize);
        void _UpdateFont(const bool initialUpdate = false);
        void _RefreshSizeUnderLock();
        void _DoResizeUnderLock(const double newWidth,
                                const double newHeight);

        void _SendInputToConnection(const winrt::hstring& wstr);
        void _SendInputToConnection(std::wstring_view wstr);

#pragma region TerminalCoreCallbacks
        void _TerminalCopyToClipboard(const std::wstring_view& wstr);
        void _TerminalWarningBell();
        void _TerminalTitleChanged(const std::wstring_view& wstr);
        void _TerminalTabColorChanged(const std::optional<til::color> color);
        void _TerminalBackgroundColorChanged(const COLORREF color);
        void _TerminalScrollPositionChanged(const int viewTop,
                                            const int viewHeight,
                                            const int bufferSize);
        void _TerminalCursorPositionChanged();
        void _TerminalTaskbarProgressChanged();
#pragma endregion

#pragma region RendererCallbacks
        void _RendererWarning(const HRESULT hr);
        void RenderEngineSwapChainChanged();
#pragma endregion

    public:
        ////////////////////////////////////////////////////////////////////////
        // These methods are new

        void _raiseHoveredHyperlinkChanged();
    };
}

namespace winrt::Microsoft::Terminal::Control::factory_implementation
{
    BASIC_FACTORY(ControlCore);
}
