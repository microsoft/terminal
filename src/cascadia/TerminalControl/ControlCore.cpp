// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ControlCore.h"
#include <argb.h>
#include <DefaultSettings.h>
#include <unicode.hpp>
#include <Utf16Parser.hpp>
#include <Utils.h>
#include <WinUser.h>
#include <LibraryResources.h>
#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/inc/Utils.hpp"
#include "../../buffer/out/search.h"

#include "ControlCore.g.cpp"

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Console::VirtualTerminal;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::Microsoft::Terminal::Control::implementation
{
    // Helper static function to ensure that all ambiguous-width glyphs are reported as narrow.
    // See microsoft/terminal#2066 for more info.
    static bool _IsGlyphWideForceNarrowFallback(const std::wstring_view /* glyph */)
    {
        return false; // glyph is not wide.
    }

    static bool _EnsureStaticInitialization()
    {
        // use C++11 magic statics to make sure we only do this once.
        static bool initialized = []() {
            // *** THIS IS A SINGLETON ***
            SetGlyphWidthFallback(_IsGlyphWideForceNarrowFallback);

            return true;
        }();
        return initialized;
    }

    ControlCore::ControlCore(IControlSettings settings,
                             TerminalConnection::ITerminalConnection connection) :
        _connection{ connection },
        _settings{ settings },
        _desiredFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8 },
        _actualFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false }
    {
        _EnsureStaticInitialization();

        _terminal = std::make_unique<::Microsoft::Terminal::Core::Terminal>();

        // Subscribe to the connection's disconnected event and call our connection closed handlers.
        _connectionStateChangedRevoker = _connection.StateChanged(winrt::auto_revoke, [this](auto&& /*s*/, auto&& /*v*/) {
            _ConnectionStateChangedHandlers(*this, nullptr);
        });

        // This event is explicitly revoked in the destructor: does not need weak_ref
        _connectionOutputEventToken = _connection.TerminalOutput({ this, &ControlCore::_connectionOutputHandler });

        _terminal->SetWriteInputCallback([this](std::wstring& wstr) {
            _sendInputToConnection(wstr);
        });

        // GH#8969: pre-seed working directory to prevent potential races
        _terminal->SetWorkingDirectory(_settings.StartingDirectory());

        auto pfnCopyToClipboard = std::bind(&ControlCore::_terminalCopyToClipboard, this, std::placeholders::_1);
        _terminal->SetCopyToClipboardCallback(pfnCopyToClipboard);

        auto pfnWarningBell = std::bind(&ControlCore::_terminalWarningBell, this);
        _terminal->SetWarningBellCallback(pfnWarningBell);

        auto pfnTitleChanged = std::bind(&ControlCore::_terminalTitleChanged, this, std::placeholders::_1);
        _terminal->SetTitleChangedCallback(pfnTitleChanged);

        auto pfnTabColorChanged = std::bind(&ControlCore::_terminalTabColorChanged, this, std::placeholders::_1);
        _terminal->SetTabColorChangedCallback(pfnTabColorChanged);

        auto pfnBackgroundColorChanged = std::bind(&ControlCore::_terminalBackgroundColorChanged, this, std::placeholders::_1);
        _terminal->SetBackgroundCallback(pfnBackgroundColorChanged);

        auto pfnScrollPositionChanged = std::bind(&ControlCore::_terminalScrollPositionChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        _terminal->SetScrollPositionChangedCallback(pfnScrollPositionChanged);

        auto pfnTerminalCursorPositionChanged = std::bind(&ControlCore::_terminalCursorPositionChanged, this);
        _terminal->SetCursorPositionChangedCallback(pfnTerminalCursorPositionChanged);

        auto pfnTerminalTaskbarProgressChanged = std::bind(&ControlCore::_terminalTaskbarProgressChanged, this);
        _terminal->TaskbarProgressChangedCallback(pfnTerminalTaskbarProgressChanged);

        UpdateSettings(settings);
    }

    ControlCore::~ControlCore()
    {
        Close();

        if (_renderer)
        {
            _renderer->TriggerTeardown();
        }
    }

    bool ControlCore::Initialize(const double actualWidth,
                                 const double actualHeight,
                                 const double compositionScale)
    {
        _panelWidth = actualWidth;
        _panelHeight = actualHeight;
        _compositionScale = compositionScale;

        { // scope for terminalLock
            auto terminalLock = _terminal->LockForWriting();

            if (_initializedTerminal)
            {
                return false;
            }

            const auto windowWidth = actualWidth * compositionScale;
            const auto windowHeight = actualHeight * compositionScale;

            if (windowWidth == 0 || windowHeight == 0)
            {
                return false;
            }

            // First create the render thread.
            // Then stash a local pointer to the render thread so we can initialize it and enable it
            // to paint itself *after* we hand off its ownership to the renderer.
            // We split up construction and initialization of the render thread object this way
            // because the renderer and render thread have circular references to each other.
            auto renderThread = std::make_unique<::Microsoft::Console::Render::RenderThread>();
            auto* const localPointerToThread = renderThread.get();

            // Now create the renderer and initialize the render thread.
            _renderer = std::make_unique<::Microsoft::Console::Render::Renderer>(_terminal.get(), nullptr, 0, std::move(renderThread));
            ::Microsoft::Console::Render::IRenderTarget& renderTarget = *_renderer;

            _renderer->SetRendererEnteredErrorStateCallback([weakThis = get_weak()]() {
                if (auto strongThis{ weakThis.get() })
                {
                    strongThis->_RendererEnteredErrorStateHandlers(*strongThis, nullptr);
                }
            });

            THROW_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

            // Set up the DX Engine
            auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
            _renderer->AddRenderEngine(dxEngine.get());

            // Initialize our font with the renderer
            // We don't have to care about DPI. We'll get a change message immediately if it's not 96
            // and react accordingly.
            _updateFont(true);

            const COORD windowSize{ static_cast<short>(windowWidth),
                                    static_cast<short>(windowHeight) };

            // First set up the dx engine with the window size in pixels.
            // Then, using the font, get the number of characters that can fit.
            // Resize our terminal connection to match that size, and initialize the terminal with that size.
            const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
            LOG_IF_FAILED(dxEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

            // Update DxEngine's SelectionBackground
            dxEngine->SetSelectionBackground(til::color{ _settings.SelectionBackground() });

            const auto vp = dxEngine->GetViewportInCharacters(viewInPixels);
            const auto width = vp.Width();
            const auto height = vp.Height();
            _connection.Resize(height, width);

            // Override the default width and height to match the size of the swapChainPanel
            _settings.InitialCols(width);
            _settings.InitialRows(height);

            _terminal->CreateFromSettings(_settings, renderTarget);

            // IMPORTANT! Set this callback up sooner than later. If we do it
            // after Enable, then it'll be possible to paint the frame once
            // _before_ the warning handler is set up, and then warnings from
            // the first paint will be ignored!
            dxEngine->SetWarningCallback(std::bind(&ControlCore::_rendererWarning, this, std::placeholders::_1));

            // Tell the DX Engine to notify us when the swap chain changes.
            // We do this after we initially set the swapchain so as to avoid unnecessary callbacks (and locking problems)
            dxEngine->SetCallback(std::bind(&ControlCore::_renderEngineSwapChainChanged, this));

            dxEngine->SetRetroTerminalEffect(_settings.RetroTerminalEffect());
            dxEngine->SetPixelShaderPath(_settings.PixelShaderPath());
            dxEngine->SetForceFullRepaintRendering(_settings.ForceFullRepaintRendering());
            dxEngine->SetSoftwareRendering(_settings.SoftwareRendering());

            _updateAntiAliasingMode(dxEngine.get());

            // GH#5098: Inform the engine of the opacity of the default text background.
            if (_settings.UseAcrylic())
            {
                dxEngine->SetDefaultTextBackgroundOpacity(::base::saturated_cast<float>(_settings.TintOpacity()));
            }

            THROW_IF_FAILED(dxEngine->Enable());
            _renderEngine = std::move(dxEngine);

            _initializedTerminal = true;
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
        if (_initializedTerminal)
        {
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
        return _terminal->SendCharEvent(ch, scanCode, modifiers);
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
        // When there is a selection active, escape should clear it and NOT flow through
        // to the terminal. With any other keypress, it should clear the selection AND
        // flow through to the terminal.
        // GH#6423 - don't dismiss selection if the key that was pressed was a
        // modifier key. We'll wait for a real keystroke to dismiss the
        // GH #7395 - don't dismiss selection when taking PrintScreen
        // selection.
        // GH#8522, GH#3758 - Only dismiss the selection on key _down_. If we
        // dismiss on key up, then there's chance that we'll immediately dismiss
        // a selection created by an action bound to a keydown.
        if (HasSelection() &&
            !KeyEvent::IsModifierKey(vkey) &&
            vkey != VK_SNAPSHOT &&
            keyDown)
        {
            // GH#8791 - don't dismiss selection if Windows key was also pressed as a key-combination.
            if (!modifiers.IsWinPressed())
            {
                _terminal->ClearSelection();
                _renderer->TriggerSelection();
            }

            if (vkey == VK_ESCAPE)
            {
                return true;
            }
        }

        // If the terminal translated the key, mark the event as handled.
        // This will prevent the system from trying to get the character out
        // of it and sending us a CharacterReceived event.
        return vkey ? _terminal->SendKeyEvent(vkey,
                                              scanCode,
                                              modifiers,
                                              keyDown) :
                      true;
    }

    bool ControlCore::SendMouseEvent(const til::point viewportPos,
                                     const unsigned int uiButton,
                                     const ControlKeyStates states,
                                     const short wheelDelta,
                                     const TerminalInput::MouseButtonState state)
    {
        return _terminal->SendMouseEvent(viewportPos, uiButton, states, wheelDelta, state);
    }

    void ControlCore::UserScrollViewport(const int viewTop)
    {
        // Clear the regex pattern tree so the renderer does not try to render them while scrolling
        _terminal->ClearPatternTree();

        // This is a scroll event that wasn't initiated by the terminal
        //      itself - it was initiated by the mouse wheel, or the scrollbar.
        _terminal->UserScrollViewport(viewTop);
    }

    void ControlCore::AdjustOpacity(const double adjustment)
    {
        if (adjustment == 0)
        {
            return;
        }

        auto newOpacity = std::clamp(_settings.TintOpacity() + adjustment,
                                     0.0,
                                     1.0);
        if (_settings.UseAcrylic())
        {
            try
            {
                _settings.TintOpacity(newOpacity);

                if (newOpacity >= 1.0)
                {
                    _settings.UseAcrylic(false);
                }
                else
                {
                    // GH#5098: Inform the engine of the new opacity of the default text background.
                    SetBackgroundOpacity(::base::saturated_cast<float>(newOpacity));
                }

                auto eventArgs = winrt::make_self<TransparencyChangedEventArgs>(newOpacity);
                _TransparencyChangedHandlers(*this, *eventArgs);
            }
            CATCH_LOG();
        }
        else if (adjustment < 0)
        {
            _settings.UseAcrylic(true);

            //Setting initial opacity set to 1 to ensure smooth transition to acrylic during mouse scroll
            newOpacity = std::clamp(1.0 + adjustment, 0.0, 1.0);
            _settings.TintOpacity(newOpacity);

            auto eventArgs = winrt::make_self<TransparencyChangedEventArgs>(newOpacity);
            _TransparencyChangedHandlers(*this, *eventArgs);
        }
    }

    void ControlCore::ToggleShaderEffects()
    {
        auto lock = _terminal->LockForWriting();
        // Originally, this action could be used to enable the retro effects
        // even when they're set to `false` in the settings. If the user didn't
        // specify a custom pixel shader, manually enable the legacy retro
        // effect first. This will ensure that a toggle off->on will still work,
        // even if they currently have retro effect off.
        if (_settings.PixelShaderPath().empty() && !_renderEngine->GetRetroTerminalEffect())
        {
            // SetRetroTerminalEffect to true will enable the effect. In this
            // case, the shader effect will already be disabled (because neither
            // a pixel shader nor the retro effects were originally requested).
            // So we _don't_ want to toggle it again below, because that would
            // toggle it back off.
            _renderEngine->SetRetroTerminalEffect(true);
        }
        else
        {
            _renderEngine->ToggleShaderEffects();
        }
    }

    // Method Description:
    // - Tell TerminalCore to update its knowledge about the locations of visible regex patterns
    // - We should call this (through the throttled function) when something causes the visible
    //   region to change, such as when new text enters the buffer or the viewport is scrolled
    void ControlCore::UpdatePatternLocations()
    {
        auto lock = _terminal->LockForWriting();
        _terminal->UpdatePatternsUnderLock();
    }

    // Method description:
    // - Updates last hovered cell, renders / removes rendering of hyper-link if required
    // Arguments:
    // - terminalPosition: The terminal position of the pointer
    void ControlCore::UpdateHoveredCell(const std::optional<til::point>& terminalPosition)
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
        decltype(_terminal->GetHyperlinkIntervalFromPosition(til::point{})) newInterval{ std::nullopt };
        if (terminalPosition.has_value())
        {
            auto lock = _terminal->LockForReading(); // Lock for the duration of our reads.
            newId = _terminal->GetHyperlinkIdAtPosition(*terminalPosition);
            newInterval = _terminal->GetHyperlinkIntervalFromPosition(*terminalPosition);
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
                auto lock = _terminal->LockForWriting();

                _lastHoveredId = newId;
                _lastHoveredInterval = newInterval;
                _renderEngine->UpdateHyperlinkHoveredId(newId);
                _renderer->UpdateLastHoveredInterval(newInterval);
                _renderer->TriggerRedrawAll();
            }

            _HoveredHyperlinkChangedHandlers(*this, nullptr);
        }
    }

    winrt::hstring ControlCore::GetHyperlink(const til::point pos) const
    {
        // Lock for the duration of our reads.
        auto lock = _terminal->LockForReading();
        return winrt::hstring{ _terminal->GetHyperlinkAtPosition(pos) };
    }

    winrt::hstring ControlCore::GetHoveredUriText() const
    {
        auto lock = _terminal->LockForReading(); // Lock for the duration of our reads.
        if (_lastHoveredCell.has_value())
        {
            return winrt::hstring{ _terminal->GetHyperlinkAtPosition(*_lastHoveredCell) };
        }
        return {};
    }

    std::optional<til::point> ControlCore::GetHoveredCell() const
    {
        return _lastHoveredCell;
    }

    // Method Description:
    // - Updates the settings of the current terminal.
    // - INVARIANT: This method can only be called if the caller DOES NOT HAVE writing lock on the terminal.
    void ControlCore::UpdateSettings(const IControlSettings& settings)
    {
        auto lock = _terminal->LockForWriting();

        _settings = settings;

        // Initialize our font information.
        const auto fontFace = _settings.FontFace();
        const short fontHeight = ::base::saturated_cast<short>(_settings.FontSize());
        const auto fontWeight = _settings.FontWeight();
        // The font width doesn't terribly matter, we'll only be using the
        //      height to look it up
        // The other params here also largely don't matter.
        //      The family is only used to determine if the font is truetype or
        //      not, but DX doesn't use that info at all.
        //      The Codepage is additionally not actually used by the DX engine at all.
        _actualFont = { fontFace, 0, fontWeight.Weight, { 0, fontHeight }, CP_UTF8, false };
        _desiredFont = { _actualFont };

        // Update the terminal core with its new Core settings
        _terminal->UpdateSettings(_settings);

        if (!_initializedTerminal)
        {
            // If we haven't initialized, there's no point in continuing.
            // Initialization will handle the renderer settings.
            return;
        }

        _renderEngine->SetForceFullRepaintRendering(_settings.ForceFullRepaintRendering());
        _renderEngine->SetSoftwareRendering(_settings.SoftwareRendering());
        _updateAntiAliasingMode(_renderEngine.get());

        // Refresh our font with the renderer
        const auto actualFontOldSize = _actualFont.GetSize();
        _updateFont();
        const auto actualFontNewSize = _actualFont.GetSize();
        if (actualFontNewSize != actualFontOldSize)
        {
            _refreshSizeUnderLock();
        }
    }

    // Method Description:
    // - Updates the appearance of the current terminal.
    // - INVARIANT: This method can only be called if the caller DOES NOT HAVE writing lock on the terminal.
    void ControlCore::UpdateAppearance(const IControlAppearance& newAppearance)
    {
        auto lock = _terminal->LockForWriting();

        // Update the terminal core with its new Core settings
        _terminal->UpdateAppearance(newAppearance);

        // Update DxEngine settings under the lock
        if (_renderEngine)
        {
            // Update DxEngine settings under the lock
            _renderEngine->SetSelectionBackground(til::color{ newAppearance.SelectionBackground() });
            _renderEngine->SetRetroTerminalEffect(newAppearance.RetroTerminalEffect());
            _renderEngine->SetPixelShaderPath(newAppearance.PixelShaderPath());
            _renderer->TriggerRedrawAll();
        }
    }

    void ControlCore::_updateAntiAliasingMode(::Microsoft::Console::Render::DxEngine* const dxEngine)
    {
        // Update DxEngine's AntialiasingMode
        switch (_settings.AntialiasingMode())
        {
        case TextAntialiasingMode::Cleartype:
            dxEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
            break;
        case TextAntialiasingMode::Aliased:
            dxEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);
            break;
        case TextAntialiasingMode::Grayscale:
        default:
            dxEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            break;
        }
    }

    // Method Description:
    // - Update the font with the renderer. This will be called either when the
    //      font changes or the DPI changes, as DPI changes will necessitate a
    //      font change. This method will *not* change the buffer/viewport size
    //      to account for the new glyph dimensions. Callers should make sure to
    //      appropriately call _doResizeUnderLock after this method is called.
    // - The write lock should be held when calling this method.
    // Arguments:
    // - initialUpdate: whether this font update should be considered as being
    //   concerned with initialization process. Value forwarded to event handler.
    void ControlCore::_updateFont(const bool initialUpdate)
    {
        const int newDpi = static_cast<int>(static_cast<double>(USER_DEFAULT_SCREEN_DPI) *
                                            _compositionScale);

        // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
        //      actually fail. We need a way to gracefully fallback.
        _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);

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
        _FontSizeChangedHandlers(actualNewSize.X, actualNewSize.Y, initialUpdate);
    }

    // Method Description:
    // - Set the font size of the terminal control.
    // Arguments:
    // - fontSize: The size of the font.
    void ControlCore::_setFontSize(int fontSize)
    {
        try
        {
            // Make sure we have a non-zero font size
            const auto newSize = std::max<short>(gsl::narrow_cast<short>(fontSize), 1);
            const auto fontFace = _settings.FontFace();
            const auto fontWeight = _settings.FontWeight();
            _actualFont = { fontFace, 0, fontWeight.Weight, { 0, newSize }, CP_UTF8, false };
            _desiredFont = { _actualFont };

            auto lock = _terminal->LockForWriting();

            // Refresh our font with the renderer
            _updateFont();

            // Resize the terminal's BUFFER to match the new font size. This does
            // NOT change the size of the window, because that can lead to more
            // problems (like what happens when you change the font size while the
            // window is maximized?)
            _refreshSizeUnderLock();
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Reset the font size of the terminal to its default size.
    // Arguments:
    // - none
    void ControlCore::ResetFontSize()
    {
        _setFontSize(_settings.FontSize());
    }

    // Method Description:
    // - Adjust the font size of the terminal control.
    // Arguments:
    // - fontSizeDelta: The amount to increase or decrease the font size by.
    void ControlCore::AdjustFontSize(int fontSizeDelta)
    {
        const auto newSize = _desiredFont.GetEngineSize().Y + fontSizeDelta;
        _setFontSize(newSize);
    }

    // Method Description:
    // - Perform a resize for the current size of the swapchainpanel. If the
    //   font size changed, we'll need to resize the buffer to fit the existing
    //   swapchain size. This helper will call _doResizeUnderLock with the
    //   current size of the swapchain, accounting for scaling due to DPI.
    // - Note that a DPI change will also trigger a font size change, and will
    //   call into here.
    // - The write lock should be held when calling this method, we might be
    //   changing the buffer size in _doResizeUnderLock.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void ControlCore::_refreshSizeUnderLock()
    {
        const auto widthInPixels = _panelWidth * _compositionScale;
        const auto heightInPixels = _panelHeight * _compositionScale;

        _doResizeUnderLock(widthInPixels, heightInPixels);
    }

    // Method Description:
    // - Process a resize event that was initiated by the user. This can either
    //   be due to the user resizing the window (causing the swapchain to
    //   resize) or due to the DPI changing (causing us to need to resize the
    //   buffer to match)
    // Arguments:
    // - newWidth: the new width of the swapchain, in pixels.
    // - newHeight: the new height of the swapchain, in pixels.
    void ControlCore::_doResizeUnderLock(const double newWidth,
                                         const double newHeight)
    {
        SIZE size;
        size.cx = static_cast<long>(newWidth);
        size.cy = static_cast<long>(newHeight);

        // Don't actually resize so small that a single character wouldn't fit
        // in either dimension. The buffer really doesn't like being size 0.
        if (size.cx < _actualFont.GetSize().X || size.cy < _actualFont.GetSize().Y)
        {
            return;
        }

        _terminal->ClearSelection();

        // Tell the dx engine that our window is now the new size.
        THROW_IF_FAILED(_renderEngine->SetWindowSize(size));

        // Invalidate everything
        _renderer->TriggerRedrawAll();

        // Convert our new dimensions to characters
        const auto viewInPixels = Viewport::FromDimensions({ 0, 0 },
                                                           { static_cast<short>(size.cx), static_cast<short>(size.cy) });
        const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);

        // If this function succeeds with S_FALSE, then the terminal didn't
        // actually change size. No need to notify the connection of this no-op.
        const HRESULT hr = _terminal->UserResize({ vp.Width(), vp.Height() });
        if (SUCCEEDED(hr) && hr != S_FALSE)
        {
            _connection.Resize(vp.Height(), vp.Width());
        }
    }

    void ControlCore::SizeChanged(const double width,
                                  const double height)
    {
        _panelWidth = width;
        _panelHeight = height;

        auto lock = _terminal->LockForWriting();
        const auto currentEngineScale = _renderEngine->GetScaling();

        auto scaledWidth = width * currentEngineScale;
        auto scaledHeight = height * currentEngineScale;
        _doResizeUnderLock(scaledWidth, scaledHeight);
    }

    void ControlCore::ScaleChanged(const double scale)
    {
        if (!_renderEngine)
        {
            return;
        }

        const auto currentEngineScale = _renderEngine->GetScaling();
        // If we're getting a notification to change to the DPI we already
        // have, then we're probably just beginning the DPI change. Since
        // we'll get _another_ event with the real DPI, do nothing here for
        // now. We'll also skip the next resize in _swapChainSizeChanged.
        const bool dpiWasUnchanged = currentEngineScale == scale;
        if (dpiWasUnchanged)
        {
            return;
        }

        const auto dpi = (float)(scale * USER_DEFAULT_SCREEN_DPI);

        const auto actualFontOldSize = _actualFont.GetSize();

        auto lock = _terminal->LockForWriting();
        _compositionScale = scale;

        _renderer->TriggerFontChange(::base::saturated_cast<int>(dpi),
                                     _desiredFont,
                                     _actualFont);

        const auto actualFontNewSize = _actualFont.GetSize();
        if (actualFontNewSize != actualFontOldSize)
        {
            _refreshSizeUnderLock();
        }
    }

    void ControlCore::SetSelectionAnchor(til::point const& position)
    {
        auto lock = _terminal->LockForWriting();
        _terminal->SetSelectionAnchor(position);
    }

    // Method Description:
    // - Sets selection's end position to match supplied cursor position, e.g. while mouse dragging.
    // Arguments:
    // - position: the point in terminal coordinates (in cells, not pixels)
    void ControlCore::SetEndSelectionPoint(til::point const& position)
    {
        if (!_terminal->IsSelectionActive())
        {
            return;
        }

        // Have to take the lock because the renderer will not draw correctly if
        // you move its endpoints while it is generating a frame.
        auto lock = _terminal->LockForWriting();

        const short lastVisibleRow = std::max<short>(_terminal->GetViewport().Height() - 1, 0);
        const short lastVisibleCol = std::max<short>(_terminal->GetViewport().Width() - 1, 0);

        til::point terminalPosition{
            std::clamp<short>(position.x<short>(), 0, lastVisibleCol),
            std::clamp<short>(position.y<short>(), 0, lastVisibleRow)
        };

        // save location (for rendering) + render
        _terminal->SetSelectionEnd(terminalPosition);
        _renderer->TriggerSelection();
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
    // - CopyOnSelect does NOT clear the selection
    // Arguments:
    // - singleLine: collapse all of the text to one line
    // - formats: which formats to copy (defined by action's CopyFormatting arg). nullptr
    //             if we should defer which formats are copied to the global setting
    bool ControlCore::CopySelectionToClipboard(bool singleLine,
                                               const Windows::Foundation::IReference<CopyFormat>& formats)
    {
        // no selection --> nothing to copy
        if (!_terminal->IsSelectionActive())
        {
            return false;
        }

        // extract text from buffer
        // RetrieveSelectedTextFromBuffer will lock while it's reading
        const auto bufferData = _terminal->RetrieveSelectedTextFromBuffer(singleLine);

        // convert text: vector<string> --> string
        std::wstring textData;
        for (const auto& text : bufferData.text)
        {
            textData += text;
        }

        // convert text to HTML format
        // GH#5347 - Don't provide a title for the generated HTML, as many
        // web applications will paste the title first, followed by the HTML
        // content, which is unexpected.
        const auto htmlData = formats == nullptr || WI_IsFlagSet(formats.Value(), CopyFormat::HTML) ?
                                  TextBuffer::GenHTML(bufferData,
                                                      _actualFont.GetUnscaledSize().Y,
                                                      _actualFont.GetFaceName(),
                                                      til::color{ _settings.DefaultBackground() }) :
                                  "";

        // convert to RTF format
        const auto rtfData = formats == nullptr || WI_IsFlagSet(formats.Value(), CopyFormat::RTF) ?
                                 TextBuffer::GenRTF(bufferData,
                                                    _actualFont.GetUnscaledSize().Y,
                                                    _actualFont.GetFaceName(),
                                                    til::color{ _settings.DefaultBackground() }) :
                                 "";

        if (!_settings.CopyOnSelect())
        {
            _terminal->ClearSelection();
            _renderer->TriggerSelection();
        }

        // send data up for clipboard
        _CopyToClipboardHandlers(*this,
                                 winrt::make<CopyToClipboardEventArgs>(winrt::hstring{ textData },
                                                                       winrt::to_hstring(htmlData),
                                                                       winrt::to_hstring(rtfData),
                                                                       formats));
        return true;
    }

    // Method Description:
    // - Pre-process text pasted (presumably from the clipboard)
    //   before sending it over the terminal's connection.
    void ControlCore::PasteText(const winrt::hstring& hstr)
    {
        _terminal->WritePastedText(hstr);
        _terminal->ClearSelection();
        _terminal->TrySnapOnInput();
    }

    FontInfo ControlCore::GetFont() const
    {
        return _actualFont;
    }

    til::size ControlCore::FontSizeInDips() const
    {
        const til::size fontSize{ GetFont().GetSize() };
        return fontSize.scale(til::math::rounding, 1.0f / ::base::saturated_cast<float>(_compositionScale));
    }

    TerminalConnection::ConnectionState ControlCore::ConnectionState() const
    {
        return _connection.State();
    }

    hstring ControlCore::Title()
    {
        return hstring{ _terminal->GetConsoleTitle() };
    }

    hstring ControlCore::WorkingDirectory() const
    {
        return hstring{ _terminal->GetWorkingDirectory() };
    }

    bool ControlCore::BracketedPasteEnabled() const noexcept
    {
        return _terminal->IsXtermBracketedPasteModeEnabled();
    }

    Windows::Foundation::IReference<winrt::Windows::UI::Color> ControlCore::TabColor() noexcept
    {
        auto coreColor = _terminal->GetTabColor();
        return coreColor.has_value() ? Windows::Foundation::IReference<winrt::Windows::UI::Color>(til::color{ coreColor.value() }) :
                                       nullptr;
    }

    til::color ControlCore::BackgroundColor() const
    {
        return _terminal->GetDefaultBackground();
    }

    // Method Description:
    // - Gets the internal taskbar state value
    // Return Value:
    // - The taskbar state of this control
    const size_t ControlCore::TaskbarState() const noexcept
    {
        return _terminal->GetTaskbarState();
    }

    // Method Description:
    // - Gets the internal taskbar progress value
    // Return Value:
    // - The taskbar progress of this control
    const size_t ControlCore::TaskbarProgress() const noexcept
    {
        return _terminal->GetTaskbarProgress();
    }

    int ControlCore::ScrollOffset()
    {
        return _terminal->GetScrollOffset();
    }

    // Function Description:
    // - Gets the height of the terminal in lines of text. This is just the
    //   height of the viewport.
    // Return Value:
    // - The height of the terminal in lines of text
    int ControlCore::ViewHeight() const
    {
        return _terminal->GetViewport().Height();
    }

    // Function Description:
    // - Gets the height of the terminal in lines of text. This includes the
    //   history AND the viewport.
    // Return Value:
    // - The height of the terminal in lines of text
    int ControlCore::BufferHeight() const
    {
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
    // - Called for the Terminal's TabColorChanged callback. This will re-raise
    //   a new winrt TypedEvent that can be listened to.
    // - The listeners to this event will re-query the control for the current
    //   value of TabColor().
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void ControlCore::_terminalTabColorChanged(const std::optional<til::color> /*color*/)
    {
        // Raise a TabColorChanged event
        _TabColorChangedHandlers(*this, nullptr);
    }

    // Method Description:
    // - Called for the Terminal's BackgroundColorChanged callback. This will
    //   re-raise a new winrt TypedEvent that can be listened to.
    // - The listeners to this event will re-query the control for the current
    //   value of BackgroundColor().
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    void ControlCore::_terminalBackgroundColorChanged(const COLORREF /*color*/)
    {
        // Raise a BackgroundColorChanged event
        _BackgroundColorChangedHandlers(*this, nullptr);
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
        // Clear the regex pattern tree so the renderer does not try to render them while scrolling
        // We're **NOT** taking the lock here unlike _scrollbarChangeHandler because
        // we are already under lock (since this usually happens as a result of writing).
        // TODO GH#9617: refine locking around pattern tree
        _terminal->ClearPatternTree();

        _ScrollPositionChangedHandlers(*this,
                                       winrt::make<ScrollPositionChangedArgs>(viewTop,
                                                                              viewHeight,
                                                                              bufferSize));
    }

    void ControlCore::_terminalCursorPositionChanged()
    {
        _CursorPositionChangedHandlers(*this, nullptr);
    }

    void ControlCore::_terminalTaskbarProgressChanged()
    {
        _TaskbarProgressChangedHandlers(*this, nullptr);
    }

    bool ControlCore::HasSelection() const
    {
        return _terminal->IsSelectionActive();
    }

    bool ControlCore::CopyOnSelect() const
    {
        return _settings.CopyOnSelect();
    }

    std::vector<std::wstring> ControlCore::SelectedText(bool trimTrailingWhitespace) const
    {
        // RetrieveSelectedTextFromBuffer will lock while it's reading
        return _terminal->RetrieveSelectedTextFromBuffer(trimTrailingWhitespace).text;
    }

    ::Microsoft::Console::Types::IUiaData* ControlCore::GetUiaData() const
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
    void ControlCore::Search(const winrt::hstring& text,
                             const bool goForward,
                             const bool caseSensitive)
    {
        if (text.size() == 0)
        {
            return;
        }

        const Search::Direction direction = goForward ?
                                                Search::Direction::Forward :
                                                Search::Direction::Backward;

        const Search::Sensitivity sensitivity = caseSensitive ?
                                                    Search::Sensitivity::CaseSensitive :
                                                    Search::Sensitivity::CaseInsensitive;

        ::Search search(*GetUiaData(), text.c_str(), direction, sensitivity);
        auto lock = _terminal->LockForWriting();
        if (search.FindNext())
        {
            _terminal->SetBlockSelection(false);
            search.Select();
            _renderer->TriggerSelection();
        }
    }

    void ControlCore::SetBackgroundOpacity(const float opacity)
    {
        if (_renderEngine)
        {
            _renderEngine->SetDefaultTextBackgroundOpacity(::base::saturated_cast<float>(opacity));
        }
    }

    // Method Description:
    // - Asynchronously close our connection. The Connection will likely wait
    //   until the attached process terminates before Close returns. If that's
    //   the case, we don't want to block the UI thread waiting on that process
    //   handle.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    winrt::fire_and_forget ControlCore::_asyncCloseConnection()
    {
        if (auto localConnection{ std::exchange(_connection, nullptr) })
        {
            // Close the connection on the background thread.
            co_await winrt::resume_background(); // ** DO NOT INTERACT WITH THE CONTROL CORE AFTER THIS LINE **

            // Here, the ControlCore very well might be gone.
            // _asyncCloseConnection is called on the dtor, so it's entirely
            // possible that the background thread is resuming after we've been
            // cleaned up.

            localConnection.Close();
            // connection is destroyed.
        }
    }

    void ControlCore::Close()
    {
        if (!_closing.exchange(true))
        {
            // Stop accepting new output and state changes before we disconnect everything.
            _connection.TerminalOutput(_connectionOutputEventToken);
            _connectionStateChangedRevoker.revoke();

            // GH#1996 - Close the connection asynchronously on a background
            // thread.
            // Since TermControl::Close is only ever triggered by the UI, we
            // don't really care to wait for the connection to be completely
            // closed. We can just do it whenever.
            _asyncCloseConnection();
        }
    }

    HANDLE ControlCore::GetSwapChainHandle() const
    {
        // This is called by:
        // * TermControl::RenderEngineSwapChainChanged, who is only registered
        //   after Core::Initialize() is called.
        // * TermControl::_InitializeTerminal, after the call to Initialize, for
        //   _AttachDxgiSwapChainToXaml.
        // In both cases, we'll have a _renderEngine by then.
        return _renderEngine->GetSwapChainHandle();
    }

    void ControlCore::_rendererWarning(const HRESULT hr)
    {
        _RendererWarningHandlers(*this, winrt::make<RendererWarningArgs>(hr));
    }

    void ControlCore::_renderEngineSwapChainChanged()
    {
        _SwapChainChangedHandlers(*this, nullptr);
    }

    void ControlCore::BlinkAttributeTick()
    {
        auto lock = _terminal->LockForWriting();

        auto& renderTarget = *_renderer;
        auto& blinkingState = _terminal->GetBlinkingState();
        blinkingState.ToggleBlinkingRendition(renderTarget);
    }

    void ControlCore::BlinkCursor()
    {
        if (!_terminal->IsCursorBlinkingAllowed() &&
            _terminal->IsCursorVisible())
        {
            return;
        }
        // SetCursorOn will take the write lock for you.
        _terminal->SetCursorOn(!_terminal->IsCursorOn());
    }

    bool ControlCore::CursorOn() const
    {
        return _terminal->IsCursorOn();
    }

    void ControlCore::CursorOn(const bool isCursorOn)
    {
        _terminal->SetCursorOn(isCursorOn);
    }

    void ControlCore::ResumeRendering()
    {
        _renderer->ResetErrorStateAndResume();
    }

    bool ControlCore::IsVtMouseModeEnabled() const
    {
        return _terminal != nullptr && _terminal->IsTrackingMouseInput();
    }

    til::point ControlCore::CursorPosition() const
    {
        // If we haven't been initialized yet, then fake it.
        if (!_initializedTerminal)
        {
            return { 0, 0 };
        }

        auto lock = _terminal->LockForReading();
        return _terminal->GetCursorPosition();
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
        auto lock = _terminal->LockForWriting();
        // handle ALT key
        _terminal->SetBlockSelection(altEnabled);

        ::Terminal::SelectionExpansionMode mode = ::Terminal::SelectionExpansionMode::Cell;
        if (numberOfClicks == 1)
        {
            mode = ::Terminal::SelectionExpansionMode::Cell;
        }
        else if (numberOfClicks == 2)
        {
            mode = ::Terminal::SelectionExpansionMode::Word;
        }
        else if (numberOfClicks == 3)
        {
            mode = ::Terminal::SelectionExpansionMode::Line;
        }

        // Update the selection appropriately

        // We reset the active selection if one of the conditions apply:
        // - shift is not held
        // - GH#9384: the position is the same as of the first click starting
        //   the selection (we need to reset selection on double-click or
        //   triple-click, so it captures the word or the line, rather than
        //   extending the selection)
        if (HasSelection() && (!shiftEnabled || isOnOriginalPosition))
        {
            // Reset the selection
            _terminal->ClearSelection();
            selectionNeedsToBeCopied = false; // there's no selection, so there's nothing to update
        }

        if (shiftEnabled && HasSelection())
        {
            // If shift is pressed and there is a selection we extend it using
            // the selection mode (expand the "end" selection point)
            _terminal->SetSelectionEnd(terminalPosition, mode);
            selectionNeedsToBeCopied = true;
        }
        else if (mode != ::Terminal::SelectionExpansionMode::Cell || shiftEnabled)
        {
            // If we are handling a double / triple-click or shift+single click
            // we establish selection using the selected mode
            // (expand both "start" and "end" selection points)
            _terminal->MultiClickSelection(terminalPosition, mode);
            selectionNeedsToBeCopied = true;
        }

        _renderer->TriggerSelection();
    }

    void ControlCore::AttachUiaEngine(::Microsoft::Console::Render::IRenderEngine* const pEngine)
    {
        if (_renderer)
        {
            _renderer->AddRenderEngine(pEngine);
        }
    }

    bool ControlCore::IsInReadOnlyMode() const
    {
        return _isReadOnly;
    }

    void ControlCore::ToggleReadOnlyMode()
    {
        _isReadOnly = !_isReadOnly;
    }

    void ControlCore::_raiseReadOnlyWarning()
    {
        auto noticeArgs = winrt::make<NoticeEventArgs>(NoticeLevel::Info, RS_(L"TermControlReadOnly"));
        _RaiseNoticeHandlers(*this, std::move(noticeArgs));
    }
    void ControlCore::_connectionOutputHandler(const hstring& hstr)
    {
        _terminal->Write(hstr);

        // NOTE: We're raising an event here to inform the TermControl that
        // output has been received, so it can queue up a throttled
        // UpdatePatternLocations call. In the future, we should have the
        // _updatePatternLocations ThrottledFunc internal to this class, and
        // run on this object's dispatcher queue.
        //
        // We're not doing that quite yet, because the Core will eventually
        // be out-of-proc from the UI thread, and won't be able to just use
        // the UI thread as the dispatcher queue thread.
        //
        // See TODO: https://github.com/microsoft/terminal/projects/5#card-50760282
        _ReceivedOutputHandlers(*this, nullptr);
    }

}
