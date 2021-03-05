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

#include "ControlCore.g.cpp"
// #include "TermControlAutomationPeer.h" // ?

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Console::VirtualTerminal;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
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

        // This event is explicitly revoked in the destructor: does not need weak_ref
        auto onReceiveOutputFn = [this](const hstring str) {
            _terminal->Write(str);

            // !TODO!: _updatePatternLocations should happen in the Core, but it's
            // a ThrottledFunc, which needs a CoreDispatcher. How do we plan on
            // dealing with that?
            // _updatePatternLocations->Run();
        };
        _connectionOutputEventToken = _connection.TerminalOutput(onReceiveOutputFn);

        _terminal->SetWriteInputCallback([this](std::wstring& wstr) {
            _SendInputToConnection(wstr);
        });

        _terminal->UpdateSettings(settings);
    }

    bool ControlCore::InitializeTerminal(const double actualWidth,
                                         const double actualHeight,
                                         const double compositionScaleX,
                                         const double compositionScaleY)
    {
        _compositionScaleX = compositionScaleX;
        _compositionScaleY = compositionScaleY;

        { // scope for terminalLock
            auto terminalLock = _terminal->LockForWriting();

            if (_initializedTerminal)
            {
                return false;
            }

            const auto windowWidth = actualWidth * compositionScaleX; // Width() and Height() are NaN?
            const auto windowHeight = actualHeight * compositionScaleY;

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

            // !TODO!: We _DO_ want this
            // _renderer->SetRendererEnteredErrorStateCallback([weakThis = get_weak()]() {
            //     if (auto strongThis{ weakThis.get() })
            //     {
            //         strongThis->_RendererEnteredErrorState();
            //     }
            // });

            THROW_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

            // Set up the DX Engine
            auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
            _renderer->AddRenderEngine(dxEngine.get());

            // Initialize our font with the renderer
            // We don't have to care about DPI. We'll get a change message immediately if it's not 96
            // and react accordingly.
            _UpdateFont(true);

            const COORD windowSize{ static_cast<short>(windowWidth),
                                    static_cast<short>(windowHeight) };

            // Fist set up the dx engine with the window size in pixels.
            // Then, using the font, get the number of characters that can fit.
            // Resize our terminal connection to match that size, and initialize the terminal with that size.
            const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
            LOG_IF_FAILED(dxEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

            // Update DxEngine's SelectionBackground
            dxEngine->SetSelectionBackground(_settings.SelectionBackground());

            const auto vp = dxEngine->GetViewportInCharacters(viewInPixels);
            const auto width = vp.Width();
            const auto height = vp.Height();
            _connection.Resize(height, width);

            // Override the default width and height to match the size of the swapChainPanel
            _settings.InitialCols(width);
            _settings.InitialRows(height);

            _terminal->CreateFromSettings(_settings, renderTarget);

            // !TODO!: We _DO_ want this
            // // IMPORTANT! Set this callback up sooner than later. If we do it
            // // after Enable, then it'll be possible to paint the frame once
            // // _before_ the warning handler is set up, and then warnings from
            // // the first paint will be ignored!
            // dxEngine->SetWarningCallback(std::bind(&TermControlTwo::_RendererWarning, this, std::placeholders::_1));

            // !TODO!: We _DO_ want this
            // // Tell the DX Engine to notify us when the swap chain changes.
            // // We do this after we initially set the swapchain so as to avoid unnecessary callbacks (and locking problems)
            // _renderEngine->SetCallback(std::bind(&TermControlTwo::RenderEngineSwapChainChanged, this));

            dxEngine->SetRetroTerminalEffect(_settings.RetroTerminalEffect());
            dxEngine->SetPixelShaderPath(_settings.PixelShaderPath());
            dxEngine->SetForceFullRepaintRendering(_settings.ForceFullRepaintRendering());
            dxEngine->SetSoftwareRendering(_settings.SoftwareRendering());

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

            // GH#5098: Inform the engine of the opacity of the default text background.
            if (_settings.UseAcrylic())
            {
                dxEngine->SetDefaultTextBackgroundOpacity(::base::saturated_cast<float>(_settings.TintOpacity()));
            }

            THROW_IF_FAILED(dxEngine->Enable());
            _renderEngine = std::move(dxEngine);

            // !TODO! in the past we did SetSwapChainHandle _before_ calling
            // EnablePainting. Mild worry that doing EnablePainting first will
            // break
            localPointerToThread->EnablePainting();

            _initializedTerminal = true;
        } // scope for TerminalLock

        // Start the connection outside of lock, because it could
        // start writing output immediately.
        _connection.Start();

        // !TODO!: Do we want this?
        // Likewise, run the event handlers outside of lock (they could
        // be reentrant)
        // _InitializedHandlers(*this, nullptr);
        return true;
    }

    // Method Description:
    // - Update the font with the renderer. This will be called either when the
    //      font changes or the DPI changes, as DPI changes will necessitate a
    //      font change. This method will *not* change the buffer/viewport size
    //      to account for the new glyph dimensions. Callers should make sure to
    //      appropriately call _DoResizeUnderLock after this method is called.
    // - The write lock should be held when calling this method.
    // Arguments:
    // - initialUpdate: whether this font update should be considered as being
    //   concerned with initialization process. Value forwarded to event handler.
    void ControlCore::_UpdateFont(const bool /*initialUpdate*/)
    {
        const int newDpi = static_cast<int>(static_cast<double>(USER_DEFAULT_SCREEN_DPI) *
                                            _compositionScaleX);

        // !TODO!: MSFT:20895307 If the font doesn't exist, this doesn't
        //      actually fail. We need a way to gracefully fallback.
        _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);

        // If the actual font isn't what was requested...
        if (_actualFont.GetFaceName() != _desiredFont.GetFaceName())
        {
            // !TODO!: We _DO_ want this
            // // Then warn the user that we picked something because we couldn't find their font.
            // // Format message with user's choice of font and the font that was chosen instead.
            // const winrt::hstring message{ fmt::format(std::wstring_view{ RS_(L"NoticeFontNotFound") }, _desiredFont.GetFaceName(), _actualFont.GetFaceName()) };
            // // Capture what we need to resume later.
            // [strongThis = get_strong(), message]() -> winrt::fire_and_forget {
            //     // Take these out of the lambda and store them locally
            //     // because the coroutine will lose them into space
            //     // by the time it resumes.
            //     const auto msg = message;
            //     const auto strong = strongThis;

            //     // Pop the rest of this function to the tail of the UI thread
            //     // Just in case someone was holding a lock when they called us and
            //     // the handlers decide to do something that take another lock
            //     // (like ShellExecute pumping our messaging thread...GH#7994)
            //     co_await strong->Dispatcher();

            //     auto noticeArgs = winrt::make<NoticeEventArgs>(NoticeLevel::Warning, std::move(msg));
            //     strong->_raiseNoticeHandlers(*strong, std::move(noticeArgs));
            // }();
        }

        // !TODO!: We _DO_ want this
        // const auto actualNewSize = _actualFont.GetSize();
        // _fontSizeChangedHandlers(actualNewSize.X, actualNewSize.Y, initialUpdate);
    }

    // Method Description:
    // - Writes the given sequence as input to the active terminal connection.
    // - This method has been overloaded to allow zero-copy winrt::param::hstring optimizations.
    // Arguments:
    // - wstr: the string of characters to write to the terminal connection.
    // Return Value:
    // - <none>
    void ControlCore::_SendInputToConnection(const winrt::hstring& wstr)
    {
        if (_isReadOnly)
        {
            // !TODO!: Do we want this?
            // _RaiseReadOnlyWarning();
        }
        else
        {
            _connection.WriteInput(wstr);
        }
    }

    void ControlCore::_SendInputToConnection(std::wstring_view wstr)
    {
        if (_isReadOnly)
        {
            // !TODO!: Do we want this?
            // _RaiseReadOnlyWarning();
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
        _SendInputToConnection(wstr);
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

    // Method description:
    // - Updates last hovered cell, renders / removes rendering of hyper-link if required
    // Arguments:
    // - terminalPosition: The terminal position of the pointer
    void ControlCore::_UpdateHoveredCell(const std::optional<COORD>& terminalPosition)
    {
        if (terminalPosition == _lastHoveredCell)
        {
            return;
        }

        _lastHoveredCell = terminalPosition;

        // if (terminalPosition.has_value())
        // {
        //     const auto uri = _core->_terminal->GetHyperlinkAtPosition(*terminalPosition);
        //     if (!uri.empty())
        //     {
        //         // Update the tooltip with the URI
        //         HoveredUri().Text(uri);

        //         // Set the border thickness so it covers the entire cell
        //         const auto charSizeInPixels = CharacterDimensions();
        //         const auto htInDips = charSizeInPixels.Height / _compositionScaleY;
        //         const auto wtInDips = charSizeInPixels.Width / _compositionScaleX;
        //         const Thickness newThickness{ wtInDips, htInDips, 0, 0 };
        //         HyperlinkTooltipBorder().BorderThickness(newThickness);

        //         // Compute the location of the top left corner of the cell in DIPS
        //         const til::size marginsInDips{ til::math::rounding, GetPadding().Left, GetPadding().Top };
        //         const til::point startPos{ terminalPosition->X, terminalPosition->Y };
        //         const til::size fontSize{ _core->_actualFont.GetSize() };
        //         const til::point posInPixels{ startPos * fontSize };
        //         const til::point posInDIPs{ posInPixels / SwapChainPanel().CompositionScaleX() };
        //         const til::point locationInDIPs{ posInDIPs + marginsInDips };

        //         // Move the border to the top left corner of the cell
        //         OverlayCanvas().SetLeft(HyperlinkTooltipBorder(), (locationInDIPs.x() - SwapChainPanel().ActualOffset().x));
        //         OverlayCanvas().SetTop(HyperlinkTooltipBorder(), (locationInDIPs.y() - SwapChainPanel().ActualOffset().y));
        //     }
        // }

        const uint16_t newId = terminalPosition.has_value() ? _terminal->GetHyperlinkIdAtPosition(*terminalPosition) :
                                                              0u;
        const auto newInterval = terminalPosition.has_value() ? _terminal->GetHyperlinkIntervalFromPosition(*terminalPosition) :
                                                                std::nullopt;

        // If the hyperlink ID changed or the interval changed, trigger a redraw all
        // (so this will happen both when we move onto a link and when we move off a link)
        if (newId != _lastHoveredId ||
            (newInterval != _lastHoveredInterval))
        {
            _lastHoveredId = newId;
            _lastHoveredInterval = newInterval;
            _renderEngine->UpdateHyperlinkHoveredId(newId);
            _renderer->UpdateLastHoveredInterval(newInterval);
            _renderer->TriggerRedrawAll();

            _raiseHoveredHyperlinkChanged();
        }
    }

    void ControlCore::_raiseHoveredHyperlinkChanged()
    {
        _HoveredHyperlinkChangedHandlers(*this, nullptr);
    }

    winrt::hstring ControlCore::GetHoveredUriText()
    {
        if (_lastHoveredCell.has_value())
        {
            const winrt::hstring uri{ _terminal->GetHyperlinkAtPosition(*_lastHoveredCell) };
            return uri;
        }
        return { L"" };
    }

}
