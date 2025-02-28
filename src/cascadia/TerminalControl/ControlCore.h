// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.
//
// Module Name:
// - ControlCore.h
//
// Abstract:
// - This encapsulates a `Terminal` instance, a `AtlasEngine` and `Renderer`, and
//   an `ITerminalConnection`. This is intended to be everything that someone
//   might need to stand up a terminal instance in a control, but without any
//   regard for how the UX works.
//
// Author:
// - Mike Griese (zadjii-msft) 01-Apr-2021

#pragma once

#include "ControlCore.g.h"
#include "SelectionColor.g.h"
#include "CommandHistoryContext.g.h"

#include "ControlSettings.h"
#include "../../audio/midi/MidiAudio.hpp"
#include "../../buffer/out/search.h"
#include "../../cascadia/TerminalCore/Terminal.hpp"
#include "../../renderer/inc/FontInfoDesired.hpp"

namespace Microsoft::Console::Render::Atlas
{
    class AtlasEngine;
}

namespace Microsoft::Console::Render
{
    class UiaEngine;
}

namespace ControlUnitTests
{
    class ControlCoreTests;
    class ControlInteractivityTests;
};

#define RUNTIME_SETTING(type, name, setting)                 \
private:                                                     \
    std::optional<type> _runtime##name{ std::nullopt };      \
    void name(const type newValue)                           \
    {                                                        \
        _runtime##name = newValue;                           \
    }                                                        \
                                                             \
