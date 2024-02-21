// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ControlCore.h"

// MidiAudio
#include <mmeapi.h>
#include <dsound.h>

#include <DefaultSettings.h>
#include <unicode.hpp>
#include <utils.hpp>
#include <WinUser.h>
#include <LibraryResources.h>

#include "EventArgs.h"
#include "../../buffer/out/search.h"
#include "../../renderer/atlas/AtlasEngine.h"
#include "../../renderer/dx/DxRenderer.hpp"

#include "ControlCore.g.cpp"
#include "SelectionColor.g.cpp"

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Console::VirtualTerminal;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

// The minimum delay between updates to the scroll bar's values.
// The updates are throttled to limit power usage.
constexpr const auto ScrollBarUpdateInterval = std::chrono::milliseconds(8);

// The minimum delay between updating the TSF input control.
constexpr const auto TsfRedrawInterval = std::chrono::milliseconds(100);

// The minimum delay between updating the locations of regex patterns
constexpr const auto UpdatePatternLocationsInterval = std::chrono::milliseconds(500);

// The delay before performing the search after change of search criteria
constexpr const auto SearchAfterChangeDelay = std::chrono::milliseconds(200);

namespace winrt::Microsoft::Terminal::Control::implementation
{
    static winrt::Microsoft::Terminal::Core::OptionalColor OptionalFromColor(const til::color& c)
    {
        Core::OptionalColor result;
        result.Color = c;
        result.HasValue = true;
        return result;
    }
    static winrt::Microsoft::Terminal::Core::OptionalColor OptionalFromColor(const std::optional<til::color>& c)
    {
        Core::OptionalColor result;
        if (c.has_value())
        {
            result.Color = *c;
            result.HasValue = true;
        }
        else
        {
            result.HasValue = false;
        }
        return result;
    }

    TextColor SelectionColor::AsTextColor() const noexcept
    {
        if (IsIndex16())
        {
            return { Color().r, false };
        }
        else
        {
            return { static_cast<COLORREF>(Color()) };
        }
    }

