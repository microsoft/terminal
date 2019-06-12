// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TermControl.h"
#include <argb.h>
#include <DefaultSettings.h>
#include <unicode.hpp>
#include <Utf16Parser.hpp>
#include <WinUser.h>
#include "..\..\types\inc\GlyphWidth.hpp"

#include "TermControl.g.cpp"

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal::Settings;

namespace winrt::Microsoft::Terminal::TerminalControl::implementation
{
    TermControl::TermControl() :
        TermControl(Settings::TerminalSettings{})
    {
    }

    TermControl::TermControl(Settings::IControlSettings settings) :
        _connection{ TerminalConnection::ConhostConnection(winrt::to_hstring("cmd.exe"), winrt::hstring(), 30, 80, winrt::guid()) },
        _initializedTerminal{ false },
        _root{ nullptr },
        _controlRoot{ nullptr },
        _swapChainPanel{ nullptr },
        _settings{ settings },
        _closing{ false },
        _lastScrollOffset{ std::nullopt },
        _desiredFont{ DEFAULT_FONT_FACE.c_str(), 0, 10, { 0, DEFAULT_FONT_SIZE }, CP_UTF8 },
        _actualFont{ DEFAULT_FONT_FACE.c_str(), 0, 10, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false },
        _touchAnchor{ std::nullopt },
        _leadingSurrogate{},
        _cursorTimer{}
    {
        _Create();
    }

    void TermControl::_Create()
    {
        // Create a dummy UserControl to use as the "root" of our control we'll
        //      build manually.
        Controls::UserControl myControl;
        _controlRoot = myControl;

        Controls::Grid container;

        Controls::ColumnDefinition contentColumn{};
        Controls::ColumnDefinition scrollbarColumn{};
        contentColumn.Width(GridLength{ 1.0, GridUnitType::Star });
        scrollbarColumn.Width(GridLength{ 1.0, GridUnitType::Auto });

        container.ColumnDefinitions().Append(contentColumn);
        container.ColumnDefinitions().Append(scrollbarColumn);

        _scrollBar = Controls::Primitives::ScrollBar{};
        _scrollBar.Orientation(Controls::Orientation::Vertical);
        _scrollBar.IndicatorMode(Controls::Primitives::ScrollingIndicatorMode::MouseIndicator);
        _scrollBar.HorizontalAlignment(HorizontalAlignment::Right);
        _scrollBar.VerticalAlignment(VerticalAlignment::Stretch);

        // Initialize the scrollbar with some placeholder values.
        // The scrollbar will be updated with real values on _Initialize
        _scrollBar.Maximum(1);
        _scrollBar.ViewportSize(10);
        _scrollBar.IsTabStop(false);
        _scrollBar.SmallChange(1);
        _scrollBar.LargeChange(4);
        _scrollBar.Visibility(Visibility::Visible);

        // Create the SwapChainPanel that will display our content
        Controls::SwapChainPanel swapChainPanel;

        _sizeChangedRevoker = swapChainPanel.SizeChanged(winrt::auto_revoke, { this, &TermControl::_SwapChainSizeChanged });
        _compositionScaleChangedRevoker = swapChainPanel.CompositionScaleChanged(winrt::auto_revoke, { this, &TermControl::_SwapChainScaleChanged });

        // Initialize the terminal only once the swapchainpanel is loaded - that
        //      way, we'll be able to query the real pixel size it got on layout
        _loadedRevoker = swapChainPanel.Loaded(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            _InitializeTerminal();
        });

        container.Children().Append(swapChainPanel);
        container.Children().Append(_scrollBar);
        Controls::Grid::SetColumn(swapChainPanel, 0);
        Controls::Grid::SetColumn(_scrollBar, 1);

        _root = container;
        _swapChainPanel = swapChainPanel;
        _controlRoot.Content(_root);

        _ApplyUISettings();
        _ApplyConnectionSettings();

        // These are important:
        // 1. When we get tapped, focus us
        _controlRoot.Tapped([this](auto&, auto& e) {
            _controlRoot.Focus(FocusState::Pointer);
            e.Handled(true);
        });
        // 2. Make sure we can be focused (why this isn't `Focusable` I'll never know)
        _controlRoot.IsTabStop(true);
        // 3. Actually not sure about this one. Maybe it isn't necessary either.
        _controlRoot.AllowFocusOnInteraction(true);

        // DON'T CALL _InitializeTerminal here - wait until the swap chain is loaded to do that.