public:                                                      \
    type name() const                                        \
    {                                                        \
        return til::coalesce_value(_runtime##name, setting); \
    }

namespace winrt::Microsoft::Terminal::Control::implementation
{
    struct SelectionColor : SelectionColorT<SelectionColor>
    {
        TextColor AsTextColor() const noexcept;

        til::property<til::color> Color;
        til::property<bool> IsIndex16;
    };
    struct CommandHistoryContext : CommandHistoryContextT<CommandHistoryContext>
    {
        til::property<Windows::Foundation::Collections::IVector<winrt::hstring>> History;
        til::property<winrt::hstring> CurrentCommandline;
        til::property<Windows::Foundation::Collections::IVector<winrt::hstring>> QuickFixes;

        CommandHistoryContext(std::vector<winrt::hstring>&& history) :
            QuickFixes(winrt::single_threaded_vector<winrt::hstring>())
        {
            History(winrt::single_threaded_vector<winrt::hstring>(std::move(history)));
        }
    };

    struct ControlCore : ControlCoreT<ControlCore>
    {
    public:
        ControlCore(Control::IControlSettings settings,
                    Control::IControlAppearance unfocusedAppearance,
                    TerminalConnection::ITerminalConnection connection);
        ~ControlCore();

        bool Initialize(const float actualWidth,
                        const float actualHeight,
                        const float compositionScale);
        void EnablePainting();

        void Detach();

        void UpdateSettings(const Control::IControlSettings& settings, const IControlAppearance& newAppearance);
        void ApplyAppearance(const bool focused);
        Control::IControlSettings Settings();
        Control::IControlAppearance FocusedAppearance() const;
        Control::IControlAppearance UnfocusedAppearance() const;
        bool HasUnfocusedAppearance() const;

        winrt::Microsoft::Terminal::Core::Scheme ColorScheme() const noexcept;
        void ColorScheme(const winrt::Microsoft::Terminal::Core::Scheme& scheme);

        ::Microsoft::Console::Render::Renderer* GetRenderer() const noexcept;
        uint64_t SwapChainHandle() const;
        void AttachToNewControl(const Microsoft::Terminal::Control::IKeyBindings& keyBindings);

        void SizeChanged(const float width, const float height);
        void ScaleChanged(const float scale);
        void SizeOrScaleChanged(const float width, const float height, const float scale);

        void AdjustFontSize(float fontSizeDelta);
        void ResetFontSize();
        FontInfo GetFont() const;
        winrt::Windows::Foundation::Size FontSizeInDips() const;

        winrt::Windows::Foundation::Size FontSize() const noexcept;
        uint16_t FontWeight() const noexcept;

        til::color ForegroundColor() const;
        til::color BackgroundColor() const;

        void SendInput(std::wstring_view wstr);
        void PasteText(const winrt::hstring& hstr);
        bool CopySelectionToClipboard(bool singleLine, bool withControlSequences, const Windows::Foundation::IReference<CopyFormat>& formats);
        void SelectAll();
        void ClearSelection();
        bool ToggleBlockSelection();
        void ToggleMarkMode();
        Control::SelectionInteractionMode SelectionMode() const;
        bool SwitchSelectionEndpoint();
        bool ExpandSelectionToWord();
        bool TryMarkModeKeybinding(const WORD vkey,
                                   const ::Microsoft::Terminal::Core::ControlKeyStates modifiers);

        void GotFocus();
        void LostFocus();

        void ToggleShaderEffects();
        void AdjustOpacity(const float adjustment);
        void ResumeRendering();

        void SetHoveredCell(Core::Point terminalPosition);
        void ClearHoveredCell();
        winrt::hstring GetHyperlink(const Core::Point position) const;
        winrt::hstring HoveredUriText() const;
        Windows::Foundation::IReference<Core::Point> HoveredCell() const;

        ::Microsoft::Console::Render::IRenderData* GetRenderData() const;

        void ColorSelection(const Control::SelectionColor& fg, const Control::SelectionColor& bg, Core::MatchMode matchMode);

        void Close();
        void PersistToPath(const wchar_t* path) const;
        void RestoreFromPath(const wchar_t* path) const;

        void ClearQuickFix();

        void OpenCWD();

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

        bool HasSelection() const;
        bool HasMultiLineSelection() const;
        winrt::hstring SelectedText(bool trimTrailingWhitespace) const;

        bool BracketedPasteEnabled() const noexcept;

        Windows::Foundation::Collections::IVector<Control::ScrollMark> ScrollMarks() const;
        void AddMark(const Control::ScrollMark& mark);
        void ClearMark();
        void ClearAllMarks();
        void ScrollToMark(const Control::ScrollToMarkDirection& direction);

        void SelectCommand(const bool goUp);
        void SelectOutput(const bool goUp);

        void ContextMenuSelectCommand();
        void ContextMenuSelectOutput();

        winrt::hstring CurrentWorkingDirectory() const;
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

        bool CopyOnSelect() const;
        Control::SelectionData SelectionInfo() const;
        void SetSelectionAnchor(const til::point position);
        void SetEndSelectionPoint(const til::point position);

        SearchResults Search(SearchRequest request);
        const std::vector<til::point_span>& SearchResultRows() const noexcept;
        void ClearSearch();

        void LeftClickOnTerminal(const til::point terminalPosition,
                                 const int numberOfClicks,
                                 const bool altEnabled,
                                 const bool shiftEnabled,
                                 const bool isOnOriginalPosition,
                                 bool& selectionNeedsToBeCopied);

        void AttachUiaEngine(::Microsoft::Console::Render::UiaEngine* const pEngine);
        void DetachUiaEngine(::Microsoft::Console::Render::UiaEngine* const pEngine);

        bool IsInReadOnlyMode() const;
        void ToggleReadOnlyMode();
        void SetReadOnlyMode(const bool readOnlyState);

        hstring ReadEntireBuffer() const;
        Control::CommandHistoryContext CommandHistory() const;
        bool QuickFixesAvailable() const noexcept;
        void UpdateQuickFixes(const Windows::Foundation::Collections::IVector<hstring>& quickFixes);

        void AdjustOpacity(const float opacity, const bool relative);

        void WindowVisibilityChanged(const bool showOrHide);

        uint64_t OwningHwnd();
        void OwningHwnd(uint64_t owner);

        TerminalConnection::ITerminalConnection Connection();
        void Connection(const TerminalConnection::ITerminalConnection& connection);

        void AnchorContextMenu(til::point viewportRelativeCharacterPosition);

        bool ShouldShowSelectCommand();
        bool ShouldShowSelectOutput();

        void PreviewInput(std::wstring_view input);

        RUNTIME_SETTING(float, Opacity, _settings->Opacity());
        RUNTIME_SETTING(float, FocusedOpacity, FocusedAppearance().Opacity());
        RUNTIME_SETTING(bool, UseAcrylic, _settings->UseAcrylic());

        // -------------------------------- WinRT Events ---------------------------------
        // clang-format off
        til::typed_event<IInspectable, Control::FontSizeChangedArgs> FontSizeChanged;

        til::typed_event<IInspectable, Control::TitleChangedEventArgs> TitleChanged;
        til::typed_event<> WarningBell;
        til::typed_event<> TabColorChanged;
        til::typed_event<> BackgroundColorChanged;
        til::typed_event<IInspectable, Control::ScrollPositionChangedArgs> ScrollPositionChanged;
        til::typed_event<> TaskbarProgressChanged;
        til::typed_event<> ConnectionStateChanged;
        til::typed_event<> HoveredHyperlinkChanged;
        til::typed_event<IInspectable, IInspectable> RendererEnteredErrorState;
        til::typed_event<> SwapChainChanged;
        til::typed_event<IInspectable, Control::RendererWarningArgs> RendererWarning;
        til::typed_event<IInspectable, Control::NoticeEventArgs> RaiseNotice;
        til::typed_event<IInspectable, Control::TransparencyChangedEventArgs> TransparencyChanged;
        til::typed_event<> OutputIdle;
        til::typed_event<IInspectable, Control::ShowWindowArgs> ShowWindowChanged;
        til::typed_event<IInspectable, Control::UpdateSelectionMarkersEventArgs> UpdateSelectionMarkers;
        til::typed_event<IInspectable, Control::OpenHyperlinkEventArgs> OpenHyperlink;
        til::typed_event<IInspectable, Control::CompletionsChangedEventArgs> CompletionsChanged;
        til::typed_event<IInspectable, Control::SearchMissingCommandEventArgs> SearchMissingCommand;
        til::typed_event<> RefreshQuickFixUI;
        til::typed_event<IInspectable, Control::WindowSizeChangedEventArgs> WindowSizeChanged;

        til::typed_event<> CloseTerminalRequested;
        til::typed_event<> RestartTerminalRequested;

        til::typed_event<> Attached;
        // clang-format on

    private:
        struct SharedState
        {
            std::unique_ptr<til::debounced_func_trailing<>> outputIdle;
            std::unique_ptr<til::debounced_func_trailing<bool>> focusChanged;
            std::shared_ptr<ThrottledFuncTrailing<Control::ScrollPositionChangedArgs>> updateScrollBar;
        };

        // Caches responses generated by our VT parser (= improved batching).
        std::wstring _pendingResponses;

        // Font stuff.
        FontInfoDesired _desiredFont;
        FontInfo _actualFont;
        bool _builtinGlyphs = true;
        bool _colorGlyphs = true;
        CSSLengthPercentage _cellWidth;
        CSSLengthPercentage _cellHeight;

        // Rendering stuff.
        winrt::handle _lastSwapChainHandle{ nullptr };
        uint64_t _owningHwnd{ 0 };
        float _panelWidth{ 0 };
        float _panelHeight{ 0 };
        float _compositionScale{ 0 };

        // Audio stuff.
        MidiAudio _midiAudio;
        winrt::Windows::System::DispatcherQueueTimer _midiAudioSkipTimer{ nullptr };

        // Other stuff.
        winrt::Windows::System::DispatcherQueue _dispatcher{ nullptr };
        winrt::com_ptr<ControlSettings> _settings{ nullptr };
        til::point _contextMenuBufferPosition{ 0, 0 };
        Windows::Foundation::Collections::IVector<hstring> _cachedQuickFixes{ nullptr };
        ::Search _searcher;
        std::optional<interval_tree::IntervalTree<til::point, size_t>::interval> _lastHoveredInterval;
        std::optional<wchar_t> _leadingSurrogate;
        std::optional<til::point> _lastHoveredCell;
        uint16_t _lastHoveredId{ 0 };
        std::atomic<bool> _initializedTerminal{ false };
        bool _isReadOnly{ false };
        bool _closing{ false };

        // ----------------------------------------------------------------------------------------
        // These are ordered last to ensure they're destroyed first.
        // This ensures that their respective contents stops taking dependency on the above.
        // I recommend reading the following paragraphs in reverse order.
        // ----------------------------------------------------------------------------------------

        // ↑ This one is tricky - all of these are raw pointers:
        //   1. _terminal depends on _renderer (for invalidations)
        //   2. _renderer depends on _terminal (for IRenderData)
        //      = circular dependency = huge architectural flaw (lifetime issues) = TODO
        //   3. _renderer depends on _renderEngine (AtlasEngine)
        // To solve the knot, we manually stop the renderer in the destructor,
        // which breaks 2. We can proceed then proceed to break 1. and then 3.
        std::unique_ptr<::Microsoft::Console::Render::Atlas::AtlasEngine> _renderEngine{ nullptr }; // 3.
        std::unique_ptr<::Microsoft::Console::Render::Renderer> _renderer{ nullptr }; // 3.
        std::shared_ptr<::Microsoft::Terminal::Core::Terminal> _terminal{ nullptr }; // 1.

        // ↑ MOST IMPORTANTLY: `_shared->outputIdle` takes dependency on the raw `this` pointer
        // (necessarily), our `_renderer`, runs async, and runs often. It must be destroyed first.
        til::shared_mutex<SharedState> _shared;

        // ↑ Prevent any more unnecessary `_shared->outputIdle` calls.
        TerminalConnection::ITerminalConnection _connection{ nullptr };
        TerminalConnection::ITerminalConnection::TerminalOutput_revoker _connectionOutputEventRevoker;
        TerminalConnection::ITerminalConnection::StateChanged_revoker _connectionStateChangedRevoker;

        void _setupDispatcherAndCallbacks();

        bool _setFontSizeUnderLock(float fontSize);
        void _updateFont();
        void _refreshSizeUnderLock();
        void _updateSelectionUI();
        bool _shouldTryUpdateSelection(const WORD vkey);

        void _handleControlC();
        void _sendInputToConnection(std::wstring_view wstr);

#pragma region TerminalCoreCallbacks
        void _terminalCopyToClipboard(wil::zwstring_view wstr);
        void _terminalWarningBell();
        void _terminalTitleChanged(std::wstring_view wstr);
        void _terminalScrollPositionChanged(const int viewTop,
                                            const int viewHeight,
                                            const int bufferSize);
        void _terminalTaskbarProgressChanged();
        void _terminalShowWindowChanged(bool showOrHide);
        void _terminalPlayMidiNote(const int noteNumber,
                                   const int velocity,
                                   const std::chrono::microseconds duration);
        void _terminalSearchMissingCommand(std::wstring_view missingCommand, const til::CoordType& bufferRow);
        void _terminalWindowSizeChanged(int32_t width, int32_t height);

        safe_void_coroutine _terminalCompletionsChanged(std::wstring_view menuJson, unsigned int replaceLength);
#pragma endregion

#pragma region RendererCallbacks
        void _rendererWarning(const HRESULT hr, wil::zwstring_view parameter);
        safe_void_coroutine _renderEngineSwapChainChanged(const HANDLE handle);
        void _rendererBackgroundColorChanged();
        void _rendererTabColorChanged();
#pragma endregion

        void _raiseReadOnlyWarning();
        void _updateAntiAliasingMode();
        void _connectionOutputHandler(const hstring& hstr);
        void _connectionStateChangedHandler(const TerminalConnection::ITerminalConnection&, const Windows::Foundation::IInspectable&);
        void _updateHoveredCell(const std::optional<til::point> terminalPosition);
        void _setOpacity(const float opacity, const bool focused = true);

        bool _isBackgroundTransparent();
        void _focusChanged(bool focused);

        void _selectSpan(til::point_span s);
        void _repositionCursorWithMouse(const til::point terminalPosition);

        void _contextMenuSelectMark(
            const til::point& pos,
            bool (*filter)(const ::MarkExtents&),
            til::point_span (*getSpan)(const ::MarkExtents&));

        bool _clickedOnMark(const til::point& pos, bool (*filter)(const ::MarkExtents&));

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
    BASIC_FACTORY(SelectionColor);
}