    ControlCore::ControlCore(Control::IControlSettings settings,
                             Control::IControlAppearance unfocusedAppearance,
                             TerminalConnection::ITerminalConnection connection) :
        _desiredFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, DEFAULT_FONT_SIZE, CP_UTF8 },
        _actualFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false }
    {
        _settings = winrt::make_self<implementation::ControlSettings>(settings, unfocusedAppearance);
        _terminal = std::make_shared<::Microsoft::Terminal::Core::Terminal>();
        const auto lock = _terminal->LockForWriting();

        _setupDispatcherAndCallbacks();

        Connection(connection);

        _terminal->SetWriteInputCallback([this](std::wstring_view wstr) {
            _sendInputToConnection(wstr);
        });

        // GH#8969: pre-seed working directory to prevent potential races
        _terminal->SetWorkingDirectory(_settings->StartingDirectory());

        auto pfnCopyToClipboard = std::bind(&ControlCore::_terminalCopyToClipboard, this, std::placeholders::_1);
        _terminal->SetCopyToClipboardCallback(pfnCopyToClipboard);

        auto pfnWarningBell = std::bind(&ControlCore::_terminalWarningBell, this);
        _terminal->SetWarningBellCallback(pfnWarningBell);

        auto pfnTitleChanged = std::bind(&ControlCore::_terminalTitleChanged, this, std::placeholders::_1);
        _terminal->SetTitleChangedCallback(pfnTitleChanged);

        auto pfnScrollPositionChanged = std::bind(&ControlCore::_terminalScrollPositionChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        _terminal->SetScrollPositionChangedCallback(pfnScrollPositionChanged);

        auto pfnTerminalCursorPositionChanged = std::bind(&ControlCore::_terminalCursorPositionChanged, this);
        _terminal->SetCursorPositionChangedCallback(pfnTerminalCursorPositionChanged);

        auto pfnTerminalTaskbarProgressChanged = std::bind(&ControlCore::_terminalTaskbarProgressChanged, this);
        _terminal->TaskbarProgressChangedCallback(pfnTerminalTaskbarProgressChanged);

        auto pfnShowWindowChanged = std::bind(&ControlCore::_terminalShowWindowChanged, this, std::placeholders::_1);
        _terminal->SetShowWindowCallback(pfnShowWindowChanged);

        auto pfnPlayMidiNote = std::bind(&ControlCore::_terminalPlayMidiNote, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        _terminal->SetPlayMidiNoteCallback(pfnPlayMidiNote);

        auto pfnCompletionsChanged = [=](auto&& menuJson, auto&& replaceLength) { _terminalCompletionsChanged(menuJson, replaceLength); };
        _terminal->CompletionsChangedCallback(pfnCompletionsChanged);

        // MSFT 33353327: Initialize the renderer in the ctor instead of Initialize().
        // We need the renderer to be ready to accept new engines before the SwapChainPanel is ready to go.
        // If we wait, a screen reader may try to get the AutomationPeer (aka the UIA Engine), and we won't be able to attach
        // the UIA Engine to the renderer. This prevents us from signaling changes to the cursor or buffer.
        {
            // First create the render thread.
            // Then stash a local pointer to the render thread so we can initialize it and enable it
            // to paint itself *after* we hand off its ownership to the renderer.
            // We split up construction and initialization of the render thread object this way
            // because the renderer and render thread have circular references to each other.
            auto renderThread = std::make_unique<::Microsoft::Console::Render::RenderThread>();
            auto* const localPointerToThread = renderThread.get();

            // Now create the renderer and initialize the render thread.
            const auto& renderSettings = _terminal->GetRenderSettings();
            _renderer = std::make_unique<::Microsoft::Console::Render::Renderer>(renderSettings, _terminal.get(), nullptr, 0, std::move(renderThread));

            _renderer->SetBackgroundColorChangedCallback([this]() { _rendererBackgroundColorChanged(); });
            _renderer->SetFrameColorChangedCallback([this]() { _rendererTabColorChanged(); });
            _renderer->SetRendererEnteredErrorStateCallback([this]() { _RendererEnteredErrorStateHandlers(nullptr, nullptr); });

            THROW_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));
        }

        UpdateSettings(settings, unfocusedAppearance);
    }

    void ControlCore::_setupDispatcherAndCallbacks()
    {
        // Get our dispatcher. If we're hosted in-proc with XAML, this will get
        // us the same dispatcher as TermControl::Dispatcher(). If we're out of
        // proc, this'll return null. We'll need to instead make a new
        // DispatcherQueue (on a new thread), so we can use that for throttled
        // functions.
        _dispatcher = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();
        if (!_dispatcher)
        {
            auto controller{ winrt::Windows::System::DispatcherQueueController::CreateOnDedicatedThread() };
            _dispatcher = controller.DispatcherQueue();
        }

        // A few different events should be throttled, so they don't fire absolutely all the time:
        // * _tsfTryRedrawCanvas: When the cursor position moves, we need to
        //   inform TSF, so it can move the canvas for the composition. We
        //   throttle this so that we're not hopping across the process boundary
        //   every time that the cursor moves.
        // * _updatePatternLocations: When there's new output, or we scroll the
        //   viewport, we should re-check if there are any visible hyperlinks.
        //   But we don't really need to do this every single time text is
        //   output, we can limit this update to once every 500ms.
        // * _updateScrollBar: Same idea as the TSF update - we don't _really_
        //   need to hop across the process boundary every time text is output.
        //   We can throttle this to once every 8ms, which will get us out of
        //   the way of the main output & rendering threads.
        const auto shared = _shared.lock();
        shared->tsfTryRedrawCanvas = std::make_shared<ThrottledFuncTrailing<>>(
            _dispatcher,
            TsfRedrawInterval,
            [weakThis = get_weak()]() {
                if (auto core{ weakThis.get() }; !core->_IsClosing())
                {
                    core->_CursorPositionChangedHandlers(*core, nullptr);
                }
            });

        // NOTE: Calling UpdatePatternLocations from a background
        // thread is a workaround for us to hit GH#12607 less often.
        shared->updatePatternLocations = std::make_unique<til::throttled_func_trailing<>>(
            UpdatePatternLocationsInterval,
            [weakTerminal = std::weak_ptr{ _terminal }]() {
                if (const auto t = weakTerminal.lock())
                {
                    const auto lock = t->LockForWriting();
                    t->UpdatePatternsUnderLock();
                }
            });

        shared->updateScrollBar = std::make_shared<ThrottledFuncTrailing<Control::ScrollPositionChangedArgs>>(
            _dispatcher,
            ScrollBarUpdateInterval,
            [weakThis = get_weak()](const auto& update) {
                if (auto core{ weakThis.get() }; !core->_IsClosing())
                {
                    core->_ScrollPositionChangedHandlers(*core, update);
                }
            });
    }

    ControlCore::~ControlCore()
    {
        Close();

        if (_renderer)
        {
            _renderer->TriggerTeardown();
        }
    }

    void ControlCore::Detach()
    {
        // Disable the renderer, so that it doesn't try to start any new frames
        // for our engines while we're not attached to anything.
        _renderer->WaitForPaintCompletionAndDisable(INFINITE);

        // Clear out any throttled funcs that we had wired up to run on this UI
        // thread. These will be recreated in _setupDispatcherAndCallbacks, when
        // we're re-attached to a new control (on a possibly new UI thread).
        const auto shared = _shared.lock();
        shared->tsfTryRedrawCanvas.reset();
        shared->updatePatternLocations.reset();
        shared->updateScrollBar.reset();
    }

    void ControlCore::AttachToNewControl(const Microsoft::Terminal::Control::IKeyBindings& keyBindings)
    {
        _settings->KeyBindings(keyBindings);
        _setupDispatcherAndCallbacks();
        const auto actualNewSize = _actualFont.GetSize();
        // Bubble this up, so our new control knows how big we want the font.
        _FontSizeChangedHandlers(*this, winrt::make<FontSizeChangedArgs>(actualNewSize.width, actualNewSize.height));

        // The renderer will be re-enabled in Initialize

        _AttachedHandlers(*this, nullptr);
    }

    TerminalConnection::ITerminalConnection ControlCore::Connection()
    {
        return _connection;
    }

    // Method Description:
    // - Setup our event handlers for this connection. If we've currently got a
    //   connection, then this'll revoke the existing connection's handlers.
    // - This will not call Start on the incoming connection. The caller should do that.
    // - If the caller doesn't want the old connection to be closed, then they
    //   should grab a reference to it before calling this (so that it doesn't
    //   destruct, and close) during this call.
    void ControlCore::Connection(const TerminalConnection::ITerminalConnection& newConnection)
    {
        auto oldState = ConnectionState(); // rely on ControlCore's automatic null handling
        // revoke ALL old handlers immediately

        _connectionOutputEventRevoker.revoke();
        _connectionStateChangedRevoker.revoke();

        _connection = newConnection;
        if (_connection)
        {
            // Subscribe to the connection's disconnected event and call our connection closed handlers.
            _connectionStateChangedRevoker = newConnection.StateChanged(winrt::auto_revoke, [this](auto&& /*s*/, auto&& /*v*/) {
                _ConnectionStateChangedHandlers(*this, nullptr);
            });

            // Get our current size in rows/cols, and hook them up to
            // this connection too.
            {
                const auto lock = _terminal->LockForReading();
                const auto vp = _terminal->GetViewport();
                const auto width = vp.Width();
                const auto height = vp.Height();

                newConnection.Resize(height, width);
            }
            // Window owner too.
            if (auto conpty{ newConnection.try_as<TerminalConnection::ConptyConnection>() })
            {
                conpty.ReparentWindow(_owningHwnd);
            }

            // This event is explicitly revoked in the destructor: does not need weak_ref
            _connectionOutputEventRevoker = _connection.TerminalOutput(winrt::auto_revoke, { this, &ControlCore::_connectionOutputHandler });
        }

        // Fire off a connection state changed notification, to let our hosting
        // app know that we're in a different state now.
        if (oldState != ConnectionState())
        { // rely on the null handling again
            // send the notification
            _ConnectionStateChangedHandlers(*this, nullptr);
        }
    }

    bool ControlCore::Initialize(const float actualWidth,
                                 const float actualHeight,
                                 const float compositionScale)
    {
        assert(_settings);

        _panelWidth = actualWidth;
        _panelHeight = actualHeight;
        _compositionScale = compositionScale;

        { // scope for terminalLock
            const auto lock = _terminal->LockForWriting();

            if (_initializedTerminal.load(std::memory_order_relaxed))
            {
                return false;
            }

            const auto windowWidth = actualWidth * compositionScale;
            const auto windowHeight = actualHeight * compositionScale;

            if (windowWidth == 0 || windowHeight == 0)
            {
                return false;
            }

            if (_settings->UseAtlasEngine())
            {
                _renderEngine = std::make_unique<::Microsoft::Console::Render::AtlasEngine>();
            }
            else
            {
                _renderEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
            }

            _renderer->AddRenderEngine(_renderEngine.get());

            // Initialize our font with the renderer
            // We don't have to care about DPI. We'll get a change message immediately if it's not 96
            // and react accordingly.
            _updateFont();

            const til::size windowSize{ til::math::rounding, windowWidth, windowHeight };

            // First set up the dx engine with the window size in pixels.
            // Then, using the font, get the number of characters that can fit.
            // Resize our terminal connection to match that size, and initialize the terminal with that size.
            const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
            LOG_IF_FAILED(_renderEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

            // Update DxEngine's SelectionBackground
            _renderEngine->SetSelectionBackground(til::color{ _settings->SelectionBackground() });

            const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);
            const auto width = vp.Width();
            const auto height = vp.Height();
            _connection.Resize(height, width);

            if (_owningHwnd != 0)
            {
                if (auto conpty{ _connection.try_as<TerminalConnection::ConptyConnection>() })
                {
                    conpty.ReparentWindow(_owningHwnd);
                }
            }

            // Override the default width and height to match the size of the swapChainPanel
            _settings->InitialCols(width);
            _settings->InitialRows(height);

            _terminal->CreateFromSettings(*_settings, *_renderer);

            // IMPORTANT! Set this callback up sooner than later. If we do it
            // after Enable, then it'll be possible to paint the frame once
            // _before_ the warning handler is set up, and then warnings from
            // the first paint will be ignored!
            _renderEngine->SetWarningCallback(std::bind(&ControlCore::_rendererWarning, this, std::placeholders::_1));

            // Tell the render engine to notify us when the swap chain changes.
            // We do this after we initially set the swapchain so as to avoid
            // unnecessary callbacks (and locking problems)
            _renderEngine->SetCallback([this](HANDLE handle) {
                _renderEngineSwapChainChanged(handle);
            });

            _renderEngine->SetRetroTerminalEffect(_settings->RetroTerminalEffect());
            _renderEngine->SetPixelShaderPath(_settings->PixelShaderPath());
            _renderEngine->SetForceFullRepaintRendering(_settings->ForceFullRepaintRendering());
            _renderEngine->SetSoftwareRendering(_settings->SoftwareRendering());

            _updateAntiAliasingMode();

            // GH#5098: Inform the engine of the opacity of the default text background.
            // GH#11315: Always do this, even if they don't have acrylic on.
            _renderEngine->EnableTransparentBackground(_isBackgroundTransparent());

            THROW_IF_FAILED(_renderEngine->Enable());

            _initializedTerminal.store(true, std::memory_order_relaxed);
        } // scope for TerminalLock

        // Start the connection outside of lock, because it could
        // start writing output immediately.
        _connection.Start();

        return true;
    }

    // Method Description:
    // - Tell the renderer to start painting.
    // - !! IMPORTANT !! Make sure that we've attached our swap chain to an
    //   actual target before calling this.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ControlCore::EnablePainting()
    {
        if (_initializedTerminal.load(std::memory_order_relaxed))
        {
            const auto lock = _terminal->LockForWriting();
            _renderer->EnablePainting();
        }
    }

    // Method Description:
    // - Writes the given sequence as input to the active terminal connection.
    // - This method has been overloaded to allow zero-copy winrt::param::hstring optimizations.
    // Arguments:
    // - wstr: the string of characters to write to the terminal connection.
    // Return Value:
    // - <none>
    void ControlCore::_sendInputToConnection(std::wstring_view wstr)
    {
        if (wstr.empty())
        {
            return;
        }

        // The connection may call functions like WriteFile() which may block indefinitely.
        // It's important we don't hold any mutexes across such calls.
        _terminal->_assertUnlocked();

        if (_isReadOnly)
        {
            _raiseReadOnlyWarning();
        }
        else
        {
            _connection.WriteInput(wstr);
        }
    }

    // Method Description:
    // - Writes the given sequence as input to the active terminal connection,
    // Arguments:
    // - wstr: the string of characters to write to the terminal connection.
    // Return Value:
    // - <none>
    void ControlCore::SendInput(const winrt::hstring& wstr)
    {
        _sendInputToConnection(wstr);
    }

    bool ControlCore::SendCharEvent(const wchar_t ch,
                                    const WORD scanCode,
                                    const ::Microsoft::Terminal::Core::ControlKeyStates modifiers)
    {
        const wchar_t CtrlD = 0x4;
        const wchar_t Enter = '\r';

        if (_connection.State() >= winrt::Microsoft::Terminal::TerminalConnection::ConnectionState::Closed)
        {
            if (ch == CtrlD)
            {
                _CloseTerminalRequestedHandlers(*this, nullptr);
                return true;
            }

            if (ch == Enter)
            {
                // Ask the hosting application to give us a new connection.
                _RestartTerminalRequestedHandlers(*this, nullptr);
                return true;
            }
        }

        if (ch == L'\x3') // Ctrl+C or Ctrl+Break
        {
            _handleControlC();
        }

        TerminalInput::OutputType out;
        {
            const auto lock = _terminal->LockForReading();
            out = _terminal->SendCharEvent(ch, scanCode, modifiers);
        }
        if (out)
        {
            _sendInputToConnection(*out);
            return true;
        }
        return false;
    }

    void ControlCore::_handleControlC()
    {
        if (!_midiAudioSkipTimer)
        {
            _midiAudioSkipTimer = _dispatcher.CreateTimer();
            _midiAudioSkipTimer.Interval(std::chrono::seconds(1));
            _midiAudioSkipTimer.IsRepeating(false);
            _midiAudioSkipTimer.Tick([weakSelf = get_weak()](auto&&, auto&&) {
                if (const auto self = weakSelf.get())
                {
                    self->_midiAudio.EndSkip();
                }
            });
        }

        _midiAudio.BeginSkip();
        _midiAudioSkipTimer.Start();
    }

    bool ControlCore::_shouldTryUpdateSelection(const WORD vkey)
    {
        // GH#6423 - don't update selection if the key that was pressed was a
        // modifier key. We'll wait for a real keystroke to dismiss the
        // GH #7395 - don't update selection when taking PrintScreen
        // selection.
        return _terminal->IsSelectionActive() && ::Microsoft::Terminal::Core::Terminal::IsInputKey(vkey);
    }

    bool ControlCore::TryMarkModeKeybinding(const WORD vkey,
                                            const ::Microsoft::Terminal::Core::ControlKeyStates mods)
    {
        auto lock = _terminal->LockForWriting();

        if (_shouldTryUpdateSelection(vkey) && _terminal->SelectionMode() == ::Terminal::SelectionInteractionMode::Mark)
        {
            if (vkey == 'A' && !mods.IsAltPressed() && !mods.IsShiftPressed() && mods.IsCtrlPressed())
            {
                // Ctrl + A --> Select all
                _terminal->SelectAll();
                _updateSelectionUI();
                return true;
            }
            else if (vkey == VK_TAB && !mods.IsAltPressed() && !mods.IsCtrlPressed() && _settings->DetectURLs())
            {
                // [Shift +] Tab --> next/previous hyperlink
                const auto direction = mods.IsShiftPressed() ? ::Terminal::SearchDirection::Backward : ::Terminal::SearchDirection::Forward;
                _terminal->SelectHyperlink(direction);
                _updateSelectionUI();
                return true;
            }
            else if (vkey == VK_RETURN && mods.IsCtrlPressed() && !mods.IsAltPressed() && !mods.IsShiftPressed())
            {
                // Ctrl + Enter --> Open URL
                if (const auto uri = _terminal->GetHyperlinkAtBufferPosition(_terminal->GetSelectionAnchor()); !uri.empty())
                {
                    lock.unlock();
                    _OpenHyperlinkHandlers(*this, winrt::make<OpenHyperlinkEventArgs>(winrt::hstring{ uri }));
                }
                else
                {
                    const auto selectedText = _terminal->GetTextBuffer().GetPlainText(_terminal->GetSelectionAnchor(), _terminal->GetSelectionEnd());
                    lock.unlock();
                    _OpenHyperlinkHandlers(*this, winrt::make<OpenHyperlinkEventArgs>(winrt::hstring{ selectedText }));
                }
                return true;
            }
            else if (vkey == VK_RETURN && !mods.IsCtrlPressed() && !mods.IsAltPressed())
            {
                // [Shift +] Enter --> copy text
                CopySelectionToClipboard(mods.IsShiftPressed(), nullptr);
                _terminal->ClearSelection();
                _updateSelectionUI();
                return true;
            }
            else if (vkey == VK_ESCAPE)
            {
                _terminal->ClearSelection();
                _updateSelectionUI();
                return true;
            }
            else if (const auto updateSlnParams{ _terminal->ConvertKeyEventToUpdateSelectionParams(mods, vkey) })
            {
                // try to update the selection
                _terminal->UpdateSelection(updateSlnParams->first, updateSlnParams->second, mods);
                _updateSelectionUI();
                return true;
            }
        }
        return false;
    }

    // Method Description:
    // - Send this particular key event to the terminal.
    //   See Terminal::SendKeyEvent for more information.
    // - Clears the current selection.
    // - Makes the cursor briefly visible during typing.
    // Arguments:
    // - vkey: The vkey of the key pressed.
    // - scanCode: The scan code of the key pressed.
    // - modifiers: The Microsoft::Terminal::Core::ControlKeyStates representing the modifier key states.
    // - keyDown: If true, the key was pressed, otherwise the key was released.
    bool ControlCore::TrySendKeyEvent(const WORD vkey,
                                      const WORD scanCode,
                                      const ControlKeyStates modifiers,
                                      const bool keyDown)
    {
        if (!vkey)
        {
            return true;
        }

        TerminalInput::OutputType out;
        {
            const auto lock = _terminal->LockForWriting();

            // Update the selection, if it's present
            // GH#8522, GH#3758 - Only modify the selection on key _down_. If we
            // modify on key up, then there's chance that we'll immediately dismiss
            // a selection created by an action bound to a keydown.
            if (_shouldTryUpdateSelection(vkey) && keyDown)
            {
                // try to update the selection
                if (const auto updateSlnParams{ _terminal->ConvertKeyEventToUpdateSelectionParams(modifiers, vkey) })
                {
                    _terminal->UpdateSelection(updateSlnParams->first, updateSlnParams->second, modifiers);
                    _updateSelectionUI();
                    return true;
                }

                // GH#8791 - don't dismiss selection if Windows key was also pressed as a key-combination.
                if (!modifiers.IsWinPressed())
                {
                    _terminal->ClearSelection();
                    _updateSelectionUI();
                }

                // When there is a selection active, escape should clear it and NOT flow through
                // to the terminal. With any other keypress, it should clear the selection AND
                // flow through to the terminal.
                if (vkey == VK_ESCAPE)
                {
                    return true;
                }
            }

            // If the terminal translated the key, mark the event as handled.
            // This will prevent the system from trying to get the character out
            // of it and sending us a CharacterReceived event.
            out = _terminal->SendKeyEvent(vkey, scanCode, modifiers, keyDown);
        }
        if (out)
        {
            _sendInputToConnection(*out);
            return true;
        }
        return false;
    }

    bool ControlCore::SendMouseEvent(const til::point viewportPos,
                                     const unsigned int uiButton,
                                     const ControlKeyStates states,
                                     const short wheelDelta,
                                     const TerminalInput::MouseButtonState state)
    {
        TerminalInput::OutputType out;
        {
            const auto lock = _terminal->LockForReading();
            out = _terminal->SendMouseEvent(viewportPos, uiButton, states, wheelDelta, state);
        }
        if (out)
        {
            _sendInputToConnection(*out);
            return true;
        }
        return false;
    }

    void ControlCore::UserScrollViewport(const int viewTop)
    {
        {
            // This is a scroll event that wasn't initiated by the terminal
            //      itself - it was initiated by the mouse wheel, or the scrollbar.
            const auto lock = _terminal->LockForWriting();
            _terminal->UserScrollViewport(viewTop);
        }

        const auto shared = _shared.lock_shared();
        if (shared->updatePatternLocations)
        {
            (*shared->updatePatternLocations)();
        }
    }

    void ControlCore::AdjustOpacity(const double adjustment)
    {
        if (adjustment == 0)
        {
            return;
        }

        _setOpacity(Opacity() + adjustment);
    }

    // Method Description:
    // - Updates the opacity of the terminal
    // Arguments:
    // - opacity: The new opacity to set.
    // - focused (default == true): Whether the window is focused or unfocused.
    // Return Value:
    // - <none>
    void ControlCore::_setOpacity(const double opacity, bool focused)
    {
        const auto newOpacity = std::clamp(opacity,
                                           0.0,
                                           1.0);

        if (newOpacity == Opacity())
        {
            return;
        }

        // Update our runtime opacity value
        _runtimeOpacity = newOpacity;

        //Stores the focused runtime opacity separately from unfocused opacity
        //to transition smoothly between the two.
        _runtimeFocusedOpacity = focused ? newOpacity : _runtimeFocusedOpacity;

        // Manually turn off acrylic if they turn off transparency.
        _runtimeUseAcrylic = newOpacity < 1.0 && _settings->UseAcrylic();

        // Update the renderer as well. It might need to fall back from
        // cleartype -> grayscale if the BG is transparent / acrylic.
        if (_renderEngine)
        {
            const auto lock = _terminal->LockForWriting();
            _renderEngine->EnableTransparentBackground(_isBackgroundTransparent());
            _renderer->NotifyPaintFrame();
        }

        auto eventArgs = winrt::make_self<TransparencyChangedEventArgs>(newOpacity);
        _TransparencyChangedHandlers(*this, *eventArgs);
    }

    void ControlCore::ToggleShaderEffects()
    {
        const auto path = _settings->PixelShaderPath();
        const auto lock = _terminal->LockForWriting();
        // Originally, this action could be used to enable the retro effects
        // even when they're set to `false` in the settings. If the user didn't
        // specify a custom pixel shader, manually enable the legacy retro
        // effect first. This will ensure that a toggle off->on will still work,
        // even if they currently have retro effect off.
        if (path.empty())
        {
            _renderEngine->SetRetroTerminalEffect(!_renderEngine->GetRetroTerminalEffect());
        }
        else
        {
            _renderEngine->SetPixelShaderPath(_renderEngine->GetPixelShaderPath().empty() ? std::wstring_view{ path } : std::wstring_view{});
        }
        // Always redraw after toggling effects. This way even if the control
        // does not have focus it will update immediately.
        _renderer->TriggerRedrawAll();
    }

    // Method description:
    // - Updates last hovered cell, renders / removes rendering of hyper-link if required
    // Arguments:
    // - terminalPosition: The terminal position of the pointer
    void ControlCore::SetHoveredCell(Core::Point pos)
    {
        _updateHoveredCell(std::optional<til::point>{ pos });
    }
    void ControlCore::ClearHoveredCell()
    {
        _updateHoveredCell(std::nullopt);
    }

    void ControlCore::_updateHoveredCell(const std::optional<til::point> terminalPosition)
    {
        if (terminalPosition == _lastHoveredCell)
        {
            return;
        }

        // GH#9618 - lock while we're reading from the terminal, and if we need
        // to update something, then lock again to write the terminal.

        _lastHoveredCell = terminalPosition;
        uint16_t newId{ 0u };
        // we can't use auto here because we're pre-declaring newInterval.
        decltype(_terminal->GetHyperlinkIntervalFromViewportPosition({})) newInterval{ std::nullopt };
        if (terminalPosition.has_value())
        {
            const auto lock = _terminal->LockForReading();
            newId = _terminal->GetHyperlinkIdAtViewportPosition(*terminalPosition);
            newInterval = _terminal->GetHyperlinkIntervalFromViewportPosition(*terminalPosition);
        }

        // If the hyperlink ID changed or the interval changed, trigger a redraw all
        // (so this will happen both when we move onto a link and when we move off a link)
        if (newId != _lastHoveredId ||
            (newInterval != _lastHoveredInterval))
        {
            // Introduce scope for lock - we don't want to raise the
            // HoveredHyperlinkChanged event under lock, because then handlers
            // wouldn't be able to ask us about the hyperlink text/position
            // without deadlocking us.
            {
                const auto lock = _terminal->LockForWriting();

                _lastHoveredId = newId;
                _lastHoveredInterval = newInterval;
                _renderer->UpdateHyperlinkHoveredId(newId);
                _renderer->UpdateLastHoveredInterval(newInterval);
                _renderer->TriggerRedrawAll();
            }

            _HoveredHyperlinkChangedHandlers(*this, nullptr);
        }
    }

    winrt::hstring ControlCore::GetHyperlink(const Core::Point pos) const
    {
        const auto lock = _terminal->LockForReading();
        return winrt::hstring{ _terminal->GetHyperlinkAtViewportPosition(til::point{ pos }) };
    }

    winrt::hstring ControlCore::HoveredUriText() const
    {
        if (_lastHoveredCell.has_value())
        {
            const auto lock = _terminal->LockForReading();
            auto uri{ _terminal->GetHyperlinkAtViewportPosition(*_lastHoveredCell) };
            uri.resize(std::min<size_t>(1024u, uri.size())); // Truncate for display
            return winrt::hstring{ uri };
        }
        return {};
    }

    Windows::Foundation::IReference<Core::Point> ControlCore::HoveredCell() const
    {
        return _lastHoveredCell.has_value() ? Windows::Foundation::IReference<Core::Point>{ _lastHoveredCell.value().to_core_point() } : nullptr;
    }

    // Method Description:
    // - Updates the settings of the current terminal.
    // - INVARIANT: This method can only be called if the caller DOES NOT HAVE writing lock on the terminal.
    void ControlCore::UpdateSettings(const IControlSettings& settings, const IControlAppearance& newAppearance)
    {
        _settings = winrt::make_self<implementation::ControlSettings>(settings, newAppearance);

        const auto lock = _terminal->LockForWriting();

        _cellWidth = CSSLengthPercentage::FromString(_settings->CellWidth().c_str());
        _cellHeight = CSSLengthPercentage::FromString(_settings->CellHeight().c_str());
        _runtimeOpacity = std::nullopt;
        _runtimeFocusedOpacity = std::nullopt;

        // Manually turn off acrylic if they turn off transparency.
        _runtimeUseAcrylic = _settings->Opacity() < 1.0 && _settings->UseAcrylic();

        const auto sizeChanged = _setFontSizeUnderLock(_settings->FontSize());

        // Update the terminal core with its new Core settings
        _terminal->UpdateSettings(*_settings);

        if (!_initializedTerminal.load(std::memory_order_relaxed))
        {
            // If we haven't initialized, there's no point in continuing.
            // Initialization will handle the renderer settings.
            return;
        }

        _renderEngine->SetForceFullRepaintRendering(_settings->ForceFullRepaintRendering());
        _renderEngine->SetSoftwareRendering(_settings->SoftwareRendering());
        // Inform the renderer of our opacity
        _renderEngine->EnableTransparentBackground(_isBackgroundTransparent());

        // Trigger a redraw to repaint the window background and tab colors.
        _renderer->TriggerRedrawAll(true, true);

        _updateAntiAliasingMode();

        if (sizeChanged)
        {
            _refreshSizeUnderLock();
        }
    }

    // Method Description:
    // - Updates the appearance of the current terminal.
    // - INVARIANT: This method can only be called if the caller DOES NOT HAVE writing lock on the terminal.
    void ControlCore::ApplyAppearance(const bool& focused)
    {
        const auto lock = _terminal->LockForWriting();
        const auto& newAppearance{ focused ? _settings->FocusedAppearance() : _settings->UnfocusedAppearance() };
        // Update the terminal core with its new Core settings
        _terminal->UpdateAppearance(*newAppearance);

        // Update DxEngine settings under the lock
        if (_renderEngine)
        {
            // Update DxEngine settings under the lock
            _renderEngine->SetSelectionBackground(til::color{ newAppearance->SelectionBackground() });
            _renderEngine->SetRetroTerminalEffect(newAppearance->RetroTerminalEffect());
            _renderEngine->SetPixelShaderPath(newAppearance->PixelShaderPath());

            // Incase EnableUnfocusedAcrylic is disabled and Focused Acrylic is set to true,
            // the terminal should ignore the unfocused opacity from settings.
            // The Focused Opacity from settings should be ignored if overridden at runtime.
            bool useFocusedRuntimeOpacity = focused || (!_settings->EnableUnfocusedAcrylic() && UseAcrylic());
            double newOpacity = useFocusedRuntimeOpacity ?
                                    FocusedOpacity() :
                                    newAppearance->Opacity();
            _setOpacity(newOpacity, focused);

            // No need to update Acrylic if UnfocusedAcrylic is disabled
            if (_settings->EnableUnfocusedAcrylic())
            {
                // Manually turn off acrylic if they turn off transparency.
                _runtimeUseAcrylic = Opacity() < 1.0 && newAppearance->UseAcrylic();
            }

            // Update the renderer as well. It might need to fall back from
            // cleartype -> grayscale if the BG is transparent / acrylic.
            _renderEngine->EnableTransparentBackground(_isBackgroundTransparent());
            _renderer->NotifyPaintFrame();

            auto eventArgs = winrt::make_self<TransparencyChangedEventArgs>(Opacity());
            _TransparencyChangedHandlers(*this, *eventArgs);

            _renderer->TriggerRedrawAll(true, true);
        }
    }

    void ControlCore::_updateAntiAliasingMode()
    {
        D2D1_TEXT_ANTIALIAS_MODE mode;
        // Update DxEngine's AntialiasingMode
        switch (_settings->AntialiasingMode())
        {
        case TextAntialiasingMode::Cleartype:
            mode = D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE;
            break;
        case TextAntialiasingMode::Aliased:
            mode = D2D1_TEXT_ANTIALIAS_MODE_ALIASED;
            break;
        default:
            mode = D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE;
            break;
        }

        _renderEngine->SetAntialiasingMode(mode);
    }

    // Method Description:
    // - Update the font with the renderer. This will be called either when the
    //      font changes or the DPI changes, as DPI changes will necessitate a
    //      font change. This method will *not* change the buffer/viewport size
    //      to account for the new glyph dimensions. Callers should make sure to
    //      appropriately call _doResizeUnderLock after this method is called.
    // - The write lock should be held when calling this method.
    // Arguments:
    // <none>
    void ControlCore::_updateFont()
    {
        const auto newDpi = static_cast<int>(lrint(_compositionScale * USER_DEFAULT_SCREEN_DPI));

        _terminal->SetFontInfo(_actualFont);

        if (_renderEngine)
        {
            std::unordered_map<std::wstring_view, uint32_t> featureMap;
            if (const auto fontFeatures = _settings->FontFeatures())
            {
                featureMap.reserve(fontFeatures.Size());

                for (const auto& [tag, param] : fontFeatures)
                {
                    featureMap.emplace(tag, param);
                }
            }
            std::unordered_map<std::wstring_view, float> axesMap;
            if (const auto fontAxes = _settings->FontAxes())
            {
                axesMap.reserve(fontAxes.Size());

                for (const auto& [axis, value] : fontAxes)
                {
                    axesMap.emplace(axis, value);
                }
            }

            // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
            //      actually fail. We need a way to gracefully fallback.
            LOG_IF_FAILED(_renderEngine->UpdateDpi(newDpi));
            LOG_IF_FAILED(_renderEngine->UpdateFont(_desiredFont, _actualFont, featureMap, axesMap));
        }

        // If the actual font isn't what was requested...
        if (_actualFont.GetFaceName() != _desiredFont.GetFaceName())
        {
            // Then warn the user that we picked something because we couldn't find their font.
            // Format message with user's choice of font and the font that was chosen instead.
            const winrt::hstring message{ fmt::format(std::wstring_view{ RS_(L"NoticeFontNotFound") },
                                                      _desiredFont.GetFaceName(),
                                                      _actualFont.GetFaceName()) };
            auto noticeArgs = winrt::make<NoticeEventArgs>(NoticeLevel::Warning, message);
            _RaiseNoticeHandlers(*this, std::move(noticeArgs));
        }

        const auto actualNewSize = _actualFont.GetSize();
        _FontSizeChangedHandlers(*this, winrt::make<FontSizeChangedArgs>(actualNewSize.width, actualNewSize.height));
    }

    // Method Description:
    // - Set the font size of the terminal control.
    // Arguments:
    // - fontSize: The size of the font.
    // Return Value:
    // - Returns true if you need to call _refreshSizeUnderLock().
    bool ControlCore::_setFontSizeUnderLock(float fontSize)
    {
        // Make sure we have a non-zero font size
        const auto newSize = std::max(fontSize, 1.0f);
        const auto fontFace = _settings->FontFace();
        const auto fontWeight = _settings->FontWeight();
        _desiredFont = { fontFace, 0, fontWeight.Weight, newSize, CP_UTF8 };
        _actualFont = { fontFace, 0, fontWeight.Weight, _desiredFont.GetEngineSize(), CP_UTF8, false };
        _actualFontFaceName = { fontFace };

        _desiredFont.SetCellSize(_cellWidth, _cellHeight);

        const auto before = _actualFont.GetSize();
        _updateFont();
        const auto after = _actualFont.GetSize();
        return before != after;
    }

    // Method Description:
    // - Reset the font size of the terminal to its default size.
    // Arguments:
    // - none
    void ControlCore::ResetFontSize()
    {
        const auto lock = _terminal->LockForWriting();

        if (_setFontSizeUnderLock(_settings->FontSize()))
        {
            _refreshSizeUnderLock();
        }
    }

    // Method Description:
    // - Adjust the font size of the terminal control.
    // Arguments:
    // - fontSizeDelta: The amount to increase or decrease the font size by.
    void ControlCore::AdjustFontSize(float fontSizeDelta)
    {
        const auto lock = _terminal->LockForWriting();

        if (_setFontSizeUnderLock(_desiredFont.GetFontSize() + fontSizeDelta))
        {
            _refreshSizeUnderLock();
        }
    }

    // Method Description:
    // - Process a resize event that was initiated by the user. This can either
    //   be due to the user resizing the window (causing the swapchain to
    //   resize) or due to the DPI changing (causing us to need to resize the
    //   buffer to match)
    // - Note that a DPI change will also trigger a font size change, and will
    //   call into here.
    // - The write lock should be held when calling this method, we might be
    //   changing the buffer size in _refreshSizeUnderLock.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ControlCore::_refreshSizeUnderLock()
    {
        if (_IsClosing())
        {
            return;
        }

        auto cx = gsl::narrow_cast<til::CoordType>(lrint(_panelWidth * _compositionScale));
        auto cy = gsl::narrow_cast<til::CoordType>(lrint(_panelHeight * _compositionScale));

        // Don't actually resize so small that a single character wouldn't fit
        // in either dimension. The buffer really doesn't like being size 0.
        cx = std::max(cx, _actualFont.GetSize().width);
        cy = std::max(cy, _actualFont.GetSize().height);

        // Convert our new dimensions to characters
        const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, { cx, cy });
        const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);

        _terminal->ClearSelection();

        // Tell the dx engine that our window is now the new size.
        THROW_IF_FAILED(_renderEngine->SetWindowSize({ cx, cy }));

        // Invalidate everything
        _renderer->TriggerRedrawAll();

        // If this function succeeds with S_FALSE, then the terminal didn't
        // actually change size. No need to notify the connection of this no-op.
        const auto hr = _terminal->UserResize({ vp.Width(), vp.Height() });
        if (SUCCEEDED(hr) && hr != S_FALSE)
        {
            _connection.Resize(vp.Height(), vp.Width());
        }
    }

    void ControlCore::SizeChanged(const float width,
                                  const float height)
    {
        SizeOrScaleChanged(width, height, _compositionScale);
    }

    void ControlCore::ScaleChanged(const float scale)
    {
        if (!_renderEngine)
        {
            return;
        }
        SizeOrScaleChanged(_panelWidth, _panelHeight, scale);
    }

    void ControlCore::SizeOrScaleChanged(const float width,
                                         const float height,
                                         const float scale)
    {
        const auto scaleChanged = _compositionScale != scale;
        // _refreshSizeUnderLock redraws the entire terminal.
        // Don't call it if we don't have to.
        if (_panelWidth == width && _panelHeight == height && !scaleChanged)
        {
            return;
        }

        _panelWidth = width;
        _panelHeight = height;
        _compositionScale = scale;

        const auto lock = _terminal->LockForWriting();
        if (scaleChanged)
        {
            // _updateFont relies on the new _compositionScale set above
            _updateFont();
        }
        _refreshSizeUnderLock();
    }

    void ControlCore::SetSelectionAnchor(const til::point position)
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->SetSelectionAnchor(position);
    }

    // Method Description:
    // - Retrieves selection metadata from Terminal necessary to draw the
    //    selection markers.
    // - Since all of this needs to be done under lock, it is more performant
    //    to throw it all in a struct and pass it along.
    Control::SelectionData ControlCore::SelectionInfo() const
    {
        const auto lock = _terminal->LockForReading();
        Control::SelectionData info;

        const auto start{ _terminal->SelectionStartForRendering() };
        info.StartPos = { start.x, start.y };

        const auto end{ _terminal->SelectionEndForRendering() };
        info.EndPos = { end.x, end.y };

        info.Endpoint = static_cast<SelectionEndpointTarget>(_terminal->SelectionEndpointTarget());

        const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
        info.StartAtLeftBoundary = _terminal->GetSelectionAnchor().x == bufferSize.Left();
        info.EndAtRightBoundary = _terminal->GetSelectionEnd().x == bufferSize.RightInclusive();
        return info;
    }

    // Method Description:
    // - Sets selection's end position to match supplied cursor position, e.g. while mouse dragging.
    // Arguments:
    // - position: the point in terminal coordinates (in cells, not pixels)
    void ControlCore::SetEndSelectionPoint(const til::point position)
    {
        const auto lock = _terminal->LockForWriting();

        if (!_terminal->IsSelectionActive())
        {
            return;
        }

        til::point terminalPosition{
            std::clamp(position.x, 0, _terminal->GetViewport().Width() - 1),
            std::clamp(position.y, 0, _terminal->GetViewport().Height() - 1)
        };

        // save location (for rendering) + render
        _terminal->SetSelectionEnd(terminalPosition);
        _updateSelectionUI();
    }

    // Called when the Terminal wants to set something to the clipboard, i.e.
    // when an OSC 52 is emitted.
    void ControlCore::_terminalCopyToClipboard(std::wstring_view wstr)
    {
        _CopyToClipboardHandlers(*this, winrt::make<implementation::CopyToClipboardEventArgs>(winrt::hstring{ wstr }));
    }

    // Method Description:
    // - Given a copy-able selection, get the selected text from the buffer and send it to the
    //     Windows Clipboard (CascadiaWin32:main.cpp).
    // Arguments:
    // - singleLine: collapse all of the text to one line
    // - formats: which formats to copy (defined by action's CopyFormatting arg). nullptr
    //             if we should defer which formats are copied to the global setting
    bool ControlCore::CopySelectionToClipboard(bool singleLine,
                                               const Windows::Foundation::IReference<CopyFormat>& formats)
    {
        const auto lock = _terminal->LockForWriting();

        // no selection --> nothing to copy
        if (!_terminal->IsSelectionActive())
        {
            return false;
        }

        // use action's copyFormatting if it's present, else fallback to globally
        // set copyFormatting.
        const auto copyFormats = formats != nullptr ? formats.Value() : _settings->CopyFormatting();

        const auto copyHtml = WI_IsFlagSet(copyFormats, CopyFormat::HTML);
        const auto copyRtf = WI_IsFlagSet(copyFormats, CopyFormat::RTF);

        // extract text from buffer
        // RetrieveSelectedTextFromBuffer will lock while it's reading
        const auto& [textData, htmlData, rtfData] = _terminal->RetrieveSelectedTextFromBuffer(singleLine, copyHtml, copyRtf);

        // send data up for clipboard
        _CopyToClipboardHandlers(*this,
                                 winrt::make<CopyToClipboardEventArgs>(winrt::hstring{ textData },
                                                                       winrt::to_hstring(htmlData),
                                                                       winrt::to_hstring(rtfData),
                                                                       copyFormats));
        return true;
    }

    void ControlCore::SelectAll()
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->SelectAll();
        _updateSelectionUI();
    }

    void ControlCore::ClearSelection()
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->ClearSelection();
        _updateSelectionUI();
    }

    bool ControlCore::ToggleBlockSelection()
    {
        const auto lock = _terminal->LockForWriting();
        if (_terminal->IsSelectionActive())
        {
            _terminal->SetBlockSelection(!_terminal->IsBlockSelection());
            _renderer->TriggerSelection();
            // do not update the selection markers!
            // if we were showing them, keep it that way.
            // otherwise, continue to not show them
            return true;
        }
        return false;
    }

    void ControlCore::ToggleMarkMode()
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->ToggleMarkMode();
        _updateSelectionUI();
    }

    Control::SelectionInteractionMode ControlCore::SelectionMode() const
    {
        return static_cast<Control::SelectionInteractionMode>(_terminal->SelectionMode());
    }

    bool ControlCore::SwitchSelectionEndpoint()
    {
        const auto lock = _terminal->LockForWriting();
        if (_terminal->IsSelectionActive())
        {
            _terminal->SwitchSelectionEndpoint();
            _updateSelectionUI();
            return true;
        }
        return false;
    }

    bool ControlCore::ExpandSelectionToWord()
    {
        const auto lock = _terminal->LockForWriting();
        if (_terminal->IsSelectionActive())
        {
            _terminal->ExpandSelectionToWord();
            _updateSelectionUI();
            return true;
        }
        return false;
    }

    // Method Description:
    // - Pre-process text pasted (presumably from the clipboard)
    //   before sending it over the terminal's connection.
    void ControlCore::PasteText(const winrt::hstring& hstr)
    {
        using namespace ::Microsoft::Console::Utils;

        auto filtered = FilterStringForPaste(hstr, CarriageReturnNewline | ControlCodes);
        if (BracketedPasteEnabled())
        {
            filtered.insert(0, L"\x1b[200~");
            filtered.append(L"\x1b[201~");
        }

        // It's important to not hold the terminal lock while calling this function as sending the data may take a long time.
        _sendInputToConnection(filtered);

        const auto lock = _terminal->LockForWriting();
        _terminal->ClearSelection();
        _updateSelectionUI();
        _terminal->TrySnapOnInput();
    }

    FontInfo ControlCore::GetFont() const
    {
        return _actualFont;
    }

    winrt::Windows::Foundation::Size ControlCore::FontSize() const noexcept
    {
        const auto fontSize = _actualFont.GetSize();
        return {
            ::base::saturated_cast<float>(fontSize.width),
            ::base::saturated_cast<float>(fontSize.height)
        };
    }
    winrt::hstring ControlCore::FontFaceName() const noexcept
    {
        // This getter used to return _actualFont.GetFaceName(), however GetFaceName() returns a STL
        // string and we need to return a WinRT string. This would require an additional allocation.
        // This method is called 10/s by TSFInputControl at the time of writing.
        return _actualFontFaceName;
    }

    uint16_t ControlCore::FontWeight() const noexcept
    {
        return static_cast<uint16_t>(_actualFont.GetWeight());
    }

    winrt::Windows::Foundation::Size ControlCore::FontSizeInDips() const
    {
        const auto fontSize = _actualFont.GetSize();
        const auto scale = 1.0f / _compositionScale;
        return {
            fontSize.width * scale,
            fontSize.height * scale,
        };
    }

    TerminalConnection::ConnectionState ControlCore::ConnectionState() const
    {
        return _connection ? _connection.State() : TerminalConnection::ConnectionState::Closed;
    }

    hstring ControlCore::Title()
    {
        const auto lock = _terminal->LockForReading();
        return hstring{ _terminal->GetConsoleTitle() };
    }

    hstring ControlCore::WorkingDirectory() const
    {
        const auto lock = _terminal->LockForReading();
        return hstring{ _terminal->GetWorkingDirectory() };
    }

    bool ControlCore::BracketedPasteEnabled() const noexcept
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->IsXtermBracketedPasteModeEnabled();
    }

    Windows::Foundation::IReference<winrt::Windows::UI::Color> ControlCore::TabColor() noexcept
    {
        const auto lock = _terminal->LockForReading();
        auto coreColor = _terminal->GetTabColor();
        return coreColor.has_value() ? Windows::Foundation::IReference<winrt::Windows::UI::Color>{ static_cast<winrt::Windows::UI::Color>(coreColor.value()) } :
                                       nullptr;
    }

    til::color ControlCore::ForegroundColor() const
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->GetRenderSettings().GetColorAlias(ColorAlias::DefaultForeground);
    }

    til::color ControlCore::BackgroundColor() const
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->GetRenderSettings().GetColorAlias(ColorAlias::DefaultBackground);
    }

    // Method Description:
    // - Gets the internal taskbar state value
    // Return Value:
    // - The taskbar state of this control
    const size_t ControlCore::TaskbarState() const noexcept
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->GetTaskbarState();
    }

    // Method Description:
    // - Gets the internal taskbar progress value
    // Return Value:
    // - The taskbar progress of this control
    const size_t ControlCore::TaskbarProgress() const noexcept
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->GetTaskbarProgress();
    }

    int ControlCore::ScrollOffset()
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->GetScrollOffset();
    }

    // Function Description:
    // - Gets the height of the terminal in lines of text. This is just the
    //   height of the viewport.
    // Return Value:
    // - The height of the terminal in lines of text
    int ControlCore::ViewHeight() const
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->GetViewport().Height();
    }

    // Function Description:
    // - Gets the height of the terminal in lines of text. This includes the
    //   history AND the viewport.
    // Return Value:
    // - The height of the terminal in lines of text
    int ControlCore::BufferHeight() const
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->GetBufferHeight();
    }

    void ControlCore::_terminalWarningBell()
    {
        // Since this can only ever be triggered by output from the connection,
        // then the Terminal already has the write lock when calling this
        // callback.
        _WarningBellHandlers(*this, nullptr);
    }

    // Method Description:
    // - Called for the Terminal's TitleChanged callback. This will re-raise
    //   a new winrt TypedEvent that can be listened to.
    // - The listeners to this event will re-query the control for the current
    //   value of Title().
    // Arguments:
    // - wstr: the new title of this terminal.
    // Return Value:
    // - <none>
    void ControlCore::_terminalTitleChanged(std::wstring_view wstr)
    {
        // Since this can only ever be triggered by output from the connection,
        // then the Terminal already has the write lock when calling this
        // callback.
        _TitleChangedHandlers(*this, winrt::make<TitleChangedEventArgs>(winrt::hstring{ wstr }));
    }

    // Method Description:
    // - Update the position and size of the scrollbar to match the given
    //      viewport top, viewport height, and buffer size.
    //   Additionally fires a ScrollPositionChanged event for anyone who's
    //      registered an event handler for us.
    // Arguments:
    // - viewTop: the top of the visible viewport, in rows. 0 indicates the top
    //      of the buffer.
    // - viewHeight: the height of the viewport in rows.
    // - bufferSize: the length of the buffer, in rows
    void ControlCore::_terminalScrollPositionChanged(const int viewTop,
                                                     const int viewHeight,
                                                     const int bufferSize)
    {
        if (!_initializedTerminal.load(std::memory_order_relaxed))
        {
            return;
        }

        // Start the throttled update of our scrollbar.
        auto update{ winrt::make<ScrollPositionChangedArgs>(viewTop,
                                                            viewHeight,
                                                            bufferSize) };

        if (_inUnitTests) [[unlikely]]
        {
            _ScrollPositionChangedHandlers(*this, update);
        }
        else
        {
            const auto shared = _shared.lock_shared();
            if (shared->updateScrollBar)
            {
                shared->updateScrollBar->Run(update);
            }
        }
    }

    void ControlCore::_terminalCursorPositionChanged()
    {
        // When the buffer's cursor moves, start the throttled func to
        // eventually dispatch a CursorPositionChanged event.
        const auto shared = _shared.lock_shared();
        if (shared->tsfTryRedrawCanvas)
        {
            shared->tsfTryRedrawCanvas->Run();
        }
    }

    void ControlCore::_terminalTaskbarProgressChanged()
    {
        _TaskbarProgressChangedHandlers(*this, nullptr);
    }

    void ControlCore::_terminalShowWindowChanged(bool showOrHide)
    {
        auto showWindow = winrt::make_self<implementation::ShowWindowArgs>(showOrHide);
        _ShowWindowChangedHandlers(*this, *showWindow);
    }

    // Method Description:
    // - Plays a single MIDI note, blocking for the duration.
    // Arguments:
    // - noteNumber - The MIDI note number to be played (0 - 127).
    // - velocity - The force with which the note should be played (0 - 127).
    // - duration - How long the note should be sustained (in microseconds).
    void ControlCore::_terminalPlayMidiNote(const int noteNumber, const int velocity, const std::chrono::microseconds duration)
    {
        // The UI thread might try to acquire the console lock from time to time.
        // --> Unlock it, so the UI doesn't hang while we're busy.
        const auto suspension = _terminal->SuspendLock();
        // This call will block for the duration, unless shutdown early.
        _midiAudio.PlayNote(reinterpret_cast<HWND>(_owningHwnd), noteNumber, velocity, std::chrono::duration_cast<std::chrono::milliseconds>(duration));
    }

    bool ControlCore::HasSelection() const
    {
        const auto lock = _terminal->LockForReading();
        return _terminal->IsSelectionActive();
    }

    // Method Description:
    // - Checks if the currently active selection spans multiple lines
    // Return Value:
    // - true if selection is multi-line
    bool ControlCore::HasMultiLineSelection() const
    {
        const auto lock = _terminal->LockForReading();
        assert(_terminal->IsSelectionActive()); // should only be called when selection is active
        return _terminal->GetSelectionAnchor().y != _terminal->GetSelectionEnd().y;
    }

    bool ControlCore::CopyOnSelect() const
    {
        return _settings->CopyOnSelect();
    }

    winrt::hstring ControlCore::SelectedText(bool trimTrailingWhitespace) const
    {
        // RetrieveSelectedTextFromBuffer will lock while it's reading
        const auto lock = _terminal->LockForReading();
        const auto internalResult{ _terminal->RetrieveSelectedTextFromBuffer(!trimTrailingWhitespace) };
        return winrt::hstring{ internalResult.plainText };
    }

    ::Microsoft::Console::Render::IRenderData* ControlCore::GetRenderData() const
    {
        return _terminal.get();
    }

    // Method Description:
    // - Search text in text buffer. This is triggered if the user click
    //   search button or press enter.
    // Arguments:
    // - text: the text to search
    // - goForward: boolean that represents if the current search direction is forward
    // - caseSensitive: boolean that represents if the current search is case sensitive
    // Return Value:
    // - <none>
    void ControlCore::Search(const winrt::hstring& text, const bool goForward, const bool caseSensitive)
    {
        const auto lock = _terminal->LockForWriting();

        if (_searcher.ResetIfStale(*GetRenderData(), text, !goForward, !caseSensitive))
        {
            _searcher.HighlightResults();
            _searcher.MoveToCurrentSelection();
            _cachedSearchResultRows = {};
        }
        else
        {
            _searcher.FindNext();
        }

        const auto foundMatch = _searcher.SelectCurrent();
        auto foundResults = winrt::make_self<implementation::FoundResultsArgs>(foundMatch);
        if (foundMatch)
        {
            // this is used for search,
            // DO NOT call _updateSelectionUI() here.
            // We don't want to show the markers so manually tell it to clear it.
            _terminal->SetBlockSelection(false);
            _UpdateSelectionMarkersHandlers(*this, winrt::make<implementation::UpdateSelectionMarkersEventArgs>(true));

            foundResults->TotalMatches(gsl::narrow<int32_t>(_searcher.Results().size()));
            foundResults->CurrentMatch(gsl::narrow<int32_t>(_searcher.CurrentMatch()));

            _terminal->AlwaysNotifyOnBufferRotation(true);
        }
        _renderer->TriggerSelection();

        // Raise a FoundMatch event, which the control will use to notify
        // narrator if there was any results in the buffer
        _FoundMatchHandlers(*this, *foundResults);
    }

    Windows::Foundation::Collections::IVector<int32_t> ControlCore::SearchResultRows()
    {
        const auto lock = _terminal->LockForReading();

        if (!_cachedSearchResultRows)
        {
            auto results = std::vector<int32_t>();
            auto lastRow = til::CoordTypeMin;

            for (const auto& match : _searcher.Results())
            {
                const auto row{ match.start.y };
                if (row != lastRow)
                {
                    results.push_back(row);
                    lastRow = row;
                }
            }

            _cachedSearchResultRows = winrt::single_threaded_vector<int32_t>(std::move(results));
        }

        return _cachedSearchResultRows;
    }

    void ControlCore::ClearSearch()
    {
        _terminal->AlwaysNotifyOnBufferRotation(false);
        _searcher = {};
    }

    void ControlCore::Close()
    {
        if (!_IsClosing())
        {
            _closing = true;

            // Ensure Close() doesn't hang, waiting for MidiAudio to finish playing an hour long song.
            _midiAudio.BeginSkip();

            // Stop accepting new output and state changes before we disconnect everything.
            _connectionOutputEventRevoker.revoke();
            _connectionStateChangedRevoker.revoke();
            _connection.Close();
        }
    }

    void ControlCore::_rendererWarning(const HRESULT hr)
    {
        _RendererWarningHandlers(*this, winrt::make<RendererWarningArgs>(hr));
    }

    winrt::fire_and_forget ControlCore::_renderEngineSwapChainChanged(const HANDLE sourceHandle)
    {
        // `sourceHandle` is a weak ref to a HANDLE that's ultimately owned by the
        // render engine's own unique_handle. We'll add another ref to it here.
        // This will make sure that we always have a valid HANDLE to give to
        // callers of our own SwapChainHandle method, even if the renderer is
        // currently in the process of discarding this value and creating a new
        // one. Callers should have already set up the SwapChainChanged
        // callback, so this all works out.

        winrt::handle duplicatedHandle;
        const auto processHandle = GetCurrentProcess();
        THROW_IF_WIN32_BOOL_FALSE(DuplicateHandle(processHandle, sourceHandle, processHandle, duplicatedHandle.put(), 0, FALSE, DUPLICATE_SAME_ACCESS));

        const auto weakThis{ get_weak() };

        // Concurrent read of _dispatcher is safe, because Detach() calls WaitForPaintCompletionAndDisable()
        // which blocks until this call returns. _dispatcher will only be changed afterwards.
        co_await wil::resume_foreground(_dispatcher);

        if (auto core{ weakThis.get() })
        {
            // `this` is safe to use now

            _lastSwapChainHandle = std::move(duplicatedHandle);
            // Now bubble the event up to the control.
            _SwapChainChangedHandlers(*this, winrt::box_value<uint64_t>(reinterpret_cast<uint64_t>(_lastSwapChainHandle.get())));
        }
    }

    void ControlCore::_rendererBackgroundColorChanged()
    {
        _BackgroundColorChangedHandlers(*this, nullptr);
    }

    void ControlCore::_rendererTabColorChanged()
    {
        _TabColorChangedHandlers(*this, nullptr);
    }

    void ControlCore::BlinkAttributeTick()
    {
        const auto lock = _terminal->LockForWriting();

        auto& renderSettings = _terminal->GetRenderSettings();
        renderSettings.ToggleBlinkRendition(*_renderer);
    }

    void ControlCore::BlinkCursor()
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->BlinkCursor();
    }

    bool ControlCore::CursorOn() const
    {
        return _terminal->IsCursorOn();
    }

    void ControlCore::CursorOn(const bool isCursorOn)
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->SetCursorOn(isCursorOn);
    }

    void ControlCore::ResumeRendering()
    {
        const auto lock = _terminal->LockForWriting();
        _renderer->ResetErrorStateAndResume();
    }

    bool ControlCore::IsVtMouseModeEnabled() const
    {
        const auto lock = _terminal->LockForWriting();
        return _terminal->IsTrackingMouseInput();
    }
    bool ControlCore::ShouldSendAlternateScroll(const unsigned int uiButton,
                                                const int32_t delta) const
    {
        const auto lock = _terminal->LockForWriting();
        return _terminal->ShouldSendAlternateScroll(uiButton, delta);
    }

    Core::Point ControlCore::CursorPosition() const
    {
        // If we haven't been initialized yet, then fake it.
        if (!_initializedTerminal.load(std::memory_order_relaxed))
        {
            return { 0, 0 };
        }

        const auto lock = _terminal->LockForReading();
        return _terminal->GetViewportRelativeCursorPosition().to_core_point();
    }

    // This one's really pushing the boundary of what counts as "encapsulation".
    // It really belongs in the "Interactivity" layer, which doesn't yet exist.
    // There's so many accesses to the selection in the Core though, that I just
    // put this here. The Control shouldn't be futzing that much with the
    // selection itself.
    void ControlCore::LeftClickOnTerminal(const til::point terminalPosition,
                                          const int numberOfClicks,
                                          const bool altEnabled,
                                          const bool shiftEnabled,
                                          const bool isOnOriginalPosition,
                                          bool& selectionNeedsToBeCopied)
    {
        const auto lock = _terminal->LockForWriting();
        // handle ALT key
        _terminal->SetBlockSelection(altEnabled);

        auto mode = ::Terminal::SelectionExpansion::Char;
        if (numberOfClicks == 1)
        {
            mode = ::Terminal::SelectionExpansion::Char;
        }
        else if (numberOfClicks == 2)
        {
            mode = ::Terminal::SelectionExpansion::Word;
        }
        else if (numberOfClicks == 3)
        {
            mode = ::Terminal::SelectionExpansion::Line;
        }

        // Update the selection appropriately

        // We reset the active selection if one of the conditions apply:
        // - shift is not held
        // - GH#9384: the position is the same as of the first click starting
        //   the selection (we need to reset selection on double-click or
        //   triple-click, so it captures the word or the line, rather than
        //   extending the selection)
        if (_terminal->IsSelectionActive() && (!shiftEnabled || isOnOriginalPosition))
        {
            // Reset the selection
            _terminal->ClearSelection();
            selectionNeedsToBeCopied = false; // there's no selection, so there's nothing to update
        }

        if (shiftEnabled && _terminal->IsSelectionActive())
        {
            // If shift is pressed and there is a selection we extend it using
            // the selection mode (expand the "end" selection point)
            _terminal->SetSelectionEnd(terminalPosition, mode);
            selectionNeedsToBeCopied = true;
        }
        else if (mode != ::Terminal::SelectionExpansion::Char || shiftEnabled)
        {
            // If we are handling a double / triple-click or shift+single click
            // we establish selection using the selected mode
            // (expand both "start" and "end" selection points)
            _terminal->MultiClickSelection(terminalPosition, mode);
            selectionNeedsToBeCopied = true;
        }
        else if (_settings->RepositionCursorWithMouse()) // This is also mode==Char && !shiftEnabled
        {
            // If we're handling a single left click, without shift pressed, and
            // outside mouse mode, AND the user has RepositionCursorWithMouse turned
            // on, let's try to move the cursor.
            //
            // We'll only move the cursor if the user has clicked after the last
            // mark, if there is one. That means the user also needs to set up
            // shell integration to enable this feature.
            //
            // As noted in GH #8573, there's plenty of edge cases with this
            // approach, but it's good enough to bring value to 90% of use cases.
            const auto cursorPos{ _terminal->GetCursorPosition() };

            // Does the current buffer line have a mark on it?
            const auto& marks{ _terminal->GetScrollMarks() };
            if (!marks.empty())
            {
                const auto& last{ marks.back() };
                const auto [start, end] = last.GetExtent();
                const auto lastNonSpace = _terminal->GetTextBuffer().GetLastNonSpaceCharacter();

                // If the user clicked off to the right side of the prompt, we
                // want to send keystrokes to the last character in the prompt +1.
                //
                // We don't want to send too many here. In CMD, if the user's
                // last command is longer than what they've currently typed, and
                // they press right arrow at the end of the prompt, COOKED_READ
                // will fill in characters from the previous command.
                //
                // By only sending keypresses to the end of the command + 1, we
                // should leave the cursor at the very end of the prompt,
                // without adding any characters from a previous command.
                auto clampedClick = terminalPosition;
                if (terminalPosition > lastNonSpace)
                {
                    clampedClick = lastNonSpace + til::point{ 1, 0 };
                    _terminal->GetTextBuffer().GetSize().Clamp(clampedClick);
                }

                if (clampedClick >= end)
                {
                    // Get the distance between the cursor and the click, in cells.
                    const auto bufferSize = _terminal->GetTextBuffer().GetSize();

                    // First, make sure to iterate from the first point to the
                    // second. The user may have clicked _earlier_ in the
                    // buffer!
                    auto goRight = clampedClick > cursorPos;
                    const auto startPoint = goRight ? cursorPos : clampedClick;
                    const auto endPoint = goRight ? clampedClick : cursorPos;

                    const auto delta = _terminal->GetTextBuffer().GetCellDistance(startPoint, endPoint);
                    const WORD key = goRight ? VK_RIGHT : VK_LEFT;

                    std::wstring buffer;
                    const auto append = [&](TerminalInput::OutputType&& out) {
                        if (out)
                        {
                            buffer.append(std::move(*out));
                        }
                    };

                    // Send an up and a down once per cell. This won't
                    // accurately handle wide characters, or continuation
                    // prompts, or cases where a single escape character in the
                    // command (e.g. ^[) takes up two cells.
                    for (size_t i = 0u; i < delta; i++)
                    {
                        append(_terminal->SendKeyEvent(key, 0, {}, true));
                        append(_terminal->SendKeyEvent(key, 0, {}, false));
                    }

                    _sendInputToConnection(buffer);
                }
            }
        }
        _updateSelectionUI();
    }

    // Method Description:
    // - Updates the renderer's representation of the selection as well as the selection marker overlay in TermControl
    void ControlCore::_updateSelectionUI()
    {
        _renderer->TriggerSelection();
        // only show the markers if we're doing a keyboard selection or in mark mode
        const bool showMarkers{ _terminal->SelectionMode() >= ::Microsoft::Terminal::Core::Terminal::SelectionInteractionMode::Keyboard };
        _UpdateSelectionMarkersHandlers(*this, winrt::make<implementation::UpdateSelectionMarkersEventArgs>(!showMarkers));
    }

    void ControlCore::AttachUiaEngine(::Microsoft::Console::Render::IRenderEngine* const pEngine)
    {
        // _renderer will always exist since it's introduced in the ctor
        const auto lock = _terminal->LockForWriting();
        _renderer->AddRenderEngine(pEngine);
    }
    void ControlCore::DetachUiaEngine(::Microsoft::Console::Render::IRenderEngine* const pEngine)
    {
        const auto lock = _terminal->LockForWriting();
        _renderer->RemoveRenderEngine(pEngine);
    }

    bool ControlCore::IsInReadOnlyMode() const
    {
        return _isReadOnly;
    }

    void ControlCore::ToggleReadOnlyMode()
    {
        _isReadOnly = !_isReadOnly;
    }

    void ControlCore::SetReadOnlyMode(const bool readOnlyState)
    {
        _isReadOnly = readOnlyState;
    }

    void ControlCore::_raiseReadOnlyWarning()
    {
        auto noticeArgs = winrt::make<NoticeEventArgs>(NoticeLevel::Info, RS_(L"TermControlReadOnly"));
        _RaiseNoticeHandlers(*this, std::move(noticeArgs));
    }
    void ControlCore::_connectionOutputHandler(const hstring& hstr)
    {
        try
        {
            {
                const auto lock = _terminal->LockForWriting();
                _terminal->Write(hstr);
            }

            // Start the throttled update of where our hyperlinks are.
            const auto shared = _shared.lock_shared();
            if (shared->updatePatternLocations)
            {
                (*shared->updatePatternLocations)();
            }
        }
        catch (...)
        {
            // We're expecting to receive an exception here if the terminal
            // is closed while we're blocked playing a MIDI note.
        }
    }

    uint64_t ControlCore::SwapChainHandle() const
    {
        // This is only ever called by TermControl::AttachContent, which occurs
        // when we're taking an existing core and moving it to a new control.
        // Otherwise, we only ever use the value from the SwapChainChanged
        // event.
        return reinterpret_cast<uint64_t>(_lastSwapChainHandle.get());
    }

    // Method Description:
    // - Clear the contents of the buffer. The region cleared is given by
    //   clearType:
    //   * Screen: Clear only the contents of the visible viewport, leaving the
    //     cursor row at the top of the viewport.
    //   * Scrollback: Clear the contents of the scrollback.
    //   * All: Do both - clear the visible viewport and the scrollback, leaving
    //     only the cursor row at the top of the viewport.
    // Arguments:
    // - clearType: The type of clear to perform.
    // Return Value:
    // - <none>
    void ControlCore::ClearBuffer(Control::ClearBufferType clearType)
    {
        if (clearType == Control::ClearBufferType::Scrollback || clearType == Control::ClearBufferType::All)
        {
            const auto lock = _terminal->LockForWriting();
            _terminal->EraseScrollback();
        }

        if (clearType == Control::ClearBufferType::Screen || clearType == Control::ClearBufferType::All)
        {
            // Send a signal to conpty to clear the buffer.
            if (auto conpty{ _connection.try_as<TerminalConnection::ConptyConnection>() })
            {
                // ConPTY will emit sequences to sync up our buffer with its new
                // contents.
                conpty.ClearBuffer();
            }
        }
    }

    hstring ControlCore::ReadEntireBuffer() const
    {
        const auto lock = _terminal->LockForWriting();

        const auto& textBuffer = _terminal->GetTextBuffer();

        std::wstring str;
        const auto lastRow = textBuffer.GetLastNonSpaceCharacter().y;
        for (auto rowIndex = 0; rowIndex <= lastRow; rowIndex++)
        {
            const auto& row = textBuffer.GetRowByOffset(rowIndex);
            const auto rowText = row.GetText();
            const auto strEnd = rowText.find_last_not_of(UNICODE_SPACE);
            if (strEnd != decltype(rowText)::npos)
            {
                str.append(rowText.substr(0, strEnd + 1));
            }

            if (!row.WasWrapForced())
            {
                str.append(L"\r\n");
            }
        }

        return hstring{ str };
    }

    // Get all of our recent commands. This will only really work if the user has enabled shell integration.
    Control::CommandHistoryContext ControlCore::CommandHistory() const
    {
        const auto lock = _terminal->LockForWriting();
        const auto& textBuffer = _terminal->GetTextBuffer();

        std::vector<winrt::hstring> commands;

        for (const auto& mark : _terminal->GetScrollMarks())
        {
            // The command text is between the `end` (which denotes the end of
            // the prompt) and the `commandEnd`.
            bool markHasCommand = mark.commandEnd.has_value() &&
                                  mark.commandEnd != mark.end;
            if (!markHasCommand)
            {
                continue;
            }

            // Get the text of the command
            const auto line = mark.end.y;
            const auto& row = textBuffer.GetRowByOffset(line);
            const auto commandText = row.GetText(mark.end.x, mark.commandEnd->x);

            // Trim off trailing spaces.
            const auto strEnd = commandText.find_last_not_of(UNICODE_SPACE);
            if (strEnd != std::string::npos)
            {
                const auto trimmed = commandText.substr(0, strEnd + 1);

                commands.push_back(winrt::hstring{ trimmed });
            }
        }
        auto context = winrt::make_self<CommandHistoryContext>(std::move(commands));
        context->CurrentCommandline(winrt::hstring{ _terminal->CurrentCommand() });

        return *context;
    }

    Core::Scheme ControlCore::ColorScheme() const noexcept
    {
        Core::Scheme s;

        // This part is definitely a hack.
        //
        // This function is usually used by the "Preview Color Scheme"
        // functionality in TerminalPage. If we've got an unfocused appearance,
        // then we've applied that appearance before this is even getting called
        // (because the command palette is open with focus on top of us). If we
        // return the _current_ colors now, we'll return out the _unfocused_
        // colors. If we do that, and the user dismisses the command palette,
        // then the scheme that will get restored is the _unfocused_ one, which
        // is not what we want.
        //
        // So if that's the case, then let's grab the colors from the focused
        // appearance as the scheme instead. We'll lose any current runtime
        // changes to the color table, but those were already blown away when we
        // switched to an unfocused appearance.
        //
        // IF WE DON'T HAVE AN UNFOCUSED APPEARANCE: then just ask the Terminal
        // for its current color table. That way, we can restore those colors
        // back.
        if (HasUnfocusedAppearance())
        {
            s.Foreground = _settings->FocusedAppearance()->DefaultForeground();
            s.Background = _settings->FocusedAppearance()->DefaultBackground();

            s.CursorColor = _settings->FocusedAppearance()->CursorColor();

            s.Black = _settings->FocusedAppearance()->GetColorTableEntry(0);
            s.Red = _settings->FocusedAppearance()->GetColorTableEntry(1);
            s.Green = _settings->FocusedAppearance()->GetColorTableEntry(2);
            s.Yellow = _settings->FocusedAppearance()->GetColorTableEntry(3);
            s.Blue = _settings->FocusedAppearance()->GetColorTableEntry(4);
            s.Purple = _settings->FocusedAppearance()->GetColorTableEntry(5);
            s.Cyan = _settings->FocusedAppearance()->GetColorTableEntry(6);
            s.White = _settings->FocusedAppearance()->GetColorTableEntry(7);
            s.BrightBlack = _settings->FocusedAppearance()->GetColorTableEntry(8);
            s.BrightRed = _settings->FocusedAppearance()->GetColorTableEntry(9);
            s.BrightGreen = _settings->FocusedAppearance()->GetColorTableEntry(10);
            s.BrightYellow = _settings->FocusedAppearance()->GetColorTableEntry(11);
            s.BrightBlue = _settings->FocusedAppearance()->GetColorTableEntry(12);
            s.BrightPurple = _settings->FocusedAppearance()->GetColorTableEntry(13);
            s.BrightCyan = _settings->FocusedAppearance()->GetColorTableEntry(14);
            s.BrightWhite = _settings->FocusedAppearance()->GetColorTableEntry(15);
        }
        else
        {
            const auto lock = _terminal->LockForReading();
            s = _terminal->GetColorScheme();
        }

        // This might be a tad bit of a hack. This event only gets called by set
        // color scheme / preview color scheme, and in that case, we know the
        // control _is_ focused.
        s.SelectionBackground = _settings->FocusedAppearance()->SelectionBackground();

        return s;
    }

    // Method Description:
    // - Apply the given color scheme to this control. We'll take the colors out
    //   of it and apply them to our focused appearance, and update the terminal
    //   buffer with the new color table.
    // - This is here to support the Set Color Scheme action, and the ability to
    //   preview schemes in the control.
    // Arguments:
    // - scheme: the collection of colors to apply.
    // Return Value:
    // - <none>
    void ControlCore::ColorScheme(const Core::Scheme& scheme)
    {
        _settings->FocusedAppearance()->DefaultForeground(scheme.Foreground);
        _settings->FocusedAppearance()->DefaultBackground(scheme.Background);
        _settings->FocusedAppearance()->CursorColor(scheme.CursorColor);
        _settings->FocusedAppearance()->SelectionBackground(scheme.SelectionBackground);

        _settings->FocusedAppearance()->SetColorTableEntry(0, scheme.Black);
        _settings->FocusedAppearance()->SetColorTableEntry(1, scheme.Red);
        _settings->FocusedAppearance()->SetColorTableEntry(2, scheme.Green);
        _settings->FocusedAppearance()->SetColorTableEntry(3, scheme.Yellow);
        _settings->FocusedAppearance()->SetColorTableEntry(4, scheme.Blue);
        _settings->FocusedAppearance()->SetColorTableEntry(5, scheme.Purple);
        _settings->FocusedAppearance()->SetColorTableEntry(6, scheme.Cyan);
        _settings->FocusedAppearance()->SetColorTableEntry(7, scheme.White);
        _settings->FocusedAppearance()->SetColorTableEntry(8, scheme.BrightBlack);
        _settings->FocusedAppearance()->SetColorTableEntry(9, scheme.BrightRed);
        _settings->FocusedAppearance()->SetColorTableEntry(10, scheme.BrightGreen);
        _settings->FocusedAppearance()->SetColorTableEntry(11, scheme.BrightYellow);
        _settings->FocusedAppearance()->SetColorTableEntry(12, scheme.BrightBlue);
        _settings->FocusedAppearance()->SetColorTableEntry(13, scheme.BrightPurple);
        _settings->FocusedAppearance()->SetColorTableEntry(14, scheme.BrightCyan);
        _settings->FocusedAppearance()->SetColorTableEntry(15, scheme.BrightWhite);

        const auto lock = _terminal->LockForWriting();
        _terminal->ApplyScheme(scheme);
        _renderEngine->SetSelectionBackground(til::color{ _settings->SelectionBackground() });
        _renderer->TriggerRedrawAll(true);
    }

    bool ControlCore::HasUnfocusedAppearance() const
    {
        return _settings->HasUnfocusedAppearance();
    }

    void ControlCore::AdjustOpacity(const double opacityAdjust, const bool relative)
    {
        if (relative)
        {
            AdjustOpacity(opacityAdjust);
        }
        else
        {
            _setOpacity(opacityAdjust);
        }
    }

    // Method Description:
    // - Notifies the attached PTY that the window has changed visibility state
    // - NOTE: Most VT commands are generated in `TerminalDispatch` and sent to this
    //         class as the target for transmission. But since this message isn't
    //         coming in via VT parsing (and rather from a window state transition)
    //         we generate and send it here.
    // Arguments:
    // - visible: True for visible; false for not visible.
    // Return Value:
    // - <none>
    void ControlCore::WindowVisibilityChanged(const bool showOrHide)
    {
        if (_initializedTerminal.load(std::memory_order_relaxed))
        {
            // show is true, hide is false
            if (auto conpty{ _connection.try_as<TerminalConnection::ConptyConnection>() })
            {
                conpty.ShowHide(showOrHide);
            }
        }
    }

    // Method Description:
    // - When the control gains focus, it needs to tell ConPTY about this.
    //   Usually, these sequences are reserved for applications that
    //   specifically request SET_FOCUS_EVENT_MOUSE, ?1004h. ConPTY uses this
    //   sequence REGARDLESS to communicate if the control was focused or not.
    // - Even if a client application disables this mode, the Terminal & conpty
    //   should always request this from the hosting terminal (and just ignore
    //   internally to ConPTY).
    // - Full support for this sequence is tracked in GH#11682.
    // - This is related to work done for GH#2988.
    void ControlCore::GotFocus()
    {
        _focusChanged(true);
    }

    // See GotFocus.
    void ControlCore::LostFocus()
    {
        _focusChanged(false);
    }

    void ControlCore::_focusChanged(bool focused)
    {
        TerminalInput::OutputType out;
        {
            const auto lock = _terminal->LockForReading();
            out = _terminal->FocusChanged(focused);
        }
        if (out && !out->empty())
        {
            // _sendInputToConnection() asserts that we aren't in focus mode,
            // but window focus events are always fine to send.
            _connection.WriteInput(*out);
        }
    }

    bool ControlCore::_isBackgroundTransparent()
    {
        // If we're:
        // * Not fully opaque
        // * rendering on top of an image
        //
        // then the renderer should not render "default background" text with a
        // fully opaque background. Doing that would cover up our nice
        // transparency, or our acrylic, or our image.
        return Opacity() < 1.0f || !_settings->BackgroundImage().empty() || _settings->UseBackgroundImageForWindow();
    }

    uint64_t ControlCore::OwningHwnd()
    {
        return _owningHwnd;
    }

    void ControlCore::OwningHwnd(uint64_t owner)
    {
        if (owner != _owningHwnd && _connection)
        {
            if (auto conpty{ _connection.try_as<TerminalConnection::ConptyConnection>() })
            {
                conpty.ReparentWindow(owner);
            }
        }
        _owningHwnd = owner;
    }

    Windows::Foundation::Collections::IVector<Control::ScrollMark> ControlCore::ScrollMarks() const
    {
        const auto lock = _terminal->LockForReading();
        const auto& internalMarks = _terminal->GetScrollMarks();
        std::vector<Control::ScrollMark> v;

        v.reserve(internalMarks.size());

        for (const auto& mark : internalMarks)
        {
            v.emplace_back(
                mark.start.to_core_point(),
                mark.end.to_core_point(),
                OptionalFromColor(_terminal->GetColorForMark(mark)));
        }

        return winrt::single_threaded_vector(std::move(v));
    }

    void ControlCore::AddMark(const Control::ScrollMark& mark)
    {
        const auto lock = _terminal->LockForReading();
        ::ScrollMark m{};

        if (mark.Color.HasValue)
        {
            m.color = til::color{ mark.Color.Color };
        }

        if (_terminal->IsSelectionActive())
        {
            m.start = til::point{ _terminal->GetSelectionAnchor() };
            m.end = til::point{ _terminal->GetSelectionEnd() };
        }
        else
        {
            m.start = m.end = til::point{ _terminal->GetTextBuffer().GetCursor().GetPosition() };
        }

        // The version of this that only accepts a ScrollMark will automatically
        // set the start & end to the cursor position.
        _terminal->AddMark(m, m.start, m.end, true);
    }

    void ControlCore::ClearMark()
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->ClearMark();
    }

    void ControlCore::ClearAllMarks()
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->ClearAllMarks();
    }

    void ControlCore::ScrollToMark(const Control::ScrollToMarkDirection& direction)
    {
        const auto lock = _terminal->LockForWriting();
        const auto currentOffset = ScrollOffset();
        const auto& marks{ _terminal->GetScrollMarks() };

        std::optional<::ScrollMark> tgt;

        switch (direction)
        {
        case ScrollToMarkDirection::Last:
        {
            int highest = currentOffset;
            for (const auto& mark : marks)
            {
                const auto newY = mark.start.y;
                if (newY > highest)
                {
                    tgt = mark;
                    highest = newY;
                }
            }
            break;
        }
        case ScrollToMarkDirection::First:
        {
            int lowest = currentOffset;
            for (const auto& mark : marks)
            {
                const auto newY = mark.start.y;
                if (newY < lowest)
                {
                    tgt = mark;
                    lowest = newY;
                }
            }
            break;
        }
        case ScrollToMarkDirection::Next:
        {
            int minDistance = INT_MAX;
            for (const auto& mark : marks)
            {
                const auto delta = mark.start.y - currentOffset;
                if (delta > 0 && delta < minDistance)
                {
                    tgt = mark;
                    minDistance = delta;
                }
            }
            break;
        }
        case ScrollToMarkDirection::Previous:
        default:
        {
            int minDistance = INT_MAX;
            for (const auto& mark : marks)
            {
                const auto delta = currentOffset - mark.start.y;
                if (delta > 0 && delta < minDistance)
                {
                    tgt = mark;
                    minDistance = delta;
                }
            }
            break;
        }
        }

        const auto viewHeight = ViewHeight();
        const auto bufferSize = BufferHeight();

        // UserScrollViewport, to update the Terminal about where the viewport should be
        // then raise a _terminalScrollPositionChanged to inform the control to update the scrollbar.
        if (tgt.has_value())
        {
            UserScrollViewport(tgt->start.y);
            _terminalScrollPositionChanged(tgt->start.y, viewHeight, bufferSize);
        }
        else
        {
            if (direction == ScrollToMarkDirection::Last || direction == ScrollToMarkDirection::Next)
            {
                UserScrollViewport(BufferHeight());
                _terminalScrollPositionChanged(BufferHeight(), viewHeight, bufferSize);
            }
            else if (direction == ScrollToMarkDirection::First || direction == ScrollToMarkDirection::Previous)
            {
                UserScrollViewport(0);
                _terminalScrollPositionChanged(0, viewHeight, bufferSize);
            }
        }
    }

    winrt::fire_and_forget ControlCore::_terminalCompletionsChanged(std::wstring_view menuJson,
                                                                    unsigned int replaceLength)
    {
        auto args = winrt::make_self<CompletionsChangedEventArgs>(winrt::hstring{ menuJson },
                                                                  replaceLength);

        co_await winrt::resume_background();

        _CompletionsChangedHandlers(*this, *args);
    }
    void ControlCore::_selectSpan(til::point_span s)
    {
        const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
        bufferSize.DecrementInBounds(s.end);

        _terminal->SelectNewRegion(s.start, s.end);
        _renderer->TriggerSelection();
    }

    void ControlCore::SelectCommand(const bool goUp)
    {
        const auto lock = _terminal->LockForWriting();

        const til::point start = _terminal->IsSelectionActive() ? (goUp ? _terminal->GetSelectionAnchor() : _terminal->GetSelectionEnd()) :
                                                                  _terminal->GetTextBuffer().GetCursor().GetPosition();

        std::optional<::ScrollMark> nearest{ std::nullopt };
        const auto& marks{ _terminal->GetScrollMarks() };

        // Early return so we don't have to check for the validity of `nearest` below after the loop exits.
        if (marks.empty())
        {
            return;
        }

        static constexpr til::point worst{ til::CoordTypeMax, til::CoordTypeMax };
        til::point bestDistance{ worst };

        for (const auto& m : marks)
        {
            if (!m.HasCommand())
            {
                continue;
            }

            const auto distance = goUp ? start - m.end : m.end - start;
            if ((distance > til::point{ 0, 0 }) && distance < bestDistance)
            {
                nearest = m;
                bestDistance = distance;
            }
        }

        if (nearest.has_value())
        {
            const auto start = nearest->end;
            auto end = *nearest->commandEnd;
            _selectSpan(til::point_span{ start, end });
        }
    }

    void ControlCore::SelectOutput(const bool goUp)
    {
        const auto lock = _terminal->LockForWriting();

        const til::point start = _terminal->IsSelectionActive() ? (goUp ? _terminal->GetSelectionAnchor() : _terminal->GetSelectionEnd()) :
                                                                  _terminal->GetTextBuffer().GetCursor().GetPosition();

        std::optional<::ScrollMark> nearest{ std::nullopt };
        const auto& marks{ _terminal->GetScrollMarks() };

        static constexpr til::point worst{ til::CoordTypeMax, til::CoordTypeMax };
        til::point bestDistance{ worst };

        for (const auto& m : marks)
        {
            if (!m.HasOutput())
            {
                continue;
            }

            const auto distance = goUp ? start - *m.commandEnd : *m.commandEnd - start;
            if ((distance > til::point{ 0, 0 }) && distance < bestDistance)
            {
                nearest = m;
                bestDistance = distance;
            }
        }

        if (nearest.has_value())
        {
            const auto start = *nearest->commandEnd;
            auto end = *nearest->outputEnd;
            _selectSpan(til::point_span{ start, end });
        }
    }

    void ControlCore::ColorSelection(const Control::SelectionColor& fg, const Control::SelectionColor& bg, Core::MatchMode matchMode)
    {
        const auto lock = _terminal->LockForWriting();

        if (_terminal->IsSelectionActive())
        {
            const auto pForeground = winrt::get_self<implementation::SelectionColor>(fg);
            const auto pBackground = winrt::get_self<implementation::SelectionColor>(bg);

            TextColor foregroundAsTextColor;
            TextColor backgroundAsTextColor;

            if (pForeground)
            {
                foregroundAsTextColor = pForeground->AsTextColor();
            }

            if (pBackground)
            {
                backgroundAsTextColor = pBackground->AsTextColor();
            }

            TextAttribute attr;
            attr.SetForeground(foregroundAsTextColor);
            attr.SetBackground(backgroundAsTextColor);

            _terminal->ColorSelection(attr, matchMode);
            _terminal->ClearSelection();
            if (matchMode != Core::MatchMode::None)
            {
                // ClearSelection will invalidate the selection area... but if we are
                // coloring other matches, then we need to make sure those get redrawn,
                // too.
                _renderer->TriggerRedrawAll();
            }
        }
    }

    void ControlCore::AnchorContextMenu(const til::point viewportRelativeCharacterPosition)
    {
        // viewportRelativeCharacterPosition is relative to the current
        // viewport, so adjust for that:
        const auto lock = _terminal->LockForReading();
        _contextMenuBufferPosition = _terminal->GetViewport().Origin() + viewportRelativeCharacterPosition;
    }

    void ControlCore::_contextMenuSelectMark(
        const til::point& pos,
        bool (*filter)(const ::ScrollMark&),
        til::point_span (*getSpan)(const ::ScrollMark&))
    {
        const auto lock = _terminal->LockForWriting();

        // Do nothing if the caller didn't give us a way to get the span to select for this mark.
        if (!getSpan)
        {
            return;
        }
        const auto& marks{ _terminal->GetScrollMarks() };
        for (auto&& m : marks)
        {
            // If the caller gave us a way to filter marks, check that now.
            // This can be used to filter to only marks that have a command, or output.
            if (filter && filter(m))
            {
                continue;
            }
            // If they clicked _anywhere_ in the mark...
            const auto [markStart, markEnd] = m.GetExtent();
            if (markStart <= pos &&
                markEnd >= pos)
            {
                // ... select the part of the mark the caller told us about.
                _selectSpan(getSpan(m));
                // And quick bail
                return;
            }
        }
    }

    void ControlCore::ContextMenuSelectCommand()
    {
        _contextMenuSelectMark(
            _contextMenuBufferPosition,
            [](const ::ScrollMark& m) -> bool { return !m.HasCommand(); },
            [](const ::ScrollMark& m) { return til::point_span{ m.end, *m.commandEnd }; });
    }
    void ControlCore::ContextMenuSelectOutput()
    {
        _contextMenuSelectMark(
            _contextMenuBufferPosition,
            [](const ::ScrollMark& m) -> bool { return !m.HasOutput(); },
            [](const ::ScrollMark& m) { return til::point_span{ *m.commandEnd, *m.outputEnd }; });
    }

    bool ControlCore::_clickedOnMark(
        const til::point& pos,
        bool (*filter)(const ::ScrollMark&))
    {
        const auto lock = _terminal->LockForWriting();

        // Don't show this if the click was on the selection
        if (_terminal->IsSelectionActive() &&
            _terminal->GetSelectionAnchor() <= pos &&
            _terminal->GetSelectionEnd() >= pos)
        {
            return false;
        }

        // DO show this if the click was on a mark with a command
        const auto& marks{ _terminal->GetScrollMarks() };
        for (auto&& m : marks)
        {
            if (filter && filter(m))
            {
                continue;
            }
            const auto [start, end] = m.GetExtent();
            if (start <= pos &&
                end >= pos)
            {
                return true;
            }
        }

        // Didn't click on a mark with a command - don't show.
        return false;
    }

    // Method Description:
    // * Don't show this if the click was on the _current_ selection
    // * Don't show this if the click wasn't on a mark with at least a command
    // * Otherwise yea, show it.
    bool ControlCore::ShouldShowSelectCommand()
    {
        // Relies on the anchor set in AnchorContextMenu
        return _clickedOnMark(_contextMenuBufferPosition,
                              [](const ::ScrollMark& m) -> bool { return !m.HasCommand(); });
    }

    // Method Description:
    // * Same as ShouldShowSelectCommand, but with the mark needing output
    bool ControlCore::ShouldShowSelectOutput()
    {
        // Relies on the anchor set in AnchorContextMenu
        return _clickedOnMark(_contextMenuBufferPosition,
                              [](const ::ScrollMark& m) -> bool { return !m.HasOutput(); });
    }
}