        // Subscribe to the connection's disconnected event and call our connection closed handlers.
        _connection.TerminalDisconnected([=]() {
            _connectionClosedHandlers();
        });
    }

    // Method Description:
    // - Given new settings for this profile, applies the settings to the current terminal.
    // Arguments:
    // - newSettings: New settings values for the profile in this terminal.
    // Return Value:
    // - <none>
    void TermControl::UpdateSettings(Settings::IControlSettings newSettings)
    {
        _settings = newSettings;

        // Dispatch a call to the UI thread to apply the new settings to the
        // terminal.
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this]() {
            // Update our control settings
            _ApplyUISettings();
            // Update the terminal core with its new Core settings
            _terminal->UpdateSettings(_settings);

            // Refresh our font with the renderer
            _UpdateFont();

            const auto width = _swapChainPanel.ActualWidth();
            const auto height = _swapChainPanel.ActualHeight();
            if (width != 0 && height != 0)
            {
                // If the font size changed, or the _swapchainPanel's size changed
                // for any reason, we'll need to make sure to also resize the
                // buffer. _DoResize will invalidate everything for us.
                auto lock = _terminal->LockForWriting();
                _DoResize(width, height);
            }
        });
    }

    // Method Description:
    // - Style our UI elements based on the values in our _settings, and set up
    //   other control-specific settings. This method will be called whenever
    //   the settings are reloaded.
    //   * Calls _InitializeBackgroundBrush to set up the Xaml brush responsible
    //     for the control's background
    //   * Calls _BackgroundColorChanged to style the background of the control
    // - Core settings will be passed to the terminal in _InitializeTerminal
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermControl::_ApplyUISettings()
    {
        _InitializeBackgroundBrush();

        uint32_t bg = _settings.DefaultBackground();
        _BackgroundColorChanged(bg);

        // Apply padding to the root Grid
        auto thickness = _ParseThicknessFromPadding(_settings.Padding());
        _root.Padding(thickness);

        // Initialize our font information.
        const auto* fontFace = _settings.FontFace().c_str();
        const short fontHeight = gsl::narrow<short>(_settings.FontSize());
        // The font width doesn't terribly matter, we'll only be using the
        //      height to look it up
        // The other params here also largely don't matter.
        //      The family is only used to determine if the font is truetype or
        //      not, but DX doesn't use that info at all.
        //      The Codepage is additionally not actually used by the DX engine at all.
        _actualFont = { fontFace, 0, 10, { 0, fontHeight }, CP_UTF8, false };
        _desiredFont = { _actualFont };
    }

    // Method Description:
    // - Set up the brush used to display the control's background.
    // - Respects the settings for acrylic, background image and opacity from
    //   _settings.
    //   * Prioritizes the acrylic background if chosen, respecting acrylicOpacity
    //       from _settings.
    //   * If acrylic is not enabled and a backgroundImage is present, it is used,
    //       respecting the opacity and stretch mode settings from _settings.
    //   * Falls back to a solid color background from _settings if acrylic is not
    //       enabled and no background image is present.
    // - Avoids image flickering and acrylic brush redraw if settings are changed
    //   but the appropriate brush is still in place.
    // - Does not apply background color outside of acrylic mode;
    //   _BackgroundColorChanged must be called to do so.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermControl::_InitializeBackgroundBrush()
    {
        if (_settings.UseAcrylic())
        {
            // See if we've already got an acrylic background brush
            // to avoid the flicker when setting up a new one
            auto acrylic = _root.Background().try_as<Media::AcrylicBrush>();

            // Instantiate a brush if there's not already one there
            if (acrylic == nullptr)
            {
                acrylic = Media::AcrylicBrush{};
                acrylic.BackgroundSource(Media::AcrylicBackgroundSource::HostBackdrop);
            }

            // see GH#1082: Initialize background color so we don't get a
            // fade/flash when _BackgroundColorChanged is called
            uint32_t color = _settings.DefaultBackground();
            winrt::Windows::UI::Color bgColor{};
            bgColor.R = GetRValue(color);
            bgColor.G = GetGValue(color);
            bgColor.B = GetBValue(color);
            bgColor.A = 255;

            acrylic.FallbackColor(bgColor);
            acrylic.TintColor(bgColor);

            // Apply brush settings
            acrylic.TintOpacity(_settings.TintOpacity());

            // Apply brush to control if it's not already there
            if (_root.Background() != acrylic)
            {
                _root.Background(acrylic);
            }
        }
        else if (!_settings.BackgroundImage().empty())
        {
            Windows::Foundation::Uri imageUri{ _settings.BackgroundImage() };

            // Check if the existing brush is an image brush, and if not
            // construct a new one
            auto brush = _root.Background().try_as<Media::ImageBrush>();

            if (brush == nullptr)
            {
                brush = Media::ImageBrush{};
            }

            // Check if the image brush is already pointing to the image
            // in the modified settings; if it isn't (or isn't there),
            // set a new image source for the brush
            auto imageSource = brush.ImageSource().try_as<Media::Imaging::BitmapImage>();

            if (imageSource == nullptr ||
                imageSource.UriSource() == nullptr ||
                imageSource.UriSource().RawUri() != imageUri.RawUri())
            {
                // Note that BitmapImage handles the image load asynchronously,
                // which is especially important since the image
                // may well be both large and somewhere out on the
                // internet.
                Media::Imaging::BitmapImage image(imageUri);
                brush.ImageSource(image);
            }

            // Apply stretch and opacity settings
            brush.Stretch(_settings.BackgroundImageStretchMode());
            brush.Opacity(_settings.BackgroundImageOpacity());

            // Apply brush if it isn't already there
            if (_root.Background() != brush)
            {
                _root.Background(brush);
            }
        }
        else
        {
            Media::SolidColorBrush solidColor{};
            _root.Background(solidColor);
        }
    }

    // Method Description:
    // - Style the background of the control with the provided background color
    // Arguments:
    // - color: The background color to use as a uint32 (aka DWORD COLORREF)
    // Return Value:
    // - <none>
    void TermControl::_BackgroundColorChanged(const uint32_t color)
    {
        _root.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [this, color]() {
            const auto R = GetRValue(color);
            const auto G = GetGValue(color);
            const auto B = GetBValue(color);

            winrt::Windows::UI::Color bgColor{};
            bgColor.R = R;
            bgColor.G = G;
            bgColor.B = B;
            bgColor.A = 255;

            if (_settings.UseAcrylic())
            {
                if (auto acrylic = _root.Background().try_as<Media::AcrylicBrush>())
                {
                    acrylic.FallbackColor(bgColor);
                    acrylic.TintColor(bgColor);
                }

                // If we're acrylic, we want to make sure that the default BG color
                // is transparent, so we can see the acrylic effect on text with the
                // default BG color.
                _settings.DefaultBackground(ARGB(0, R, G, B));
            }
            else if (!_settings.BackgroundImage().empty())
            {
                // This currently applies no changes to the image background
                // brush itself.

                // Set the default background as transparent to prevent the
                // DX layer from overwriting the background image
                _settings.DefaultBackground(ARGB(0, R, G, B));
            }
            else
            {
                if (auto solidColor = _root.Background().try_as<Media::SolidColorBrush>())
                {
                    solidColor.Color(bgColor);
                }

                _settings.DefaultBackground(RGB(R, G, B));
            }
        });
    }

    // Method Description:
    // - Create a connection based on the values in our settings object.
    //   * Gets the commandline and working directory out of the _settings and
    //     creates a ConhostConnection with the given commandline and starting
    //     directory.
    void TermControl::_ApplyConnectionSettings()
    {
        _connection = TerminalConnection::ConhostConnection(_settings.Commandline(), _settings.StartingDirectory(), 30, 80, winrt::guid());
    }

    TermControl::~TermControl()
    {
        Close();
    }

    UIElement TermControl::GetRoot()
    {
        return _root;
    }

    Controls::UserControl TermControl::GetControl()
    {
        return _controlRoot;
    }

    void TermControl::SwapChainChanged()
    {
        if (!_initializedTerminal)
        {
            return;
        }

        auto chain = _renderEngine->GetSwapChain();
        _swapChainPanel.Dispatcher().RunAsync(CoreDispatcherPriority::High, [=]() {
            auto lock = _terminal->LockForWriting();
            auto nativePanel = _swapChainPanel.as<ISwapChainPanelNative>();
            nativePanel->SetSwapChain(chain.Get());
        });
    }

    void TermControl::_InitializeTerminal()
    {
        if (_initializedTerminal)
        {
            return;
        }

        const auto windowWidth = _swapChainPanel.ActualWidth(); // Width() and Height() are NaN?
        const auto windowHeight = _swapChainPanel.ActualHeight();

        _terminal = std::make_unique<::Microsoft::Terminal::Core::Terminal>();

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

        THROW_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

        // Set up the DX Engine
        auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
        _renderer->AddRenderEngine(dxEngine.get());

        // Set up the renderer to be used to calculate the width of a glyph,
        //      should we be unable to figure out its width another way.
        auto pfn = std::bind(&::Microsoft::Console::Render::Renderer::IsGlyphWideByFont, _renderer.get(), std::placeholders::_1);
        SetGlyphWidthFallback(pfn);

        // Initialize our font with the renderer
        // We don't have to care about DPI. We'll get a change message immediately if it's not 96
        // and react accordingly.
        _UpdateFont();

        const COORD windowSize{ static_cast<short>(windowWidth), static_cast<short>(windowHeight) };

        // Fist set up the dx engine with the window size in pixels.
        // Then, using the font, get the number of characters that can fit.
        // Resize our terminal connection to match that size, and initialize the terminal with that size.
        const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
        THROW_IF_FAILED(dxEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));
        const auto vp = dxEngine->GetViewportInCharacters(viewInPixels);
        const auto width = vp.Width();
        const auto height = vp.Height();
        _connection.Resize(height, width);

        // Override the default width and height to match the size of the swapChainPanel
        _settings.InitialCols(width);
        _settings.InitialRows(height);

        _terminal->CreateFromSettings(_settings, renderTarget);

        // Tell the DX Engine to notify us when the swap chain changes.
        dxEngine->SetCallback(std::bind(&TermControl::SwapChainChanged, this));

        THROW_IF_FAILED(dxEngine->Enable());
        _renderEngine = std::move(dxEngine);

        auto onRecieveOutputFn = [this](const hstring str) {
            _terminal->Write(str.c_str());
        };
        _connectionOutputEventToken = _connection.TerminalOutput(onRecieveOutputFn);

        auto inputFn = std::bind(&TermControl::_SendInputToConnection, this, std::placeholders::_1);
        _terminal->SetWriteInputCallback(inputFn);

        auto chain = _renderEngine->GetSwapChain();
        _swapChainPanel.Dispatcher().RunAsync(CoreDispatcherPriority::High, [this, chain]() {
            _terminal->LockConsole();
            auto nativePanel = _swapChainPanel.as<ISwapChainPanelNative>();
            nativePanel->SetSwapChain(chain.Get());
            _terminal->UnlockConsole();
        });

        // Set up the height of the ScrollViewer and the grid we're using to fake our scrolling height
        auto bottom = _terminal->GetViewport().BottomExclusive();
        auto bufferHeight = bottom;

        const auto originalMaximum = _scrollBar.Maximum();
        const auto originalMinimum = _scrollBar.Minimum();
        const auto originalValue = _scrollBar.Value();
        const auto originalViewportSize = _scrollBar.ViewportSize();

        _scrollBar.Maximum(bufferHeight - bufferHeight);
        _scrollBar.Minimum(0);
        _scrollBar.Value(0);
        _scrollBar.ViewportSize(bufferHeight);
        _scrollBar.ValueChanged({ this, &TermControl::_ScrollbarChangeHandler });

        // Apply settings for scrollbar
        if (_settings.ScrollState() == ScrollbarState::Visible)
        {
            _scrollBar.IndicatorMode(Controls::Primitives::ScrollingIndicatorMode::MouseIndicator);
        }
        else if (_settings.ScrollState() == ScrollbarState::Hidden)
        {
            _scrollBar.IndicatorMode(Controls::Primitives::ScrollingIndicatorMode::None);

            // In the scenario where the user has turned off the OS setting to automatically hide scollbars, the
            // Terminal scrollbar would still be visible; so, we need to set the control's visibility accordingly to
            // achieve the intended effect.
            _scrollBar.Visibility(Visibility::Collapsed);
        }
        else
        {
            // Default behavior
            _scrollBar.IndicatorMode(Controls::Primitives::ScrollingIndicatorMode::MouseIndicator);
        }

        _root.PointerWheelChanged({ this, &TermControl::_MouseWheelHandler });
        _root.PointerPressed({ this, &TermControl::_MouseClickHandler });
        _root.PointerMoved({ this, &TermControl::_MouseMovedHandler });
        _root.PointerReleased({ this, &TermControl::_PointerReleasedHandler });

        localPointerToThread->EnablePainting();

        // No matter what order these guys are in, The KeyDown's will fire
        //      before the CharacterRecieved, so we can't easily get characters
        //      first, then fallback to getting keys from vkeys.
        // TODO: This apparently handles keys and characters correctly, though
        //      I'd keep an eye on it, and test more.
        // I presume that the characters that aren't translated by terminalInput
        //      just end up getting ignored, and the rest of the input comes
        //      through CharacterRecieved.
        // I don't believe there's a difference between KeyDown and
        //      PreviewKeyDown for our purposes
        // These two handlers _must_ be on _controlRoot, not _root.
        _controlRoot.PreviewKeyDown({ this, &TermControl::_KeyDownHandler });
        _controlRoot.CharacterReceived({ this, &TermControl::_CharacterHandler });

        auto pfnTitleChanged = std::bind(&TermControl::_TerminalTitleChanged, this, std::placeholders::_1);
        _terminal->SetTitleChangedCallback(pfnTitleChanged);

        auto pfnBackgroundColorChanged = std::bind(&TermControl::_BackgroundColorChanged, this, std::placeholders::_1);
        _terminal->SetBackgroundCallback(pfnBackgroundColorChanged);

        auto pfnScrollPositionChanged = std::bind(&TermControl::_TerminalScrollPositionChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        _terminal->SetScrollPositionChangedCallback(pfnScrollPositionChanged);

        // Set up blinking cursor
        int blinkTime = GetCaretBlinkTime();
        if (blinkTime != INFINITE)
        {
            // Create a timer
            _cursorTimer = std::make_optional(DispatcherTimer());
            _cursorTimer.value().Interval(std::chrono::milliseconds(blinkTime));
            _cursorTimer.value().Tick({ this, &TermControl::_BlinkCursor });
            _cursorTimer.value().Start();
        }
        else
        {
            // The user has disabled cursor blinking
            _cursorTimer = std::nullopt;
        }

        _gotFocusRevoker = _controlRoot.GotFocus(winrt::auto_revoke, { this, &TermControl::_GotFocusHandler });
        _lostFocusRevoker = _controlRoot.LostFocus(winrt::auto_revoke, { this, &TermControl::_LostFocusHandler });

        // Focus the control here. If we do it up above (in _Create_), then the
        //      focus won't actually get passed to us. I believe this is because
        //      we're not technically a part of the UI tree yet, so focusing us
        //      becomes a no-op.
        _controlRoot.Focus(FocusState::Programmatic);

        _connection.Start();
        _initializedTerminal = true;
    }

    void TermControl::_CharacterHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                        Input::CharacterReceivedRoutedEventArgs const& e)
    {
        if (_closing)
        {
            return;
        }

        const auto ch = e.Character();

        // We want Backspace to be handled by _KeyDownHandler, so the
        // terminalInput can translate it into a \x7f. So, do nothing here, so
        // we don't end up sending both a BS and a DEL to the terminal.
        // Also skip processing DEL here, which will hit here when Ctrl+Bkspc is
        // pressed, but after it's handled by the _KeyDownHandler below.
        if (ch == UNICODE_BACKSPACE || ch == UNICODE_DEL)
        {
            return;
        }
        else if (Utf16Parser::IsLeadingSurrogate(ch))
        {
            if (_leadingSurrogate.has_value())
            {
                // we already were storing a leading surrogate but we got another one. Go ahead and send the
                // saved surrogate piece and save the new one
                auto hstr = to_hstring(_leadingSurrogate.value());
                _connection.WriteInput(hstr);
            }
            // save the leading portion of a surrogate pair so that they can be sent at the same time
            _leadingSurrogate.emplace(ch);
        }
        else if (_leadingSurrogate.has_value())
        {
            std::wstring wstr;
            wstr.reserve(2);
            wstr.push_back(_leadingSurrogate.value());
            wstr.push_back(ch);
            _leadingSurrogate.reset();

            auto hstr = to_hstring(wstr.c_str());
            _connection.WriteInput(hstr);
        }
        else
        {
            auto hstr = to_hstring(ch);
            _connection.WriteInput(hstr);
        }
        e.Handled(true);
    }

    void TermControl::_KeyDownHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                      Input::KeyRoutedEventArgs const& e)
    {
        // mark event as handled and do nothing if...
        //   - closing
        //   - key modifier is pressed
        // NOTE: for key combos like CTRL + C, two events are fired (one for CTRL, one for 'C'). We care about the 'C' event and then check for key modifiers below.
        if (_closing ||
            e.OriginalKey() == VirtualKey::Control ||
            e.OriginalKey() == VirtualKey::Shift ||
            e.OriginalKey() == VirtualKey::Menu)
        {
            e.Handled(true);
            return;
        }

        auto modifiers = _GetPressedModifierKeys();

        const auto vkey = static_cast<WORD>(e.OriginalKey());

        bool handled = false;
        auto bindings = _settings.KeyBindings();
        if (bindings)
        {
            KeyChord chord(modifiers, vkey);
            handled = bindings.TryKeyChord(chord);
        }

        if (!handled)
        {
            _terminal->ClearSelection();
            // If the terminal translated the key, mark the event as handled.
            // This will prevent the system from trying to get the character out
            // of it and sending us a CharacterRecieved event.
            handled = _terminal->SendKeyEvent(vkey,
                                              WI_IsFlagSet(modifiers, KeyModifiers::Ctrl),
                                              WI_IsFlagSet(modifiers, KeyModifiers::Alt),
                                              WI_IsFlagSet(modifiers, KeyModifiers::Shift));

            if (_cursorTimer.has_value())
            {
                // Manually show the cursor when a key is pressed. Restarting
                // the timer prevents flickering.
                _terminal->SetCursorVisible(true);
                _cursorTimer.value().Start();
            }
        }

        // Manually prevent keyboard navigation with tab. We want to send tab to
        // the terminal, and we don't want to be able to escape focus of the
        // control with tab.
        if (e.OriginalKey() == VirtualKey::Tab)
        {
            handled = true;
        }

        e.Handled(handled);
    }

    // Method Description:
    // - handle a mouse click event. Begin selection process.
    // Arguments:
    // - sender: not used
    // - args: event data
    void TermControl::_MouseClickHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                         Input::PointerRoutedEventArgs const& args)
    {
        const auto ptr = args.Pointer();
        const auto point = args.GetCurrentPoint(_root);

        if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Mouse)
        {
            // Ignore mouse events while the terminal does not have focus.
            // This prevents the user from selecting and copying text if they
            // click inside the current tab to refocus the terminal window.
            if (!_focused)
            {
                args.Handled(true);
                return;
            }

            const auto modifiers = args.KeyModifiers();
            const auto altEnabled = WI_IsFlagSet(modifiers, VirtualKeyModifiers::Menu);
            const auto shiftEnabled = WI_IsFlagSet(modifiers, VirtualKeyModifiers::Shift);

            if (point.Properties().IsLeftButtonPressed())
            {
                const auto cursorPosition = point.Position();
                const auto terminalPosition = _GetTerminalPosition(cursorPosition);

                // save location before rendering
                _terminal->SetSelectionAnchor(terminalPosition);

                // handle ALT key
                _terminal->SetBoxSelection(altEnabled);

                _renderer->TriggerSelection();
            }
            else if (point.Properties().IsRightButtonPressed())
            {
                // copy selection, if one exists
                if (_terminal->IsSelectionActive())
                {
                    CopySelectionToClipboard(!shiftEnabled);
                }
                // paste selection, otherwise
                else
                {
                    // attach TermControl::_SendInputToConnection() as the clipboardDataHandler.
                    // This is called when the clipboard data is loaded.
                    auto clipboardDataHandler = std::bind(&TermControl::_SendInputToConnection, this, std::placeholders::_1);
                    auto pasteArgs = winrt::make_self<PasteFromClipboardEventArgs>(clipboardDataHandler);

                    // send paste event up to TermApp
                    _clipboardPasteHandlers(*this, *pasteArgs);
                }
            }
        }
        else if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            const auto contactRect = point.Properties().ContactRect();
            // Set our touch rect, to start a pan.
            _touchAnchor = winrt::Windows::Foundation::Point{ contactRect.X, contactRect.Y };
        }

        args.Handled(true);
    }

    // Method Description:
    // - handle a mouse moved event. Specifically handling mouse drag to update selection process.
    // Arguments:
    // - sender: not used
    // - args: event data
    void TermControl::_MouseMovedHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                         Input::PointerRoutedEventArgs const& args)
    {
        const auto ptr = args.Pointer();
        const auto point = args.GetCurrentPoint(_root);

        if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Mouse)
        {
            if (point.Properties().IsLeftButtonPressed())
            {
                const auto cursorPosition = point.Position();
                const auto terminalPosition = _GetTerminalPosition(cursorPosition);

                // save location (for rendering) + render
                _terminal->SetEndSelectionPosition(terminalPosition);
                _renderer->TriggerSelection();
            }
        }
        else if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Touch && _touchAnchor)
        {
            const auto contactRect = point.Properties().ContactRect();
            winrt::Windows::Foundation::Point newTouchPoint{ contactRect.X, contactRect.Y };
            const auto anchor = _touchAnchor.value();

            // Get the difference between the point we've dragged to and the start of the touch.
            const float fontHeight = float(_actualFont.GetSize().Y);

            const float dy = newTouchPoint.Y - anchor.Y;

            // If we've moved more than one row of text, we'll want to scroll the viewport
            if (std::abs(dy) > fontHeight)
            {
                // Multiply by -1, because moving the touch point down will
                // create a positive delta, but we want the viewport to move up,
                // so we'll need a negative scroll amount (and the inverse for
                // panning down)
                const float numRows = -1.0f * (dy / fontHeight);

                const auto currentOffset = this->GetScrollOffset();
                const double newValue = (numRows) + (currentOffset);

                // Clear our expected scroll offset. The viewport will now move
                //      in response to our user input.
                _lastScrollOffset = std::nullopt;
                _scrollBar.Value(static_cast<int>(newValue));

                // Use this point as our new scroll anchor.
                _touchAnchor = newTouchPoint;
            }
        }
        args.Handled(true);
    }

    // Method Description:
    // - Event handler for the PointerReleased event. We use this to de-anchor
    //   touch events, to stop scrolling via touch.
    // Arguments:
    // - sender: not used
    // - args: event data
    // Return Value:
    // - <none>
    void TermControl::_PointerReleasedHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                              Input::PointerRoutedEventArgs const& args)
    {
        const auto ptr = args.Pointer();

        if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            _touchAnchor = std::nullopt;
        }

        args.Handled(true);
    }

    // Method Description:
    // - Event handler for the PointerWheelChanged event. This is raised in
    //   response to mouse wheel changes. Depending upon what modifier keys are
    //   pressed, different actions will take place.
    // Arguments:
    // - args: the event args containing information about t`he mouse wheel event.
    void TermControl::_MouseWheelHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                         Input::PointerRoutedEventArgs const& args)
    {
        auto delta = args.GetCurrentPoint(_root).Properties().MouseWheelDelta();
        // Get the state of the Ctrl & Shift keys
        const auto modifiers = args.KeyModifiers();
        const auto ctrlPressed = WI_IsFlagSet(modifiers, VirtualKeyModifiers::Control);
        const auto shiftPressed = WI_IsFlagSet(modifiers, VirtualKeyModifiers::Shift);

        if (ctrlPressed && shiftPressed)
        {
            _MouseTransparencyHandler(delta);
        }
        else if (ctrlPressed)
        {
            _MouseZoomHandler(delta);
        }
        else
        {
            _MouseScrollHandler(delta);
        }
    }

    // Method Description:
    // - Adjust the opacity of the acrylic background in response to a mouse
    //   scrolling event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    void TermControl::_MouseTransparencyHandler(const double mouseDelta)
    {
        // Transparency is on a scale of [0.0,1.0], so only increment by .01.
        const auto effectiveDelta = mouseDelta < 0 ? -.01 : .01;

        if (_settings.UseAcrylic())
        {
            try
            {
                auto acrylicBrush = _root.Background().as<Media::AcrylicBrush>();
                acrylicBrush.TintOpacity(acrylicBrush.TintOpacity() + effectiveDelta);
            }
            CATCH_LOG();
        }
    }

    // Method Description:
    // - Adjust the font size of the terminal in response to a mouse scrolling
    //   event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    void TermControl::_MouseZoomHandler(const double mouseDelta)
    {
        const auto fontDelta = mouseDelta < 0 ? -1 : 1;
        try
        {
            // Make sure we have a non-zero font size
            const auto newSize = std::max(gsl::narrow<short>(_desiredFont.GetEngineSize().Y + fontDelta), static_cast<short>(1));
            const auto* fontFace = _settings.FontFace().c_str();
            _actualFont = { fontFace, 0, 10, { 0, newSize }, CP_UTF8, false };
            _desiredFont = { _actualFont };

            // Refresh our font with the renderer
            _UpdateFont();
            // Resize the terminal's BUFFER to match the new font size. This does
            // NOT change the size of the window, because that can lead to more
            // problems (like what happens when you change the font size while the
            // window is maximized?)
            auto lock = _terminal->LockForWriting();
            _DoResize(_swapChainPanel.ActualWidth(), _swapChainPanel.ActualHeight());
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Scroll the visible viewport in response to a mouse wheel event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    void TermControl::_MouseScrollHandler(const double mouseDelta)
    {
        const auto currentOffset = this->GetScrollOffset();

        // negative = down, positive = up
        // However, for us, the signs are flipped.
        const auto rowDelta = mouseDelta < 0 ? 1.0 : -1.0;

        // TODO: Should we be getting some setting from the system
        //      for number of lines scrolled?
        // With one of the precision mouses, one click is always a multiple of 120,
        // but the "smooth scrolling" mode results in non-int values

        // Conhost seems to use four lines at a time, so we'll emulate that for now.
        double newValue = (4 * rowDelta) + (currentOffset);

        // Clear our expected scroll offset. The viewport will now move in
        //      response to our user input.
        _lastScrollOffset = std::nullopt;
        // The scroll bar's ValueChanged handler will actually move the viewport
        //      for us.
        _scrollBar.Value(static_cast<int>(newValue));
    }

    void TermControl::_ScrollbarChangeHandler(Windows::Foundation::IInspectable const& sender,
                                              Controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        const auto newValue = args.NewValue();

        // If we've stored a lastScrollOffset, that means the terminal has
        //      initiated some scrolling operation. We're responding to that event here.
        if (_lastScrollOffset.has_value())
        {
            // If this event's offset is the same as the last offset message
            //      we've sent, then clear out the expected offset. We do this
            //      because in that case, the message we're replying to was the
            //      last scroll event we raised.
            // Regardless, we're going to ignore this message, because the
            //      terminal is already in the scroll position it wants.
            const auto ourLastOffset = _lastScrollOffset.value();
            if (newValue == ourLastOffset)
            {
                _lastScrollOffset = std::nullopt;
            }
        }
        else
        {
            // This is a scroll event that wasn't initiated by the termnial
            //      itself - it was initiated by the mouse wheel, or the scrollbar.
            this->ScrollViewport(static_cast<int>(newValue));
        }
    }

    // Method Description:
    // - Event handler for the GotFocus event. This is used to start
    //   blinking the cursor when the window is focused.
    void TermControl::_GotFocusHandler(Windows::Foundation::IInspectable const& /* sender */,
                                       RoutedEventArgs const& /* args */)
    {
        if (_closing)
        {
            return;
        }
        _focused = true;

        if (_cursorTimer.has_value())
        {
            _cursorTimer.value().Start();
        }
    }

    // Method Description:
    // - Event handler for the LostFocus event. This is used to hide
    //   and stop blinking the cursor when the window loses focus.
    void TermControl::_LostFocusHandler(Windows::Foundation::IInspectable const& /* sender */,
                                        RoutedEventArgs const& /* args */)
    {
        if (_closing)
        {
            return;
        }
        _focused = false;

        if (_cursorTimer.has_value())
        {
            _cursorTimer.value().Stop();
            _terminal->SetCursorVisible(false);
        }
    }

    void TermControl::_SendInputToConnection(const std::wstring& wstr)
    {
        _connection.WriteInput(wstr);
    }

    // Method Description:
    // - Update the font with the renderer. This will be called either when the
    //      font changes or the DPI changes, as DPI changes will necessitate a
    //      font change. This method will *not* change the buffer/viewport size
    //      to account for the new glyph dimensions. Callers should make sure to
    //      appropriately call _DoResize after this method is called.
    void TermControl::_UpdateFont()
    {
        auto lock = _terminal->LockForWriting();

        const int newDpi = static_cast<int>(static_cast<double>(USER_DEFAULT_SCREEN_DPI) * _swapChainPanel.CompositionScaleX());

        // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
        //      actually fail. We need a way to gracefully fallback.
        _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);
    }

    // Method Description:
    // - Triggered when the swapchain changes size. We use this to resize the
    //      terminal buffers to match the new visible size.
    // Arguments:
    // - e: a SizeChangedEventArgs with the new dimensions of the SwapChainPanel
    void TermControl::_SwapChainSizeChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                            SizeChangedEventArgs const& e)
    {
        if (!_initializedTerminal)
        {
            return;
        }

        auto lock = _terminal->LockForWriting();

        const auto foundationSize = e.NewSize();

        _DoResize(foundationSize.Width, foundationSize.Height);
    }

    void TermControl::_SwapChainScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel const& sender,
                                             Windows::Foundation::IInspectable const& /*args*/)
    {
        const auto scale = sender.CompositionScaleX();
        const auto dpi = (int)(scale * USER_DEFAULT_SCREEN_DPI);

        // TODO: MSFT: 21169071 - Shouldn't this all happen through _renderer and trigger the invalidate automatically on DPI change?
        THROW_IF_FAILED(_renderEngine->UpdateDpi(dpi));
        _renderer->TriggerRedrawAll();
    }

    // Method Description:
    // - Toggle the cursor on and off when called by the cursor blink timer.
    // Arguments:
    // - sender: not used
    // - e: not used
    void TermControl::_BlinkCursor(Windows::Foundation::IInspectable const& /* sender */,
                                   Windows::Foundation::IInspectable const& /* e */)
    {
        if ((_closing) || (!_terminal->IsCursorBlinkingAllowed() && _terminal->IsCursorVisible()))
        {
            return;
        }
        _terminal->SetCursorVisible(!_terminal->IsCursorVisible());
    }

    // Method Description:
    // - Process a resize event that was initiated by the user. This can either be due to the user resizing the window (causing the swapchain to resize) or due to the DPI changing (causing us to need to resize the buffer to match)
    // Arguments:
    // - newWidth: the new width of the swapchain, in pixels.
    // - newHeight: the new height of the swapchain, in pixels.
    void TermControl::_DoResize(const double newWidth, const double newHeight)
    {
        SIZE size;
        size.cx = static_cast<long>(newWidth);
        size.cy = static_cast<long>(newHeight);

        // Tell the dx engine that our window is now the new size.
        THROW_IF_FAILED(_renderEngine->SetWindowSize(size));

        // Invalidate everything
        _renderer->TriggerRedrawAll();

        // Convert our new dimensions to characters
        const auto viewInPixels = Viewport::FromDimensions({ 0, 0 },
                                                           { static_cast<short>(size.cx), static_cast<short>(size.cy) });
        const auto vp = _renderEngine->GetViewportInCharacters(viewInPixels);

        // If this function succeeds with S_FALSE, then the terminal didn't
        //      actually change size. No need to notify the connection of this
        //      no-op.
        // TODO: MSFT:20642295 Resizing the buffer will corrupt it
        // I believe we'll need support for CSI 2J, and additionally I think
        //      we're resetting the viewport to the top
        const HRESULT hr = _terminal->UserResize({ vp.Width(), vp.Height() });
        if (SUCCEEDED(hr) && hr != S_FALSE)
        {
            _connection.Resize(vp.Height(), vp.Width());
        }
    }

    void TermControl::_TerminalTitleChanged(const std::wstring_view& wstr)
    {
        _titleChangedHandlers(winrt::hstring{ wstr });
    }

    // Method Description:
    // - Update the postion and size of the scrollbar to match the given
    //      viewport top, viewport height, and buffer size.
    //   The change will be actually handled in _ScrollbarChangeHandler.
    //   This should be done on the UI thread. Make sure the caller is calling
    //      us in a RunAsync block.
    // Arguments:
    // - viewTop: the top of the visible viewport, in rows. 0 indicates the top
    //      of the buffer.
    // - viewHeight: the height of the viewport in rows.
    // - bufferSize: the length of the buffer, in rows
    void TermControl::_ScrollbarUpdater(Controls::Primitives::ScrollBar scrollBar,
                                        const int viewTop,
                                        const int viewHeight,
                                        const int bufferSize)
    {
        const auto hiddenContent = bufferSize - viewHeight;
        scrollBar.Maximum(hiddenContent);
        scrollBar.Minimum(0);
        scrollBar.ViewportSize(viewHeight);

        scrollBar.Value(viewTop);
    }

    // Method Description:
    // - Update the postion and size of the scrollbar to match the given
    //      viewport top, viewport height, and buffer size.
    //   Additionally fires a ScrollPositionChanged event for anyone who's
    //      registered an event handler for us.
    // Arguments:
    // - viewTop: the top of the visible viewport, in rows. 0 indicates the top
    //      of the buffer.
    // - viewHeight: the height of the viewport in rows.
    // - bufferSize: the length of the buffer, in rows
    void TermControl::_TerminalScrollPositionChanged(const int viewTop,
                                                     const int viewHeight,
                                                     const int bufferSize)
    {
        // Update our scrollbar
        _scrollBar.Dispatcher().RunAsync(CoreDispatcherPriority::Low, [=]() {
            _ScrollbarUpdater(_scrollBar, viewTop, viewHeight, bufferSize);
        });

        // Set this value as our next expected scroll position.
        _lastScrollOffset = { viewTop };
        _scrollPositionChangedHandlers(viewTop, viewHeight, bufferSize);
    }

    hstring TermControl::Title()
    {
        if (!_initializedTerminal)
            return L"";

        hstring hstr(_terminal->GetConsoleTitle());
        return hstr;
    }

    // Method Description:
    // - get text from buffer and send it to the Windows Clipboard (CascadiaWin32:main.cpp). Also removes rendering of selection.
    // Arguments:
    // - trimTrailingWhitespace: enable removing any whitespace from copied selection
    //    and get text to appear on separate lines.
    void TermControl::CopySelectionToClipboard(bool trimTrailingWhitespace)
    {
        // extract text from buffer
        const auto copiedData = _terminal->RetrieveSelectedTextFromBuffer(trimTrailingWhitespace);

        _terminal->ClearSelection();

        // send data up for clipboard
        _clipboardCopyHandlers(copiedData);
    }

    void TermControl::Close()
    {
        if (!_closing.exchange(true))
        {
            // Stop accepting new output before we disconnect everything.
            _connection.TerminalOutput(_connectionOutputEventToken);

            // Clear out the cursor timer, so it doesn't trigger again on us once we're destructed.
            if (auto localCursorTimer{ std::exchange(_cursorTimer, std::nullopt) })
            {
                localCursorTimer->Stop();
                // cursorTimer timer, now stopped, is destroyed.
            }

            if (auto localConnection{ std::exchange(_connection, nullptr) })
            {
                localConnection.Close();
                // connection is destroyed.
            }

            if (auto localRenderEngine{ std::exchange(_renderEngine, nullptr) })
            {
                if (auto localRenderer{ std::exchange(_renderer, nullptr) })
                {
                    localRenderer->TriggerTeardown();
                    // renderer is destroyed
                }
                // renderEngine is destroyed
            }

            if (auto localTerminal{ std::exchange(_terminal, nullptr) })
            {
                _initializedTerminal = false;
                // terminal is destroyed.
            }
        }
    }

    void TermControl::ScrollViewport(int viewTop)
    {
        _terminal->UserScrollViewport(viewTop);
    }

    // Method Description:
    // - Scrolls the viewport of the terminal and updates the scroll bar accordingly
    // Arguments:
    // - viewTop: the viewTop to scroll to
    // The difference between this function and ScrollViewport is that this one also
    // updates the _scrollBar after the viewport scroll. The reason _scrollBar is not updated in
    // ScrollViewport is because ScrollViewport is being called by _ScrollbarChangeHandler
    void TermControl::KeyboardScrollViewport(int viewTop)
    {
        _terminal->UserScrollViewport(viewTop);
        _lastScrollOffset = std::nullopt;
        _scrollBar.Value(static_cast<int>(viewTop));
    }

    int TermControl::GetScrollOffset()
    {
        return _terminal->GetScrollOffset();
    }

    // Function Description:
    // - Gets the height of the terminal in lines of text
    // Return Value:
    // - The height of the terminal in lines of text
    int TermControl::GetViewHeight() const
    {
        const auto viewPort = _terminal->GetViewport();
        return viewPort.Height();
    }

    // Function Description:
    // - Determines how much space (in pixels) an app would need to reserve to
    //   create a control with the settings stored in the settings param. This
    //   accounts for things like the font size and face, the initialRows and
    //   initialCols, and scrollbar visibility. The returned sized is based upon
    //   the provided DPI value
    // Arguments:
    // - settings: A IControlSettings with the settings to get the pixel size of.
    // - dpi: The DPI we should create the terminal at. This affects things such
    //   as font size, scrollbar and other control scaling, etc. Make sure the
    //   caller knows what monitor the control is about to appear on.
    // Return Value:
    // - a point containing the requested dimensions in pixels.
    winrt::Windows::Foundation::Point TermControl::GetProposedDimensions(IControlSettings const& settings, const uint32_t dpi)
    {
        // Initialize our font information.
        const auto* fontFace = settings.FontFace().c_str();
        const short fontHeight = gsl::narrow<short>(settings.FontSize());
        // The font width doesn't terribly matter, we'll only be using the
        //      height to look it up
        // The other params here also largely don't matter.
        //      The family is only used to determine if the font is truetype or
        //      not, but DX doesn't use that info at all.
        //      The Codepage is additionally not actually used by the DX engine at all.
        FontInfo actualFont = { fontFace, 0, 10, { 0, fontHeight }, CP_UTF8, false };
        FontInfoDesired desiredFont = { actualFont };

        // If the settings have negative or zero row or column counts, ignore those counts.
        // (The lower TerminalCore layer also has upper bounds as well, but at this layer
        //  we may eventually impose different ones depending on how many pixels we can address.)
        const auto cols = std::max(settings.InitialCols(), 1);
        const auto rows = std::max(settings.InitialRows(), 1);

        // Create a DX engine and initialize it with our font and DPI. We'll
        // then use it to measure how much space the requested rows and columns
        // will take up.
        // TODO: MSFT:21254947 - use a static function to do this instead of
        // instantiating a DxEngine
        auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
        THROW_IF_FAILED(dxEngine->UpdateDpi(dpi));
        THROW_IF_FAILED(dxEngine->UpdateFont(desiredFont, actualFont));

        const float scale = dxEngine->GetScaling();
        const auto fontSize = actualFont.GetSize();

        // Manually multiply by the scaling factor. The DX engine doesn't
        // actually store the scaled font size in the fontInfo.GetSize()
        // property when the DX engine is in Composition mode (which it is for
        // the Terminal). At runtime, this is fine, as we'll transform
        // everything by our scaling, so it'll work out. However, right now we
        // need to get the exact pixel count.
        const float fFontWidth = gsl::narrow<float>(fontSize.X * scale);
        const float fFontHeight = gsl::narrow<float>(fontSize.Y * scale);

        // UWP XAML scrollbars aren't guaranteed to be the same size as the
        // ComCtl scrollbars, but it's certainly close enough.
        const auto scrollbarSize = GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);

        double width = cols * fFontWidth;

        // Reserve additional space if scrollbar is intended to be visible
        if (settings.ScrollState() == ScrollbarState::Visible)
        {
            width += scrollbarSize;
        }

        double height = rows * fFontHeight;
        auto thickness = _ParseThicknessFromPadding(settings.Padding());
        width += thickness.Left + thickness.Right;
        height += thickness.Top + thickness.Bottom;

        return { gsl::narrow_cast<float>(width), gsl::narrow_cast<float>(height) };
    }

    // Method Description:
    // - Create XAML Thickness object based on padding props provided.
    //   Used for controlling the TermControl XAML Grid container's Padding prop.
    // Arguments:
    // - padding: 2D padding values
    //      Single Double value provides uniform padding
    //      Two Double values provide isometric horizontal & vertical padding
    //      Four Double values provide independent padding for 4 sides of the bounding rectangle
    // Return Value:
    // - Windows::UI::Xaml::Thickness object
    Windows::UI::Xaml::Thickness TermControl::_ParseThicknessFromPadding(const hstring padding)
    {
        const wchar_t singleCharDelim = L',';
        std::wstringstream tokenStream(padding.c_str());
        std::wstring token;
        uint8_t paddingPropIndex = 0;
        std::array<double, 4> thicknessArr = {};
        size_t* idx = nullptr;

        // Get padding values till we run out of delimiter separated values in the stream
        //  or we hit max number of allowable values (= 4) for the bounding rectangle
        // Non-numeral values detected will default to 0
        // std::getline will not throw exception unless flags are set on the wstringstream
        // std::stod will throw invalid_argument expection if the input is an invalid double value
        // std::stod will throw out_of_range expection if the input value is more than DBL_MAX
        try
        {
            for (; std::getline(tokenStream, token, singleCharDelim) && (paddingPropIndex < thicknessArr.size()); paddingPropIndex++)
            {
                // std::stod internall calls wcstod which handles whitespace prefix (which is ignored)
                //  & stops the scan when first char outside the range of radix is encountered
                // We'll be permissive till the extent that stod function allows us to be by default
                // Ex. a value like 100.3#535w2 will be read as 100.3, but ;df25 will fail
                thicknessArr[paddingPropIndex] = std::stod(token, idx);
            }
        }
        catch (...)
        {
            // If something goes wrong, even if due to a single bad padding value, we'll reset the index & return default 0 padding
            paddingPropIndex = 0;
            LOG_CAUGHT_EXCEPTION();
        }

        switch (paddingPropIndex)
        {
        case 1:
            return ThicknessHelper::FromUniformLength(thicknessArr[0]);
        case 2:
            return ThicknessHelper::FromLengths(thicknessArr[0], thicknessArr[1], thicknessArr[0], thicknessArr[1]);
        // No case for paddingPropIndex = 3, since it's not a norm to provide just Left, Top & Right padding values leaving out Bottom
        case 4:
            return ThicknessHelper::FromLengths(thicknessArr[0], thicknessArr[1], thicknessArr[2], thicknessArr[3]);
        default:
            return Thickness();
        }
    }

    // Method Description:
    // - Get the modifier keys that are currently pressed. This can be used to
    //   find out which modifiers (ctrl, alt, shift) are pressed in events that
    //   don't necessarily include that state.
    // Return Value:
    // - a KeyModifiers value with flags set for each key that's pressed.
    Settings::KeyModifiers TermControl::_GetPressedModifierKeys() const
    {
        CoreWindow window = CoreWindow::GetForCurrentThread();
        // DONT USE
        //      != CoreVirtualKeyStates::None
        // OR
        //      == CoreVirtualKeyStates::Down
        // Sometimes with the key down, the state is Down | Locked.
        // Sometimes with the key up, the state is Locked.
        // IsFlagSet(Down) is the only correct solution.
        const auto ctrlKeyState = window.GetKeyState(VirtualKey::Control);
        const auto shiftKeyState = window.GetKeyState(VirtualKey::Shift);
        const auto altKeyState = window.GetKeyState(VirtualKey::Menu);

        const auto ctrl = WI_IsFlagSet(ctrlKeyState, CoreVirtualKeyStates::Down);
        const auto shift = WI_IsFlagSet(shiftKeyState, CoreVirtualKeyStates::Down);
        const auto alt = WI_IsFlagSet(altKeyState, CoreVirtualKeyStates::Down);

        return KeyModifiers{ (ctrl ? Settings::KeyModifiers::Ctrl : Settings::KeyModifiers::None) |
                             (alt ? Settings::KeyModifiers::Alt : Settings::KeyModifiers::None) |
                             (shift ? Settings::KeyModifiers::Shift : Settings::KeyModifiers::None) };
    }

    // Method Description:
    // - Returns true if this control should close when its connection is closed.
    // Arguments:
    // - <none>
    // Return Value:
    // - true iff the control should close when the connection is closed.
    bool TermControl::ShouldCloseOnExit() const noexcept
    {
        return _settings.CloseOnExit();
    }

    // Method Description:
    // - Gets the corresponding viewport terminal position for the cursor
    //    by excluding the padding and normalizing with the font size.
    //    This is used for selection.
    // Arguments:
    // - cursorPosition: the (x,y) position of a given cursor (i.e.: mouse cursor).
    //    NOTE: origin (0,0) is top-left.
    // Return Value:
    // - the corresponding viewport terminal position for the given Point parameter
    const COORD TermControl::_GetTerminalPosition(winrt::Windows::Foundation::Point cursorPosition)
    {
        // Exclude padding from cursor position calculation
        COORD terminalPosition = {
            static_cast<SHORT>(cursorPosition.X - _root.Padding().Left),
            static_cast<SHORT>(cursorPosition.Y - _root.Padding().Top)
        };

        const auto fontSize = _actualFont.GetSize();
        FAIL_FAST_IF(fontSize.X == 0);
        FAIL_FAST_IF(fontSize.Y == 0);

        // Normalize to terminal coordinates by using font size
        terminalPosition.X /= fontSize.X;
        terminalPosition.Y /= fontSize.Y;

        return terminalPosition;
    }

    // clang-format off
    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT(TermControl, TitleChanged,          _titleChangedHandlers,          TerminalControl::TitleChangedEventArgs);
    DEFINE_EVENT(TermControl, ConnectionClosed,      _connectionClosedHandlers,      TerminalControl::ConnectionClosedEventArgs);
    DEFINE_EVENT(TermControl, CopyToClipboard,       _clipboardCopyHandlers,         TerminalControl::CopyToClipboardEventArgs);
    DEFINE_EVENT(TermControl, ScrollPositionChanged, _scrollPositionChangedHandlers, TerminalControl::ScrollPositionChangedEventArgs);
    // clang-format on

    DEFINE_EVENT_WITH_TYPED_EVENT_HANDLER(TermControl, PasteFromClipboard, _clipboardPasteHandlers, TerminalControl::TermControl, TerminalControl::PasteFromClipboardEventArgs);
}
