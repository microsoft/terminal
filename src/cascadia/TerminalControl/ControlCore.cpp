// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ControlCore.h"

// MidiAudio
#include <mmeapi.h>
#include <dsound.h>

#include <DefaultSettings.h>
#include <LibraryResources.h>
#include <unicode.hpp>
#include <utils.hpp>
#include <WinUser.h>

#include "EventArgs.h"
#include "../../renderer/atlas/AtlasEngine.h"
#include "../../renderer/base/renderer.hpp"
#include "../../renderer/uia/UiaRenderer.hpp"
#include "../../types/inc/CodepointWidthDetector.hpp"

#include "ControlCore.g.cpp"
#include "SelectionColor.g.cpp"

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Console::VirtualTerminal;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    static winrt::Microsoft::Terminal::Core::OptionalColor OptionalFromColor(const til::color& c) noexcept
    {
        Core::OptionalColor result;
        result.Color = c;
        result.HasValue = true;
        return result;
    }

    static ::Microsoft::Console::Render::Atlas::GraphicsAPI parseGraphicsAPI(GraphicsAPI api) noexcept
    {
        using GA = ::Microsoft::Console::Render::Atlas::GraphicsAPI;
        switch (api)
        {
        case GraphicsAPI::Direct2D:
            return GA::Direct2D;
        case GraphicsAPI::Direct3D11:
            return GA::Direct3D11;
        default:
            return GA::Automatic;
        }
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
        static const auto textMeasurementInit = [&]() {
            TextMeasurementMode mode = TextMeasurementMode::Graphemes;
            switch (settings.TextMeasurement())
            {
            case TextMeasurement::Wcswidth:
                mode = TextMeasurementMode::Wcswidth;
                break;
            case TextMeasurement::Console:
                mode = TextMeasurementMode::Console;
                break;
            default:
                break;
            }
            CodepointWidthDetector::Singleton().Reset(mode);
            return true;
        }();

        _settings = winrt::make_self<implementation::ControlSettings>(settings, unfocusedAppearance);
        _terminal = std::make_shared<::Microsoft::Terminal::Core::Terminal>();
        const auto lock = _terminal->LockForWriting();

        _setupDispatcherAndCallbacks();

        Connection(connection);

        _terminal->SetWriteInputCallback([this](std::wstring_view wstr) {
            _pendingResponses.append(wstr);
        });

        // GH#8969: pre-seed working directory to prevent potential races
        _terminal->SetWorkingDirectory(_settings->StartingDirectory());

        auto pfnCopyToClipboard = [this](auto&& PH1) { _terminalCopyToClipboard(std::forward<decltype(PH1)>(PH1)); };
        _terminal->SetCopyToClipboardCallback(pfnCopyToClipboard);

        auto pfnWarningBell = [this] { _terminalWarningBell(); };
        _terminal->SetWarningBellCallback(pfnWarningBell);

        auto pfnTitleChanged = [this](auto&& PH1) { _terminalTitleChanged(std::forward<decltype(PH1)>(PH1)); };
        _terminal->SetTitleChangedCallback(pfnTitleChanged);

        auto pfnScrollPositionChanged = [this](auto&& PH1, auto&& PH2, auto&& PH3) { _terminalScrollPositionChanged(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3)); };
        _terminal->SetScrollPositionChangedCallback(pfnScrollPositionChanged);

        auto pfnTerminalTaskbarProgressChanged = [this] { _terminalTaskbarProgressChanged(); };
        _terminal->TaskbarProgressChangedCallback(pfnTerminalTaskbarProgressChanged);

        auto pfnShowWindowChanged = [this](auto&& PH1) { _terminalShowWindowChanged(std::forward<decltype(PH1)>(PH1)); };
        _terminal->SetShowWindowCallback(pfnShowWindowChanged);

        auto pfnPlayMidiNote = [this](auto&& PH1, auto&& PH2, auto&& PH3) { _terminalPlayMidiNote(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2), std::forward<decltype(PH3)>(PH3)); };
        _terminal->SetPlayMidiNoteCallback(pfnPlayMidiNote);

        auto pfnCompletionsChanged = [=](auto&& menuJson, auto&& replaceLength) { _terminalCompletionsChanged(menuJson, replaceLength); };
        _terminal->CompletionsChangedCallback(pfnCompletionsChanged);

        auto pfnSearchMissingCommand = [this](auto&& PH1, auto&& PH2) { _terminalSearchMissingCommand(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); };
        _terminal->SetSearchMissingCommandCallback(pfnSearchMissingCommand);

        auto pfnClearQuickFix = [this] { ClearQuickFix(); };
        _terminal->SetClearQuickFixCallback(pfnClearQuickFix);

        auto pfnWindowSizeChanged = [this](auto&& PH1, auto&& PH2) { _terminalWindowSizeChanged(std::forward<decltype(PH1)>(PH1), std::forward<decltype(PH2)>(PH2)); };
        _terminal->SetWindowSizeChangedCallback(pfnWindowSizeChanged);

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
            _renderer->SetRendererEnteredErrorStateCallback([this]() { RendererEnteredErrorState.raise(nullptr, nullptr); });

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

        const auto shared = _shared.lock();
        // Raises an OutputIdle event once there hasn't been any output for at least 100ms.
        // It also updates all regex patterns in the viewport.
        //
        // NOTE: Calling UpdatePatternLocations from a background
        // thread is a workaround for us to hit GH#12607 less often.
        shared->outputIdle = std::make_unique<til::debounced_func_trailing<>>(
            std::chrono::milliseconds{ 100 },
            [weakTerminal = std::weak_ptr{ _terminal }, weakThis = get_weak(), dispatcher = _dispatcher]() {
                dispatcher.TryEnqueue(DispatcherQueuePriority::Normal, [weakThis]() {
                    if (const auto self = weakThis.get(); self && !self->_IsClosing())
                    {
                        self->OutputIdle.raise(*self, nullptr);
                    }
                });

                if (const auto t = weakTerminal.lock())
                {
                    const auto lock = t->LockForWriting();
                    t->UpdatePatternsUnderLock();
                }
            });

        // If you rapidly show/hide Windows Terminal, something about GotFocus()/LostFocus() gets broken.
        // We'll then receive easily 10+ such calls from WinUI the next time the application is shown.
        shared->focusChanged = std::make_unique<til::debounced_func_trailing<bool>>(
            std::chrono::milliseconds{ 25 },
            [weakThis = get_weak()](const bool focused) {
                if (const auto core{ weakThis.get() })
                {
                    core->_focusChanged(focused);
                }
            });

        // Scrollbar updates are also expensive (XAML), so we'll throttle them as well.
        shared->updateScrollBar = std::make_shared<ThrottledFuncTrailing<Control::ScrollPositionChangedArgs>>(
            _dispatcher,
            std::chrono::milliseconds{ 8 },
            [weakThis = get_weak()](const auto& update) {
                if (auto core{ weakThis.get() }; core && !core->_IsClosing())
                {
                    core->ScrollPositionChanged.raise(*core, update);
                }
            });
    }

    ControlCore::~ControlCore()
    {
        Close();

        _renderer.reset();
        _renderEngine.reset();
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
        shared->outputIdle.reset();
        shared->updateScrollBar.reset();
    }

    void ControlCore::AttachToNewControl(const Microsoft::Terminal::Control::IKeyBindings& keyBindings)
    {
        _settings->KeyBindings(keyBindings);
        _setupDispatcherAndCallbacks();
        const auto actualNewSize = _actualFont.GetSize();
        // Bubble this up, so our new control knows how big we want the font.
        FontSizeChanged.raise(*this, winrt::make<FontSizeChangedArgs>(actualNewSize.width, actualNewSize.height));

        // The renderer will be re-enabled in Initialize

        Attached.raise(*this, nullptr);
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
                ConnectionStateChanged.raise(*this, nullptr);
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
            ConnectionStateChanged.raise(*this, nullptr);
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

            _renderEngine = std::make_unique<::Microsoft::Console::Render::AtlasEngine>();
            _renderer->AddRenderEngine(_renderEngine.get());

            // Hook up the warnings callback as early as possible so that we catch everything.
            _renderEngine->SetWarningCallback([this](HRESULT hr, wil::zwstring_view parameter) {
                _rendererWarning(hr, parameter);
            });

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

            // Tell the render engine to notify us when the swap chain changes.
            // We do this after we initially set the swapchain so as to avoid
            // unnecessary callbacks (and locking problems)
            _renderEngine->SetCallback([this](HANDLE handle) {
                _renderEngineSwapChainChanged(handle);
            });

            _renderEngine->SetRetroTerminalEffect(_settings->RetroTerminalEffect());
            _renderEngine->SetPixelShaderPath(_settings->PixelShaderPath());
            _renderEngine->SetPixelShaderImagePath(_settings->PixelShaderImagePath());
            _renderEngine->SetGraphicsAPI(parseGraphicsAPI(_settings->GraphicsAPI()));
            _renderEngine->SetDisablePartialInvalidation(_settings->DisablePartialInvalidation());
            _renderEngine->SetSoftwareRendering(_settings->SoftwareRendering());

            _updateAntiAliasingMode();

            // GH#5098: Inform the engine of the opacity of the default text background.
            // GH#11315: Always do this, even if they don't have acrylic on.
            _renderEngine->EnableTransparentBackground(_isBackgroundTransparent());

            _initializedTerminal.store(true, std::memory_order_relaxed);
        } // scope for TerminalLock

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
        _connection.WriteInput(winrt_wstring_to_array_view(wstr));
    }

    // Method Description:
    // - Writes the given sequence as input to the active terminal connection,
    // Arguments:
    // - wstr: the string of characters to write to the terminal connection.
    // Return Value:
    // - <none>
    void ControlCore::SendInput(const std::wstring_view wstr)
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
            _sendInputToConnection(wstr);
        }
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
                CloseTerminalRequested.raise(*this, nullptr);
                return true;
            }

            if (ch == Enter)
            {
                // Ask the hosting application to give us a new connection.
                RestartTerminalRequested.raise(*this, nullptr);
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
            SendInput(*out);
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
                    OpenHyperlink.raise(*this, winrt::make<OpenHyperlinkEventArgs>(winrt::hstring{ uri }));
                }
                else
                {
                    const auto selectedText = _terminal->GetTextBuffer().GetPlainText(_terminal->GetSelectionAnchor(), _terminal->GetSelectionEnd());
                    lock.unlock();
                    OpenHyperlink.raise(*this, winrt::make<OpenHyperlinkEventArgs>(winrt::hstring{ selectedText }));
                }
                return true;
            }
            else if (vkey == VK_RETURN && !mods.IsCtrlPressed() && !mods.IsAltPressed())
            {
                // [Shift +] Enter --> copy text
                CopySelectionToClipboard(mods.IsShiftPressed(), false, nullptr);
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
            SendInput(*out);
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
            SendInput(*out);
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
        if (shared->outputIdle)
        {
            (*shared->outputIdle)();
        }
    }

    void ControlCore::AdjustOpacity(const float adjustment)
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
    void ControlCore::_setOpacity(const float opacity, bool focused)
    {
        const auto newOpacity = std::clamp(opacity, 0.0f, 1.0f);

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
        _runtimeUseAcrylic = newOpacity < 1.0f && _settings->UseAcrylic();

        // Update the renderer as well. It might need to fall back from
        // cleartype -> grayscale if the BG is transparent / acrylic.
        if (_renderEngine)
        {
            const auto lock = _terminal->LockForWriting();
            _renderEngine->EnableTransparentBackground(_isBackgroundTransparent());
            _renderer->NotifyPaintFrame();
        }

        auto eventArgs = winrt::make_self<TransparencyChangedEventArgs>(newOpacity);
        TransparencyChanged.raise(*this, *eventArgs);
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

            HoveredHyperlinkChanged.raise(*this, nullptr);
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

        _builtinGlyphs = _settings->EnableBuiltinGlyphs();
        _colorGlyphs = _settings->EnableColorGlyphs();
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

        _renderEngine->SetGraphicsAPI(parseGraphicsAPI(_settings->GraphicsAPI()));
        _renderEngine->SetDisablePartialInvalidation(_settings->DisablePartialInvalidation());
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
    void ControlCore::ApplyAppearance(const bool focused)
    {
        const auto lock = _terminal->LockForWriting();
        const auto& newAppearance{ focused ? _settings->FocusedAppearance() : _settings->UnfocusedAppearance() };
        // Update the terminal core with its new Core settings
        _terminal->UpdateAppearance(*newAppearance);

        // Update AtlasEngine settings under the lock
        if (_renderEngine)
        {
            // Update AtlasEngine settings under the lock
            _renderEngine->SetRetroTerminalEffect(newAppearance->RetroTerminalEffect());
            _renderEngine->SetPixelShaderPath(newAppearance->PixelShaderPath());
            _renderEngine->SetPixelShaderImagePath(newAppearance->PixelShaderImagePath());

            // Incase EnableUnfocusedAcrylic is disabled and Focused Acrylic is set to true,
            // the terminal should ignore the unfocused opacity from settings.
            // The Focused Opacity from settings should be ignored if overridden at runtime.
            const auto useFocusedRuntimeOpacity = focused || (!_settings->EnableUnfocusedAcrylic() && UseAcrylic());
            const auto newOpacity = useFocusedRuntimeOpacity ? FocusedOpacity() : newAppearance->Opacity();
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
            TransparencyChanged.raise(*this, *eventArgs);

            _renderer->TriggerRedrawAll(true, true);
        }
    }

    Control::IControlSettings ControlCore::Settings()
    {
        return *_settings;
    }

    Control::IControlAppearance ControlCore::FocusedAppearance() const
    {
        return *_settings->FocusedAppearance();
    }

    Control::IControlAppearance ControlCore::UnfocusedAppearance() const
    {
        return *_settings->UnfocusedAppearance();
    }

    void ControlCore::_updateAntiAliasingMode()
    {
        D2D1_TEXT_ANTIALIAS_MODE mode;
        // Update AtlasEngine's AntialiasingMode
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
            static constexpr auto cloneMap = [](const IFontFeatureMap& map) {
                std::unordered_map<std::wstring_view, float> clone;
                if (map)
                {
                    clone.reserve(map.Size());
                    for (const auto& [tag, param] : map)
                    {
                        clone.emplace(tag, param);
                    }
                }
                return clone;
            };

            const auto fontFeatures = _settings->FontFeatures();
            const auto fontAxes = _settings->FontAxes();
            const auto featureMap = cloneMap(fontFeatures);
            const auto axesMap = cloneMap(fontAxes);

            // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
            //      actually fail. We need a way to gracefully fallback.
            LOG_IF_FAILED(_renderEngine->UpdateDpi(newDpi));
            LOG_IF_FAILED(_renderEngine->UpdateFont(_desiredFont, _actualFont, featureMap, axesMap));
        }

        const auto actualNewSize = _actualFont.GetSize();
        FontSizeChanged.raise(*this, winrt::make<FontSizeChangedArgs>(actualNewSize.width, actualNewSize.height));
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

        _desiredFont.SetEnableBuiltinGlyphs(_builtinGlyphs);
        _desiredFont.SetEnableColorGlyphs(_colorGlyphs);
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
        if (FAILED(hr) || hr == S_FALSE)
        {
            return;
        }

        _connection.Resize(vp.Height(), vp.Width());

        // TermControl will call Search() once the OutputIdle even fires after 100ms.
        // Until then we need to hide the now-stale search results from the renderer.
        ClearSearch();
        const auto shared = _shared.lock_shared();
        if (shared->outputIdle)
        {
            (*shared->outputIdle)();
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
        info.EndAtRightBoundary = _terminal->GetSelectionEnd().x == bufferSize.RightExclusive();
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

        // clamp the converted position to be within the viewport bounds
        // x: allow range of [0, RightExclusive]
        // GH #18106: right exclusive needed for selection to support exclusive end
        til::point terminalPosition{
            std::clamp(position.x, 0, _terminal->GetViewport().Width()),
            std::clamp(position.y, 0, _terminal->GetViewport().Height() - 1)
        };

        // save location (for rendering) + render
        _terminal->SetSelectionEnd(terminalPosition);
        _updateSelectionUI();
    }

    static wil::unique_close_clipboard_call _openClipboard(HWND hwnd)
    {
        bool success = false;

        // OpenClipboard may fail to acquire the internal lock --> retry.
        for (DWORD sleep = 10;; sleep *= 2)
        {
            if (OpenClipboard(hwnd))
            {
                success = true;
                break;
            }
            // 10 iterations
            if (sleep > 10000)
            {
                break;
            }
            Sleep(sleep);
        }

        return wil::unique_close_clipboard_call{ success };
    }

    static void _copyToClipboard(const UINT format, const void* src, const size_t bytes)
    {
        wil::unique_hglobal handle{ THROW_LAST_ERROR_IF_NULL(GlobalAlloc(GMEM_MOVEABLE, bytes)) };

        const auto locked = GlobalLock(handle.get());
        memcpy(locked, src, bytes);
        GlobalUnlock(handle.get());

        THROW_LAST_ERROR_IF_NULL(SetClipboardData(format, handle.get()));
        handle.release();
    }

    static void _copyToClipboardRegisteredFormat(const wchar_t* format, const void* src, size_t bytes)
    {
        const auto id = RegisterClipboardFormatW(format);
        if (!id)
        {
            LOG_LAST_ERROR();
            return;
        }
        _copyToClipboard(id, src, bytes);
    }

    static void copyToClipboard(wil::zwstring_view text, std::string_view html, std::string_view rtf)
    {
        const auto clipboard = _openClipboard(nullptr);
        if (!clipboard)
        {
            LOG_LAST_ERROR();
            return;
        }

        EmptyClipboard();

        if (!text.empty())
        {
            // As per: https://learn.microsoft.com/en-us/windows/win32/dataxchg/standard-clipboard-formats
            //   CF_UNICODETEXT: [...] A null character signals the end of the data.
            // --> We add +1 to the length. This works because .c_str() is null-terminated.
            _copyToClipboard(CF_UNICODETEXT, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
        }

        if (!html.empty())
        {
            _copyToClipboardRegisteredFormat(L"HTML Format", html.data(), html.size());
        }

        if (!rtf.empty())
        {
            _copyToClipboardRegisteredFormat(L"Rich Text Format", rtf.data(), rtf.size());
        }
    }

    // Called when the Terminal wants to set something to the clipboard, i.e.
    // when an OSC 52 is emitted.
    void ControlCore::_terminalCopyToClipboard(wil::zwstring_view wstr)
    {
        copyToClipboard(wstr, {}, {});
    }

    // Method Description:
    // - Given a copy-able selection, get the selected text from the buffer and send it to the
    //     Windows Clipboard (CascadiaWin32:main.cpp).
    // Arguments:
    // - singleLine: collapse all of the text to one line
    // - withControlSequences: if enabled, the copied plain text contains color/style ANSI escape codes from the selection
    // - formats: which formats to copy (defined by action's CopyFormatting arg). nullptr
    //             if we should defer which formats are copied to the global setting
    bool ControlCore::CopySelectionToClipboard(bool singleLine,
                                               bool withControlSequences,
                                               const Windows::Foundation::IReference<CopyFormat>& formats)
    {
        ::Microsoft::Terminal::Core::Terminal::TextCopyData payload;
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
            payload = _terminal->RetrieveSelectedTextFromBuffer(singleLine, withControlSequences, copyHtml, copyRtf);
        }

        copyToClipboard(payload.plainText, payload.html, payload.rtf);
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
        SendInput(filtered);

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
            static_cast<float>(fontSize.width),
            static_cast<float>(fontSize.height)
        };
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
        WarningBell.raise(*this, nullptr);
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
        TitleChanged.raise(*this, winrt::make<TitleChangedEventArgs>(winrt::hstring{ wstr }));
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
            ScrollPositionChanged.raise(*this, update);
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

    void ControlCore::_terminalTaskbarProgressChanged()
    {
        TaskbarProgressChanged.raise(*this, nullptr);
    }

    void ControlCore::_terminalShowWindowChanged(bool showOrHide)
    {
        auto showWindow = winrt::make_self<implementation::ShowWindowArgs>(showOrHide);
        ShowWindowChanged.raise(*this, *showWindow);
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

    void ControlCore::_terminalWindowSizeChanged(int32_t width, int32_t height)
    {
        auto size = winrt::make<implementation::WindowSizeChangedEventArgs>(width, height);
        WindowSizeChanged.raise(*this, size);
    }

    void ControlCore::_terminalSearchMissingCommand(std::wstring_view missingCommand, const til::CoordType& bufferRow)
    {
        SearchMissingCommand.raise(*this, make<implementation::SearchMissingCommandEventArgs>(hstring{ missingCommand }, bufferRow));
    }

    void ControlCore::OpenCWD()
    {
        const auto workingDirectory = WorkingDirectory();
        ShellExecute(nullptr, nullptr, L"explorer", workingDirectory.c_str(), nullptr, SW_SHOW);
    }

    void ControlCore::ClearQuickFix()
    {
        _cachedQuickFixes = nullptr;
        RefreshQuickFixUI.raise(*this, nullptr);
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
    // - caseSensitive: boolean that represents if the current search is case-sensitive
    // - resetOnly: If true, only Reset() will be called, if anything. FindNext() will never be called.
    // Return Value:
    // - <none>
    SearchResults ControlCore::Search(SearchRequest request)
    {
        const auto lock = _terminal->LockForWriting();

        SearchFlag flags{};
        WI_SetFlagIf(flags, SearchFlag::CaseInsensitive, !request.CaseSensitive);
        WI_SetFlagIf(flags, SearchFlag::RegularExpression, request.RegularExpression);
        const auto searchInvalidated = _searcher.IsStale(*_terminal.get(), request.Text, flags);

        if (searchInvalidated || !request.ResetOnly)
        {
            std::vector<til::point_span> oldResults;
            til::point_span oldFocused;

            if (const auto focused = _terminal->GetSearchHighlightFocused())
            {
                oldFocused = *focused;
            }

            if (searchInvalidated)
            {
                oldResults = _searcher.ExtractResults();
                _searcher.Reset(*_terminal.get(), request.Text, flags, !request.GoForward);
                _terminal->SetSearchHighlights(_searcher.Results());
            }

            if (!request.ResetOnly)
            {
                _searcher.FindNext(!request.GoForward);
            }

            _terminal->SetSearchHighlightFocused(gsl::narrow<size_t>(std::max<ptrdiff_t>(0, _searcher.CurrentMatch())));
            _renderer->TriggerSearchHighlight(oldResults);

            if (const auto focused = _terminal->GetSearchHighlightFocused(); focused && *focused != oldFocused)
            {
                _terminal->ScrollToSearchHighlight(request.ScrollOffset);
            }
        }

        int32_t totalMatches = 0;
        int32_t currentMatch = 0;
        if (const auto idx = _searcher.CurrentMatch(); idx >= 0)
        {
            totalMatches = gsl::narrow<int32_t>(_searcher.Results().size());
            currentMatch = gsl::narrow<int32_t>(idx);
        }

        return {
            .TotalMatches = totalMatches,
            .CurrentMatch = currentMatch,
            .SearchInvalidated = searchInvalidated,
            .SearchRegexInvalid = !_searcher.IsOk(),
        };
    }

    const std::vector<til::point_span>& ControlCore::SearchResultRows() const noexcept
    {
        return _searcher.Results();
    }

    void ControlCore::ClearSearch()
    {
        const auto lock = _terminal->LockForWriting();
        _terminal->SetSearchHighlights({});
        _terminal->SetSearchHighlightFocused(0);
        _renderer->TriggerSearchHighlight(_searcher.Results());
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

    void ControlCore::PersistToPath(const wchar_t* path) const
    {
        const auto lock = _terminal->LockForReading();
        _terminal->SerializeMainBuffer(path);
    }

    void ControlCore::RestoreFromPath(const wchar_t* path) const
    {
        const wil::unique_handle file{ CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, nullptr) };
        if (!file)
        {
            return;
        }

        FILETIME lastWriteTime;
        SYSTEMTIME lastWriteSystemTime;
        if (!GetFileTime(file.get(), nullptr, nullptr, &lastWriteTime) ||
            !FileTimeToSystemTime(&lastWriteTime, &lastWriteSystemTime))
        {
            return;
        }

        wchar_t dateBuf[256];
        const auto dateLen = GetDateFormatEx(nullptr, 0, &lastWriteSystemTime, nullptr, &dateBuf[0], ARRAYSIZE(dateBuf), nullptr);
        wchar_t timeBuf[256];
        const auto timeLen = GetTimeFormatEx(nullptr, 0, &lastWriteSystemTime, nullptr, &timeBuf[0], ARRAYSIZE(timeBuf));

        std::wstring message;
        if (dateLen > 0 && timeLen > 0)
        {
            const auto msg = RS_(L"SessionRestoreMessage");
            const std::wstring_view date{ &dateBuf[0], gsl::narrow_cast<size_t>(dateLen) };
            const std::wstring_view time{ &timeBuf[0], gsl::narrow_cast<size_t>(timeLen) };
            // This escape sequence string
            // * sets the color to white on a bright black background ("\x1b[100;37m")
            // * prints "  [Restored <date> <time>]  <spaces until end of line>  "
            // * resets the color ("\x1b[m")
            // * newlines
            message = fmt::format(FMT_COMPILE(L"\x1b[100;37m  [{} {} {}]\x1b[K\x1b[m\r\n"), msg, date, time);
        }

        wchar_t buffer[32 * 1024];
        DWORD read = 0;

        // Ensure the text file starts with a UTF-16 BOM.
        if (!ReadFile(file.get(), &buffer[0], 2, &read, nullptr) || read < 2 || buffer[0] != L'\uFEFF')
        {
            return;
        }

        for (;;)
        {
            if (!ReadFile(file.get(), &buffer[0], sizeof(buffer), &read, nullptr))
            {
                break;
            }

            const auto lock = _terminal->LockForWriting();
            _terminal->Write({ &buffer[0], read / 2 });

            if (read < sizeof(buffer))
            {
                // Normally the cursor should already be at the start of the line, but let's be absolutely sure it is.
                if (_terminal->GetCursorPosition().x != 0)
                {
                    _terminal->Write(L"\r\n");
                }
                _terminal->Write(message);
                break;
            }
        }
    }

    void ControlCore::_rendererWarning(const HRESULT hr, wil::zwstring_view parameter)
    {
        RendererWarning.raise(*this, winrt::make<RendererWarningArgs>(hr, winrt::hstring{ parameter }));
    }

    safe_void_coroutine ControlCore::_renderEngineSwapChainChanged(const HANDLE sourceHandle)
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
            SwapChainChanged.raise(*this, winrt::box_value<uint64_t>(reinterpret_cast<uint64_t>(_lastSwapChainHandle.get())));
        }
    }

    void ControlCore::_rendererBackgroundColorChanged()
    {
        BackgroundColorChanged.raise(*this, nullptr);
    }

    void ControlCore::_rendererTabColorChanged()
    {
        TabColorChanged.raise(*this, nullptr);
    }

    void ControlCore::BlinkAttributeTick()
    {
        const auto lock = _terminal->LockForWriting();

        auto& renderSettings = _terminal->GetRenderSettings();
        renderSettings.ToggleBlinkRendition(_renderer.get());
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
            _repositionCursorWithMouse(terminalPosition);
        }
        _updateSelectionUI();
    }

    void ControlCore::_repositionCursorWithMouse(const til::point terminalPosition)
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
        const auto& marks{ _terminal->GetMarkExtents() };
        if (!marks.empty())
        {
            const auto& last{ marks.back() };
            const auto [start, end] = last.GetExtent();
            const auto bufferSize = _terminal->GetTextBuffer().GetSize();
            auto lastNonSpace = _terminal->GetTextBuffer().GetLastNonSpaceCharacter();
            bufferSize.IncrementInBounds(lastNonSpace, true);

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

            // terminalPosition is viewport-relative.
            const auto bufferPos = _terminal->GetViewport().Origin() + terminalPosition;
            if (bufferPos.y > lastNonSpace.y)
            {
                // Clicked under the prompt. Bail.
                return;
            }

            // Limit the click to 1 past the last character on the last line.
            const auto clampedClick = std::min(bufferPos, lastNonSpace);

            if (clampedClick >= last.end)
            {
                // Get the distance between the cursor and the click, in cells.

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

                {
                    // Sending input requires that we're unlocked, because
                    // writing the input pipe may block indefinitely.
                    const auto suspension = _terminal->SuspendLock();
                    SendInput(buffer);
                }
            }
        }
    }

    // Method Description:
    // - Updates the renderer's representation of the selection as well as the selection marker overlay in TermControl
    void ControlCore::_updateSelectionUI()
    {
        _renderer->TriggerSelection();
        // only show the markers if we're doing a keyboard selection or in mark mode
        const bool showMarkers{ _terminal->SelectionMode() >= ::Microsoft::Terminal::Core::Terminal::SelectionInteractionMode::Keyboard };
        UpdateSelectionMarkers.raise(*this, winrt::make<implementation::UpdateSelectionMarkersEventArgs>(!showMarkers));
    }

    void ControlCore::AttachUiaEngine(::Microsoft::Console::Render::UiaEngine* const pEngine)
    {
        // _renderer will always exist since it's introduced in the ctor
        const auto lock = _terminal->LockForWriting();
        _renderer->AddRenderEngine(pEngine);
    }
    void ControlCore::DetachUiaEngine(::Microsoft::Console::Render::UiaEngine* const pEngine)
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
        RaiseNotice.raise(*this, std::move(noticeArgs));
    }
    void ControlCore::_connectionOutputHandler(const hstring& hstr)
    {
        try
        {
            {
                const auto lock = _terminal->LockForWriting();
                _terminal->Write(hstr);
            }

            if (!_pendingResponses.empty())
            {
                _sendInputToConnection(_pendingResponses);
                _pendingResponses.clear();
            }

            // Start the throttled update of where our hyperlinks are.
            const auto shared = _shared.lock_shared();
            if (shared->outputIdle)
            {
                (*shared->outputIdle)();
            }
        }
        catch (...)
        {
            // We're expecting to receive an exception here if the terminal
            // is closed while we're blocked playing a MIDI note.
        }
    }

    ::Microsoft::Console::Render::Renderer* ControlCore::GetRenderer() const noexcept
    {
        return _renderer.get();
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
        std::wstring_view command;
        switch (clearType)
        {
        case ClearBufferType::Screen:
            command = L"\x1b[H\x1b[2J";
            break;
        case ClearBufferType::Scrollback:
            command = L"\x1b[3J";
            break;
        case ClearBufferType::All:
            command = L"\x1b[H\x1b[2J\x1b[3J";
            break;
        }

        {
            const auto lock = _terminal->LockForWriting();
            _terminal->Write(command);
        }

        if (clearType == Control::ClearBufferType::Screen || clearType == Control::ClearBufferType::All)
        {
            if (auto conpty{ _connection.try_as<TerminalConnection::ConptyConnection>() })
            {
                // Since the clearing of ConPTY occurs asynchronously, this call can result weird issues,
                // where a console application still sees contents that we've already deleted, etc.
                // The correct way would be for ConPTY to emit the appropriate CSI n J sequences.
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
        const auto bufferCommands{ textBuffer.Commands() };

        auto trimToHstring = [](const auto& s) -> winrt::hstring {
            const auto strEnd = s.find_last_not_of(UNICODE_SPACE);
            if (strEnd != std::string::npos)
            {
                const auto trimmed = s.substr(0, strEnd + 1);
                return winrt::hstring{ trimmed };
            }
            return {};
        };

        const auto currentCommand = _terminal->CurrentCommand();
        const auto trimmedCurrentCommand = trimToHstring(currentCommand);

        for (const auto& commandInBuffer : bufferCommands)
        {
            if (const auto hstr{ trimToHstring(commandInBuffer) };
                (!hstr.empty() && hstr != trimmedCurrentCommand))
            {
                commands.push_back(hstr);
            }
        }

        // If the very last thing in the list of recent commands, is exactly the
        // same as the current command, then let's not include it in the
        // history. It's literally the thing the user has typed, RIGHT now.
        // (also account for the fact that the cursor may be in the middle of a commandline)
        if (!commands.empty() &&
            !trimmedCurrentCommand.empty() &&
            std::wstring_view{ commands.back() }.substr(0, trimmedCurrentCommand.size()) == trimmedCurrentCommand)
        {
            commands.pop_back();
        }

        auto context = winrt::make_self<CommandHistoryContext>(std::move(commands));
        context->CurrentCommandline(trimmedCurrentCommand);
        context->QuickFixes(_cachedQuickFixes);
        return *context;
    }

    winrt::hstring ControlCore::CurrentWorkingDirectory() const
    {
        return winrt::hstring{ _terminal->GetWorkingDirectory() };
    }

    bool ControlCore::QuickFixesAvailable() const noexcept
    {
        return _cachedQuickFixes && _cachedQuickFixes.Size() > 0;
    }

    void ControlCore::UpdateQuickFixes(const Windows::Foundation::Collections::IVector<hstring>& quickFixes)
    {
        _cachedQuickFixes = quickFixes;
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
        _renderer->TriggerRedrawAll(true);
    }

    bool ControlCore::HasUnfocusedAppearance() const
    {
        return _settings->HasUnfocusedAppearance();
    }

    void ControlCore::AdjustOpacity(const float opacityAdjust, const bool relative)
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
        const auto shared = _shared.lock_shared();
        if (shared->focusChanged)
        {
            (*shared->focusChanged)(true);
        }
    }

    // See GotFocus.
    void ControlCore::LostFocus()
    {
        const auto shared = _shared.lock_shared();
        if (shared->focusChanged)
        {
            (*shared->focusChanged)(false);
        }
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
            _sendInputToConnection(*out);
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

    // This one is fairly hot! it gets called every time we redraw the scrollbar
    // marks, which is frequently. Fortunately, we don't need to bother with
    // collecting up the actual extents of the marks in here - we just need the
    // rows they start on.
    Windows::Foundation::Collections::IVector<Control::ScrollMark> ControlCore::ScrollMarks() const
    {
        const auto lock = _terminal->LockForReading();
        const auto& markRows = _terminal->GetMarkRows();
        std::vector<Control::ScrollMark> v;

        v.reserve(markRows.size());

        for (const auto& mark : markRows)
        {
            v.emplace_back(
                mark.row,
                OptionalFromColor(_terminal->GetColorForMark(mark.data)));
        }

        return winrt::single_threaded_vector(std::move(v));
    }

    void ControlCore::AddMark(const Control::ScrollMark& mark)
    {
        const auto lock = _terminal->LockForReading();
        ::ScrollbarData m{};

        if (mark.Color.HasValue)
        {
            m.color = til::color{ mark.Color.Color };
        }
        const auto row = (_terminal->IsSelectionActive()) ?
                             _terminal->GetSelectionAnchor().y :
                             _terminal->GetTextBuffer().GetCursor().GetPosition().y;

        _terminal->AddMarkFromUI(m, row);
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
        const auto& marks{ _terminal->GetMarkExtents() };

        std::optional<::MarkExtents> tgt;

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

    safe_void_coroutine ControlCore::_terminalCompletionsChanged(std::wstring_view menuJson,
                                                                 unsigned int replaceLength)
    {
        auto args = winrt::make_self<CompletionsChangedEventArgs>(winrt::hstring{ menuJson },
                                                                  replaceLength);

        co_await winrt::resume_background();

        CompletionsChanged.raise(*this, *args);
    }

    // Select the region of text between [s.start, s.end), in buffer space
    void ControlCore::_selectSpan(til::point_span s)
    {
        // s.end is an _exclusive_ point. We need an inclusive one.
        const auto bufferSize{ _terminal->GetTextBuffer().GetSize() };
        til::point inclusiveEnd = s.end;
        bufferSize.DecrementInBounds(inclusiveEnd);

        _terminal->SelectNewRegion(s.start, inclusiveEnd);
    }

    void ControlCore::SelectCommand(const bool goUp)
    {
        const auto lock = _terminal->LockForWriting();

        const til::point start = _terminal->IsSelectionActive() ? (goUp ? _terminal->GetSelectionAnchor() : _terminal->GetSelectionEnd()) :
                                                                  _terminal->GetTextBuffer().GetCursor().GetPosition();

        std::optional<::MarkExtents> nearest{ std::nullopt };
        const auto& marks{ _terminal->GetMarkExtents() };

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

        std::optional<::MarkExtents> nearest{ std::nullopt };
        const auto& marks{ _terminal->GetMarkExtents() };

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
        bool (*filter)(const ::MarkExtents&),
        til::point_span (*getSpan)(const ::MarkExtents&))
    {
        const auto lock = _terminal->LockForWriting();

        // Do nothing if the caller didn't give us a way to get the span to select for this mark.
        if (!getSpan)
        {
            return;
        }
        const auto& marks{ _terminal->GetMarkExtents() };

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
            [](const ::MarkExtents& m) -> bool { return !m.HasCommand(); },
            [](const ::MarkExtents& m) { return til::point_span{ m.end, *m.commandEnd }; });
    }
    void ControlCore::ContextMenuSelectOutput()
    {
        _contextMenuSelectMark(
            _contextMenuBufferPosition,
            [](const ::MarkExtents& m) -> bool { return !m.HasOutput(); },
            [](const ::MarkExtents& m) { return til::point_span{ *m.commandEnd, *m.outputEnd }; });
    }

    bool ControlCore::_clickedOnMark(
        const til::point& pos,
        bool (*filter)(const ::MarkExtents&))
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
        const auto& marks{ _terminal->GetMarkExtents() };
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
                              [](const ::MarkExtents& m) -> bool { return !m.HasCommand(); });
    }

    // Method Description:
    // * Same as ShouldShowSelectCommand, but with the mark needing output
    bool ControlCore::ShouldShowSelectOutput()
    {
        // Relies on the anchor set in AnchorContextMenu
        return _clickedOnMark(_contextMenuBufferPosition,
                              [](const ::MarkExtents& m) -> bool { return !m.HasOutput(); });
    }

    void ControlCore::PreviewInput(std::wstring_view input)
    {
        _terminal->PreviewText(input);
    }
}
