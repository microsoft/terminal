// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TermControl.h"
#include <argb.h>
#include <DefaultSettings.h>
#include <unicode.hpp>
#include <Utf16Parser.hpp>
#include <Utils.h>
#include <WinUser.h>
#include <LibraryResources.h>
#include "../../types/inc/GlyphWidth.hpp"
#include "../../types/inc/Utils.hpp"

#include "TermControl.g.cpp"
#include "TermControlAutomationPeer.h"

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Console::VirtualTerminal;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::System;
using namespace winrt::Windows::ApplicationModel::DataTransfer;

// The minimum delay between updates to the scroll bar's values.
// The updates are throttled to limit power usage.
constexpr const auto ScrollBarUpdateInterval = std::chrono::milliseconds(8);

// The minimum delay between updating the TSF input control.
constexpr const auto TsfRedrawInterval = std::chrono::milliseconds(100);

// The minimum delay between updating the locations of regex patterns
constexpr const auto UpdatePatternLocationsInterval = std::chrono::milliseconds(500);

// The minimum delay between emitting warning bells
constexpr const auto TerminalWarningBellInterval = std::chrono::milliseconds(1000);

DEFINE_ENUM_FLAG_OPERATORS(winrt::Microsoft::Terminal::Control::CopyFormat);

namespace winrt::Microsoft::Terminal::Control::implementation
{
    TermControl::TermControl(IControlSettings settings,
                             TerminalConnection::ITerminalConnection connection) :
        _initializedTerminal{ false },
        _settings{ settings },
        _closing{ false },
        _isInternalScrollBarUpdate{ false },
        _autoScrollVelocity{ 0 },
        _autoScrollingPointerPoint{ std::nullopt },
        _autoScrollTimer{},
        _lastAutoScrollUpdateTime{ std::nullopt },
        _cursorTimer{},
        _blinkTimer{},
        _searchBox{ nullptr }
    {
        InitializeComponent();

        _interactivity = winrt::make_self<ControlInteractivity>(settings, connection);
        _core = _interactivity->GetCore();

        // Use a manual revoker on the output event, so we can immediately stop
        // worrying about it on destruction.
        _coreOutputEventToken = _core->ReceivedOutput({ this, &TermControl::_coreReceivedOutput });

        // These events might all be triggered by the connection, but that
        // should be drained and closed before we complete destruction. So these
        // are safe.
        _core->ScrollPositionChanged({ this, &TermControl::_ScrollPositionChanged });
        _core->WarningBell({ this, &TermControl::_coreWarningBell });
        _core->CursorPositionChanged({ this, &TermControl::_CursorPositionChanged });

        // This event is specifically triggered by the renderer thread, a BG thread. Use a weak ref here.
        _core->RendererEnteredErrorState({ get_weak(), &TermControl::_RendererEnteredErrorState });

        // These callbacks can only really be triggered by UI interactions. So
        // they don't need weak refs - they can't be triggered unless we're
        // alive.
        _core->BackgroundColorChanged({ this, &TermControl::_BackgroundColorChangedHandler });
        _core->FontSizeChanged({ this, &TermControl::_coreFontSizeChanged });
        _core->TransparencyChanged({ this, &TermControl::_coreTransparencyChanged });
        _core->RaiseNotice({ this, &TermControl::_coreRaisedNotice });
        _core->HoveredHyperlinkChanged({ this, &TermControl::_hoveredHyperlinkChanged });
        _interactivity->OpenHyperlink({ this, &TermControl::_HyperlinkHandler });
        _interactivity->ScrollPositionChanged({ this, &TermControl::_ScrollPositionChanged });

        // Initialize the terminal only once the swapchainpanel is loaded - that
        //      way, we'll be able to query the real pixel size it got on layout
        _layoutUpdatedRevoker = SwapChainPanel().LayoutUpdated(winrt::auto_revoke, [this](auto /*s*/, auto /*e*/) {
            // This event fires every time the layout changes, but it is always the last one to fire
            // in any layout change chain. That gives us great flexibility in finding the right point
            // at which to initialize our renderer (and our terminal).
            // Any earlier than the last layout update and we may not know the terminal's starting size.

            if (_InitializeTerminal())
            {
                // Only let this succeed once.
                _layoutUpdatedRevoker.revoke();
            }
        });

        // Many of these ThrottledFunc's should be inside ControlCore. However,
        // currently they depend on the Dispatcher() of the UI thread, which the
        // Core eventually won't have access to. When we get to
        // https://github.com/microsoft/terminal/projects/5#card-50760282
        // then we'll move the applicable ones.
        _tsfTryRedrawCanvas = std::make_shared<ThrottledFunc<>>(
            [weakThis = get_weak()]() {
                if (auto control{ weakThis.get() })
                {
                    control->TSFInputControl().TryRedrawCanvas();
                }
            },
            TsfRedrawInterval,
            Dispatcher());

        _updatePatternLocations = std::make_shared<ThrottledFunc<>>(
            [weakThis = get_weak()]() {
                if (auto control{ weakThis.get() })
                {
                    control->_core->UpdatePatternLocations();
                }
            },
            UpdatePatternLocationsInterval,
            Dispatcher());

        _playWarningBell = std::make_shared<ThrottledFunc<>>(
            [weakThis = get_weak()]() {
                if (auto control{ weakThis.get() })
                {
                    control->_WarningBellHandlers(*control, nullptr);
                }
            },
            TerminalWarningBellInterval,
            Dispatcher());

        _updateScrollBar = std::make_shared<ThrottledFunc<ScrollBarUpdate>>(
            [weakThis = get_weak()](const auto& update) {
                if (auto control{ weakThis.get() })
                {
                    control->_isInternalScrollBarUpdate = true;

                    auto scrollBar = control->ScrollBar();
                    if (update.newValue.has_value())
                    {
                        scrollBar.Value(update.newValue.value());
                    }
                    scrollBar.Maximum(update.newMaximum);
                    scrollBar.Minimum(update.newMinimum);
                    scrollBar.ViewportSize(update.newViewportSize);
                    // scroll one full screen worth at a time when the scroll bar is clicked
                    scrollBar.LargeChange(std::max(update.newViewportSize - 1, 0.));

                    control->_isInternalScrollBarUpdate = false;
                }
            },
            ScrollBarUpdateInterval,
            Dispatcher());

        static constexpr auto AutoScrollUpdateInterval = std::chrono::microseconds(static_cast<int>(1.0 / 30.0 * 1000000));
        _autoScrollTimer.Interval(AutoScrollUpdateInterval);
        _autoScrollTimer.Tick({ this, &TermControl::_UpdateAutoScroll });

        _ApplyUISettings(_settings);
    }

    // Method Description:
    // - Loads the search box from the xaml UI and focuses it.
    void TermControl::CreateSearchBoxControl()
    {
        // Lazy load the search box control.
        if (auto loadedSearchBox{ FindName(L"SearchBox") })
        {
            if (auto searchBox{ loadedSearchBox.try_as<::winrt::Microsoft::Terminal::Control::SearchBoxControl>() })
            {
                // get at its private implementation
                _searchBox.copy_from(winrt::get_self<implementation::SearchBoxControl>(searchBox));
                _searchBox->Visibility(Visibility::Visible);

                // If a text is selected inside terminal, use it to populate the search box.
                // If the search box already contains a value, it will be overridden.
                if (_core->HasSelection())
                {
                    // Currently we populate the search box only if a single line is selected.
                    // Empirically, multi-line selection works as well on sample scenarios,
                    // but since code paths differ, extra work is required to ensure correctness.
                    auto bufferText = _core->SelectedText(true);
                    if (bufferText.size() == 1)
                    {
                        const auto selectedLine{ til::at(bufferText, 0) };
                        _searchBox->PopulateTextbox(selectedLine.data());
                    }
                }

                _searchBox->SetFocusOnTextbox();
            }
        }
    }

    void TermControl::SearchMatch(const bool goForward)
    {
        if (_closing)
        {
            return;
        }
        if (!_searchBox)
        {
            CreateSearchBoxControl();
        }
        else
        {
            _core->Search(_searchBox->TextBox().Text(), goForward, false);
        }
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
    void TermControl::_Search(const winrt::hstring& text,
                              const bool goForward,
                              const bool caseSensitive)
    {
        _core->Search(text, goForward, caseSensitive);
    }

    // Method Description:
    // - The handler for the close button or pressing "Esc" when focusing on the
    //   search dialog.
    // Arguments:
    // - IInspectable: not used
    // - RoutedEventArgs: not used
    // Return Value:
    // - <none>
    void TermControl::_CloseSearchBoxControl(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                             RoutedEventArgs const& /*args*/)
    {
        _searchBox->Visibility(Visibility::Collapsed);

        // Set focus back to terminal control
        this->Focus(FocusState::Programmatic);
    }

    // Method Description:
    // - Given Settings having been updated, applies the settings to the current terminal.
    // Return Value:
    // - <none>
    winrt::fire_and_forget TermControl::UpdateSettings()
    {
        auto weakThis{ get_weak() };

        // Dispatch a call to the UI thread to apply the new settings to the
        // terminal.
        co_await winrt::resume_foreground(Dispatcher());

        _UpdateSettingsFromUIThread(_settings);

        auto appearance = _settings.try_as<IControlAppearance>();
        if (!_focused && _UnfocusedAppearance)
        {
            appearance = _UnfocusedAppearance;
        }
        _UpdateAppearanceFromUIThread(appearance);
    }

    // Method Description:
    // - Dispatches a call to the UI thread and updates the appearance
    // Arguments:
    // - newAppearance: the new appearance to set
    winrt::fire_and_forget TermControl::UpdateAppearance(const IControlAppearance newAppearance)
    {
        // Dispatch a call to the UI thread
        co_await winrt::resume_foreground(Dispatcher());

        _UpdateAppearanceFromUIThread(newAppearance);
    }

    // Method Description:
    // - Updates the settings of the current terminal.
    // - This method is separate from UpdateSettings because there is an apparent optimizer
    //   issue that causes one of our hstring -> wstring_view conversions to result in garbage,
    //   but only from a coroutine context. See GH#8723.
    // - INVARIANT: This method must be called from the UI thread.
    // Arguments:
    // - newSettings: the new settings to set
    void TermControl::_UpdateSettingsFromUIThread(IControlSettings newSettings)
    {
        if (_closing)
        {
            return;
        }

        _core->UpdateSettings(_settings);

        // Update our control settings
        _ApplyUISettings(_settings);
    }

    // Method Description:
    // - Updates the appearance
    // - INVARIANT: This method must be called from the UI thread.
    // Arguments:
    // - newAppearance: the new appearance to set
    void TermControl::_UpdateAppearanceFromUIThread(IControlAppearance newAppearance)
    {
        if (_closing)
        {
            return;
        }

        if (!newAppearance.BackgroundImage().empty())
        {
            Windows::Foundation::Uri imageUri{ newAppearance.BackgroundImage() };

            // Check if the image brush is already pointing to the image
            // in the modified settings; if it isn't (or isn't there),
            // set a new image source for the brush
            auto imageSource = BackgroundImage().Source().try_as<Media::Imaging::BitmapImage>();

            if (imageSource == nullptr ||
                imageSource.UriSource() == nullptr ||
                imageSource.UriSource().RawUri() != imageUri.RawUri())
            {
                // Note that BitmapImage handles the image load asynchronously,
                // which is especially important since the image
                // may well be both large and somewhere out on the
                // internet.
                Media::Imaging::BitmapImage image(imageUri);
                BackgroundImage().Source(image);
            }

            // Apply stretch, opacity and alignment settings
            BackgroundImage().Stretch(newAppearance.BackgroundImageStretchMode());
            BackgroundImage().Opacity(newAppearance.BackgroundImageOpacity());
            BackgroundImage().HorizontalAlignment(newAppearance.BackgroundImageHorizontalAlignment());
            BackgroundImage().VerticalAlignment(newAppearance.BackgroundImageVerticalAlignment());
        }
        else
        {
            BackgroundImage().Source(nullptr);
        }

        // Update our control settings
        const auto bg = newAppearance.DefaultBackground();
        _changeBackgroundColor(bg);

        // Set TSF Foreground
        Media::SolidColorBrush foregroundBrush{};
        foregroundBrush.Color(static_cast<til::color>(newAppearance.DefaultForeground()));
        TSFInputControl().Foreground(foregroundBrush);

        _core->UpdateAppearance(newAppearance);
    }

    // Method Description:
    // - Writes the given sequence as input to the active terminal connection,
    // Arguments:
    // - wstr: the string of characters to write to the terminal connection.
    // Return Value:
    // - <none>
    void TermControl::SendInput(const winrt::hstring& wstr)
    {
        _core->SendInput(wstr);
    }

    void TermControl::ToggleShaderEffects()
    {
        _core->ToggleShaderEffects();
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
    void TermControl::_ApplyUISettings(const IControlSettings& newSettings)
    {
        _InitializeBackgroundBrush();

        const auto bg = newSettings.DefaultBackground();
        _changeBackgroundColor(bg);

        // Apply padding as swapChainPanel's margin
        auto newMargin = _ParseThicknessFromPadding(newSettings.Padding());
        SwapChainPanel().Margin(newMargin);

        TSFInputControl().Margin(newMargin);

        // Apply settings for scrollbar
        if (newSettings.ScrollState() == ScrollbarState::Hidden)
        {
            // In the scenario where the user has turned off the OS setting to automatically hide scrollbars, the
            // Terminal scrollbar would still be visible; so, we need to set the control's visibility accordingly to
            // achieve the intended effect.
            ScrollBar().IndicatorMode(Controls::Primitives::ScrollingIndicatorMode::None);
            ScrollBar().Visibility(Visibility::Collapsed);
        }
        else // (default or Visible)
        {
            // Default behavior
            ScrollBar().IndicatorMode(Controls::Primitives::ScrollingIndicatorMode::MouseIndicator);
            ScrollBar().Visibility(Visibility::Visible);
        }

        _interactivity->UpdateSettings();
    }

    // Method Description:
    // - Set up each layer's brush used to display the control's background.
    // - Respects the settings for acrylic, background image and opacity from
    //   _settings.
    //   * If acrylic is not enabled, setup a solid color background, otherwise
    //       use bgcolor as acrylic's tint
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
            auto acrylic = RootGrid().Background().try_as<Media::AcrylicBrush>();

            // Instantiate a brush if there's not already one there
            if (acrylic == nullptr)
            {
                acrylic = Media::AcrylicBrush{};
                acrylic.BackgroundSource(Media::AcrylicBackgroundSource::HostBackdrop);
            }

            // see GH#1082: Initialize background color so we don't get a
            // fade/flash when _BackgroundColorChanged is called
            auto bgColor = til::color{ _settings.DefaultBackground() }.with_alpha(0xff);

            acrylic.FallbackColor(bgColor);
            acrylic.TintColor(bgColor);

            // Apply brush settings
            acrylic.TintOpacity(_settings.TintOpacity());

            // Apply brush to control if it's not already there
            if (RootGrid().Background() != acrylic)
            {
                RootGrid().Background(acrylic);
            }

            // GH#5098: Inform the engine of the new opacity of the default text background.
            _core->SetBackgroundOpacity(::base::saturated_cast<float>(_settings.TintOpacity()));
        }
        else
        {
            Media::SolidColorBrush solidColor{};
            RootGrid().Background(solidColor);

            // GH#5098: Inform the engine of the new opacity of the default text background.
            _core->SetBackgroundOpacity(1.0f);
        }
    }

    // Method Description:
    // - Style the background of the control with the provided background color
    // Arguments:
    // - color: The background color to use as a uint32 (aka DWORD COLORREF)
    // Return Value:
    // - <none>
    void TermControl::_BackgroundColorChangedHandler(const IInspectable& /*sender*/,
                                                     const IInspectable& /*args*/)
    {
        til::color newBgColor{ _core->BackgroundColor() };
        _changeBackgroundColor(newBgColor);
    }

    winrt::fire_and_forget TermControl::_changeBackgroundColor(const til::color bg)
    {
        auto weakThis{ get_weak() };
        co_await winrt::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            if (auto acrylic = RootGrid().Background().try_as<Media::AcrylicBrush>())
            {
                acrylic.FallbackColor(bg);
                acrylic.TintColor(bg);
            }
            else if (auto solidColor = RootGrid().Background().try_as<Media::SolidColorBrush>())
            {
                solidColor.Color(bg);
            }
        }
    }

    TermControl::~TermControl()
    {
        Close();
    }

    // Method Description:
    // - Creates an automation peer for the Terminal Control, enabling accessibility on our control.
    // Arguments:
    // - None
    // Return Value:
    // - The automation peer for our control
    Windows::UI::Xaml::Automation::Peers::AutomationPeer TermControl::OnCreateAutomationPeer()
    try
    {
        if (_initializedTerminal && !_closing) // only set up the automation peer if we're ready to go live
        {
            // create a custom automation peer with this code pattern:
            // (https://docs.microsoft.com/en-us/windows/uwp/design/accessibility/custom-automation-peers)
            auto autoPeer = winrt::make_self<implementation::TermControlAutomationPeer>(this);

            _uiaEngine = std::make_unique<::Microsoft::Console::Render::UiaEngine>(autoPeer.get());
            _core->AttachUiaEngine(_uiaEngine.get());
            return *autoPeer;
        }
        return nullptr;
    }
    catch (...)
    {
        LOG_CAUGHT_EXCEPTION();
        return nullptr;
    }

    ::Microsoft::Console::Types::IUiaData* TermControl::GetUiaData() const
    {
        return _core->GetUiaData();
    }

    // This is needed for TermControlAutomationPeer. We probably could find a
    // clever way around asking the core for this.
    til::point TermControl::GetFontSize() const
    {
        return _core->GetFont().GetSize();
    }

    const Windows::UI::Xaml::Thickness TermControl::GetPadding()
    {
        return SwapChainPanel().Margin();
    }

    TerminalConnection::ConnectionState TermControl::ConnectionState() const
    {
        return _core->ConnectionState();
    }

    winrt::fire_and_forget TermControl::RenderEngineSwapChainChanged(IInspectable /*sender*/, IInspectable /*args*/)
    {
        // This event is only registered during terminal initialization,
        // so we don't need to check _initializedTerminal.
        // We also don't lock for things that come back from the renderer.
        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            const auto chainHandle = _core->GetSwapChainHandle();
            _AttachDxgiSwapChainToXaml(chainHandle);
        }
    }

    // Method Description:
    // - Called when the renderer triggers a warning. It might do this when it
    //   fails to find a shader file, or fails to compile a shader. We'll take
    //   that renderer warning, and display a dialog to the user with and
    //   appropriate error message. WE'll display the dialog with our
    //   RaiseNotice event.
    // Arguments:
    // - hr: an  HRESULT describing the warning
    // Return Value:
    // - <none>
    winrt::fire_and_forget TermControl::_RendererWarning(IInspectable /*sender*/,
                                                         Control::RendererWarningArgs args)
    {
        const auto hr = static_cast<HRESULT>(args.Result());

        auto weakThis{ get_weak() };
        co_await winrt::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            winrt::hstring message;
            if (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr ||
                HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) == hr)
            {
                message = { fmt::format(std::wstring_view{ RS_(L"PixelShaderNotFound") },
                                        _settings.PixelShaderPath()) };
            }
            else if (D2DERR_SHADER_COMPILE_FAILED == hr)
            {
                message = { fmt::format(std::wstring_view{ RS_(L"PixelShaderCompileFailed") }) };
            }
            else
            {
                message = { fmt::format(std::wstring_view{ RS_(L"UnexpectedRendererError") },
                                        hr) };
            }

            auto noticeArgs = winrt::make<NoticeEventArgs>(NoticeLevel::Warning, std::move(message));
            control->_RaiseNoticeHandlers(*control, std::move(noticeArgs));
        }
    }

    void TermControl::_AttachDxgiSwapChainToXaml(HANDLE swapChainHandle)
    {
        auto nativePanel = SwapChainPanel().as<ISwapChainPanelNative2>();
        nativePanel->SetSwapChainHandle(swapChainHandle);
    }

    bool TermControl::_InitializeTerminal()
    {
        if (_initializedTerminal)
        {
            return false;
        }

        const auto panelWidth = SwapChainPanel().ActualWidth();
        const auto panelHeight = SwapChainPanel().ActualHeight();
        const auto panelScaleX = SwapChainPanel().CompositionScaleX();
        const auto panelScaleY = SwapChainPanel().CompositionScaleY();

        const auto windowWidth = panelWidth * panelScaleX;
        const auto windowHeight = panelHeight * panelScaleY;

        if (windowWidth == 0 || windowHeight == 0)
        {
            return false;
        }

        // IMPORTANT! Set this callback up sooner rather than later. If we do it
        // after Enable, then it'll be possible to paint the frame once
        // _before_ the warning handler is set up, and then warnings from
        // the first paint will be ignored!
        _core->RendererWarning({ get_weak(), &TermControl::_RendererWarning });

        const auto coreInitialized = _core->Initialize(panelWidth,
                                                       panelHeight,
                                                       panelScaleX);
        if (!coreInitialized)
        {
            return false;
        }
        _interactivity->Initialize();

        _AttachDxgiSwapChainToXaml(_core->GetSwapChainHandle());

        // Tell the DX Engine to notify us when the swap chain changes. We do
        // this after we initially set the swapchain so as to avoid unnecessary
        // callbacks (and locking problems)
        _core->SwapChainChanged({ get_weak(), &TermControl::RenderEngineSwapChainChanged });

        // !! LOAD BEARING !!
        // Make sure you enable painting _AFTER_ calling _AttachDxgiSwapChainToXaml
        //
        // If you EnablePainting first, then you almost certainly won't have any
        // problems when running in Debug. However, in Release, you'll run into
        // issues where the Renderer starts trying to paint before we've
        // actually attached the swapchain to anything, and the DxEngine is not
        // prepared to handle that.
        _core->EnablePainting();

        auto bufferHeight = _core->BufferHeight();

        ScrollBar().Maximum(bufferHeight - bufferHeight);
        ScrollBar().Minimum(0);
        ScrollBar().Value(0);
        ScrollBar().ViewportSize(bufferHeight);
        ScrollBar().LargeChange(std::max(bufferHeight - 1, 0)); // scroll one "screenful" at a time when the scroll bar is clicked

        // Set up blinking cursor
        int blinkTime = GetCaretBlinkTime();
        if (blinkTime != INFINITE)
        {
            // Create a timer
            DispatcherTimer cursorTimer;
            cursorTimer.Interval(std::chrono::milliseconds(blinkTime));
            cursorTimer.Tick({ get_weak(), &TermControl::_CursorTimerTick });
            cursorTimer.Start();
            _cursorTimer.emplace(std::move(cursorTimer));
        }
        else
        {
            // The user has disabled cursor blinking
            _cursorTimer = std::nullopt;
        }

        // Set up blinking attributes
        BOOL animationsEnabled = TRUE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animationsEnabled, 0);
        if (animationsEnabled && blinkTime != INFINITE)
        {
            // Create a timer
            DispatcherTimer blinkTimer;
            blinkTimer.Interval(std::chrono::milliseconds(blinkTime));
            blinkTimer.Tick({ get_weak(), &TermControl::_BlinkTimerTick });
            blinkTimer.Start();
            _blinkTimer.emplace(std::move(blinkTimer));
        }
        else
        {
            // The user has disabled blinking
            _blinkTimer = std::nullopt;
        }

        // Now that the renderer is set up, update the appearance for initialization
        _UpdateAppearanceFromUIThread(_settings);

        // Focus the control here. If we do it during control initialization, then
        //      focus won't actually get passed to us. I believe this is because
        //      we're not technically a part of the UI tree yet, so focusing us
        //      becomes a no-op.
        this->Focus(FocusState::Programmatic);

        _initializedTerminal = true;

        // Likewise, run the event handlers outside of lock (they could
        // be reentrant)
        _InitializedHandlers(*this, nullptr);
        return true;
    }

    void TermControl::_CharacterHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                        Input::CharacterReceivedRoutedEventArgs const& e)
    {
        if (_closing)
        {
            return;
        }

        _HidePointerCursorHandlers(*this, nullptr);

        const auto ch = e.Character();
        const auto scanCode = gsl::narrow_cast<WORD>(e.KeyStatus().ScanCode);
        auto modifiers = _GetPressedModifierKeys();
        if (e.KeyStatus().IsExtendedKey)
        {
            modifiers |= ControlKeyStates::EnhancedKey;
        }
        const bool handled = _core->SendCharEvent(ch, scanCode, modifiers);
        e.Handled(handled);
    }

    // Method Description:
    // - Manually handles key events for certain keys that can't be passed to us
    //   normally. Namely, the keys we're concerned with are F7 down and Alt up.
    // Return value:
    // - Whether the key was handled.
    bool TermControl::OnDirectKeyEvent(const uint32_t vkey, const uint8_t scanCode, const bool down)
    {
        // Short-circuit isReadOnly check to avoid warning dialog
        if (_core->IsInReadOnlyMode())
        {
            return false;
        }

        const auto modifiers{ _GetPressedModifierKeys() };
        auto handled = false;

        if (vkey == VK_MENU && !down)
        {
            // Manually generate an Alt KeyUp event into the key bindings or terminal.
            //   This is required as part of GH#6421.
            (void)_TrySendKeyEvent(VK_MENU, scanCode, modifiers, false);
            handled = true;
        }
        else if (vkey == VK_F7 && down)
        {
            // Manually generate an F7 event into the key bindings or terminal.
            //   This is required as part of GH#638.
            auto bindings{ _settings.KeyBindings() };

            if (bindings)
            {
                handled = bindings.TryKeyChord({
                    modifiers.IsCtrlPressed(),
                    modifiers.IsAltPressed(),
                    modifiers.IsShiftPressed(),
                    VK_F7,
                });
            }

            if (!handled)
            {
                // _TrySendKeyEvent pretends it didn't handle F7 for some unknown reason.
                (void)_TrySendKeyEvent(VK_F7, scanCode, modifiers, true);
                // GH#6438: Note that we're _not_ sending the key up here - that'll
                // get passed through XAML to our KeyUp handler normally.
                handled = true;
            }
        }
        return handled;
    }

    void TermControl::_KeyDownHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                      Input::KeyRoutedEventArgs const& e)
    {
        _KeyHandler(e, true);
    }

    void TermControl::_KeyUpHandler(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                    Input::KeyRoutedEventArgs const& e)
    {
        _KeyHandler(e, false);
    }

    void TermControl::_KeyHandler(Input::KeyRoutedEventArgs const& e, const bool keyDown)
    {
        // If the current focused element is a child element of searchbox,
        // we do not send this event up to terminal
        if (_searchBox && _searchBox->ContainsFocus())
        {
            return;
        }

        // Mark the event as handled and do nothing if we're closing, or the key
        // was the Windows key.
        //
        // NOTE: for key combos like CTRL + C, two events are fired (one for
        // CTRL, one for 'C'). Since it's possible the terminal is in
        // win32-input-mode, then we'll send all these keystrokes to the
        // terminal - it's smart enough to ignore the keys it doesn't care
        // about.
        if (_closing ||
            e.OriginalKey() == VirtualKey::LeftWindows ||
            e.OriginalKey() == VirtualKey::RightWindows)

        {
            e.Handled(true);
            return;
        }

        auto modifiers = _GetPressedModifierKeys();
        const auto vkey = gsl::narrow_cast<WORD>(e.OriginalKey());
        const auto scanCode = gsl::narrow_cast<WORD>(e.KeyStatus().ScanCode);

        // Short-circuit isReadOnly check to avoid warning dialog
        if (_core->IsInReadOnlyMode())
        {
            e.Handled(!keyDown || _TryHandleKeyBinding(vkey, scanCode, modifiers));
            return;
        }

        if (e.KeyStatus().IsExtendedKey)
        {
            modifiers |= ControlKeyStates::EnhancedKey;
        }

        // Alt-Numpad# input will send us a character once the user releases
        // Alt, so we should be ignoring the individual keydowns. The character
        // will be sent through the TSFInputControl. See GH#1401 for more
        // details
        if (modifiers.IsAltPressed() &&
            (e.OriginalKey() >= VirtualKey::NumberPad0 && e.OriginalKey() <= VirtualKey::NumberPad9))

        {
            e.Handled(true);
            return;
        }

        // GH#2235: Terminal::Settings hasn't been modified to differentiate
        // between AltGr and Ctrl+Alt yet.
        // -> Don't check for key bindings if this is an AltGr key combination.
        //
        // GH#4999: Only process keybindings on the keydown. If we don't check
        // this at all, we'll process the keybinding twice. If we only process
        // keybindings on the keyUp, then we'll still send the keydown to the
        // connected terminal application, and something like ctrl+shift+T will
        // emit a ^T to the pipe.
        if (!modifiers.IsAltGrPressed() && keyDown && _TryHandleKeyBinding(vkey, scanCode, modifiers))
        {
            e.Handled(true);
            return;
        }

        if (_TrySendKeyEvent(vkey, scanCode, modifiers, keyDown))
        {
            e.Handled(true);
            return;
        }

        // Manually prevent keyboard navigation with tab. We want to send tab to
        // the terminal, and we don't want to be able to escape focus of the
        // control with tab.
        e.Handled(e.OriginalKey() == VirtualKey::Tab);
    }

    // Method Description:
    // - Attempt to handle this key combination as a key binding
    // Arguments:
    // - vkey: The vkey of the key pressed.
    // - scanCode: The scan code of the key pressed.
    // - modifiers: The ControlKeyStates representing the modifier key states.
    bool TermControl::_TryHandleKeyBinding(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers) const
    {
        auto bindings = _settings.KeyBindings();
        if (!bindings)
        {
            return false;
        }

        auto success = bindings.TryKeyChord({
            modifiers.IsCtrlPressed(),
            modifiers.IsAltPressed(),
            modifiers.IsShiftPressed(),
            modifiers.IsWinPressed(),
            vkey,
        });
        if (!success)
        {
            return false;
        }

        // Let's assume the user has bound the dead key "^" to a sendInput command that sends "b".
        // If the user presses the two keys "^a" it'll produce "bâ", despite us marking the key event as handled.
        // The following is used to manually "consume" such dead keys and clear them from the keyboard state.
        _ClearKeyboardState(vkey, scanCode);
        return true;
    }

    // Method Description:
    // - Discards currently pressed dead keys.
    // Arguments:
    // - vkey: The vkey of the key pressed.
    // - scanCode: The scan code of the key pressed.
    void TermControl::_ClearKeyboardState(const WORD vkey, const WORD scanCode) const noexcept
    {
        std::array<BYTE, 256> keyState;
        if (!GetKeyboardState(keyState.data()))
        {
            return;
        }

        // As described in "Sometimes you *want* to interfere with the keyboard's state buffer":
        //   http://archives.miloush.net/michkap/archive/2006/09/10/748775.html
        // > "The key here is to keep trying to pass stuff to ToUnicode until -1 is not returned."
        std::array<wchar_t, 16> buffer;
        while (ToUnicodeEx(vkey, scanCode, keyState.data(), buffer.data(), gsl::narrow_cast<int>(buffer.size()), 0b1, nullptr) < 0)
        {
        }
    }

    // Method Description:
    // - Send this particular key event to the terminal.
    //   See Terminal::SendKeyEvent for more information.
    // - Clears the current selection.
    // - Makes the cursor briefly visible during typing.
    // Arguments:
    // - vkey: The vkey of the key pressed.
    // - scanCode: The scan code of the key pressed.
    // - states: The Microsoft::Terminal::Core::ControlKeyStates representing the modifier key states.
    // - keyDown: If true, the key was pressed, otherwise the key was released.
    bool TermControl::_TrySendKeyEvent(const WORD vkey,
                                       const WORD scanCode,
                                       const ControlKeyStates modifiers,
                                       const bool keyDown)
    {
        const CoreWindow window = CoreWindow::GetForCurrentThread();

        if (vkey == VK_ESCAPE ||
            vkey == VK_RETURN)
        {
            TSFInputControl().ClearBuffer();
        }

        // If the terminal translated the key, mark the event as handled.
        // This will prevent the system from trying to get the character out
        // of it and sending us a CharacterReceived event.
        const auto handled = vkey ?
                                 _core->TrySendKeyEvent(vkey,
                                                        scanCode,
                                                        modifiers,
                                                        keyDown) :
                                 true;

        if (_cursorTimer.has_value())
        {
            // Manually show the cursor when a key is pressed. Restarting
            // the timer prevents flickering.
            _core->CursorOn(true);
            _cursorTimer.value().Start();
        }

        return handled;
    }

    // Method Description:
    // - handle a tap event by taking focus
    // Arguments:
    // - sender: the XAML element responding to the tap event
    // - args: event data
    void TermControl::_TappedHandler(const IInspectable& /*sender*/, const TappedRoutedEventArgs& e)
    {
        Focus(FocusState::Pointer);
        e.Handled(true);
    }

    // Method Description:
    // - handle a mouse click event. Begin selection process.
    // Arguments:
    // - sender: the XAML element responding to the pointer input
    // - args: event data
    void TermControl::_PointerPressedHandler(Windows::Foundation::IInspectable const& sender,
                                             Input::PointerRoutedEventArgs const& args)
    {
        if (_closing)
        {
            return;
        }

        _RestorePointerCursorHandlers(*this, nullptr);

        _CapturePointer(sender, args);

        const auto ptr = args.Pointer();
        const auto point = args.GetCurrentPoint(*this);
        const auto type = ptr.PointerDeviceType();

        // We also TryShow in GotFocusHandler, but this call is specifically
        // for the case where the Terminal is in focus but the user closed the
        // on-screen keyboard. This lets the user simply tap on the terminal
        // again to bring it up.
        InputPane::GetForCurrentView().TryShow();

        if (!_focused)
        {
            Focus(FocusState::Pointer);
        }

        if (type == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            const auto contactRect = point.Properties().ContactRect();
            auto anchor = til::point{ til::math::rounding, contactRect.X, contactRect.Y };
            _interactivity->TouchPressed(anchor);
        }
        else
        {
            const auto cursorPosition = point.Position();
            _interactivity->PointerPressed(TermControl::GetPressedMouseButtons(point),
                                           TermControl::GetPointerUpdateKind(point),
                                           point.Timestamp(),
                                           ControlKeyStates{ args.KeyModifiers() },
                                           _toTerminalOrigin(cursorPosition));
        }

        args.Handled(true);
    }

    // Method Description:
    // - handle a mouse moved event. Specifically handling mouse drag to update selection process.
    // Arguments:
    // - sender: not used
    // - args: event data
    void TermControl::_PointerMovedHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                           Input::PointerRoutedEventArgs const& args)
    {
        if (_closing)
        {
            return;
        }

        _RestorePointerCursorHandlers(*this, nullptr);

        const auto ptr = args.Pointer();
        const auto point = args.GetCurrentPoint(*this);
        const auto cursorPosition = point.Position();
        const auto pixelPosition = _toTerminalOrigin(cursorPosition);
        const auto type = ptr.PointerDeviceType();

        if (!_focused && _settings.FocusFollowMouse())
        {
            _FocusFollowMouseRequestedHandlers(*this, nullptr);
        }

        if (type == Windows::Devices::Input::PointerDeviceType::Mouse ||
            type == Windows::Devices::Input::PointerDeviceType::Pen)
        {
            _interactivity->PointerMoved(TermControl::GetPressedMouseButtons(point),
                                         TermControl::GetPointerUpdateKind(point),
                                         ControlKeyStates(args.KeyModifiers()),
                                         _focused,
                                         pixelPosition);

            if (_focused && point.Properties().IsLeftButtonPressed())
            {
                const double cursorBelowBottomDist = cursorPosition.Y - SwapChainPanel().Margin().Top - SwapChainPanel().ActualHeight();
                const double cursorAboveTopDist = -1 * cursorPosition.Y + SwapChainPanel().Margin().Top;

                constexpr double MinAutoScrollDist = 2.0; // Arbitrary value
                double newAutoScrollVelocity = 0.0;
                if (cursorBelowBottomDist > MinAutoScrollDist)
                {
                    newAutoScrollVelocity = _GetAutoScrollSpeed(cursorBelowBottomDist);
                }
                else if (cursorAboveTopDist > MinAutoScrollDist)
                {
                    newAutoScrollVelocity = -1.0 * _GetAutoScrollSpeed(cursorAboveTopDist);
                }

                if (newAutoScrollVelocity != 0)
                {
                    _TryStartAutoScroll(point, newAutoScrollVelocity);
                }
                else
                {
                    _TryStopAutoScroll(ptr.PointerId());
                }
            }
        }
        else if (type == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            const auto contactRect = point.Properties().ContactRect();
            til::point newTouchPoint{ til::math::rounding, contactRect.X, contactRect.Y };

            _interactivity->TouchMoved(newTouchPoint, _focused);
        }

        args.Handled(true);
    }

    // Method Description:
    // - Event handler for the PointerReleased event. We use this to de-anchor
    //   touch events, to stop scrolling via touch.
    // Arguments:
    // - sender: the XAML element responding to the pointer input
    // - args: event data
    void TermControl::_PointerReleasedHandler(Windows::Foundation::IInspectable const& sender,
                                              Input::PointerRoutedEventArgs const& args)
    {
        if (_closing)
        {
            return;
        }

        const auto ptr = args.Pointer();
        const auto point = args.GetCurrentPoint(*this);
        const auto cursorPosition = point.Position();
        const auto pixelPosition = _toTerminalOrigin(cursorPosition);
        const auto type = ptr.PointerDeviceType();

        _ReleasePointerCapture(sender, args);

        if (type == Windows::Devices::Input::PointerDeviceType::Mouse ||
            type == Windows::Devices::Input::PointerDeviceType::Pen)
        {
            _interactivity->PointerReleased(TermControl::GetPressedMouseButtons(point),
                                            TermControl::GetPointerUpdateKind(point),
                                            ControlKeyStates(args.KeyModifiers()),
                                            pixelPosition);
        }
        else if (type == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            _interactivity->TouchReleased();
        }

        _TryStopAutoScroll(ptr.PointerId());

        args.Handled(true);
    }

    // Method Description:
    // - Event handler for the PointerWheelChanged event. This is raised in
    //   response to mouse wheel changes. Depending upon what modifier keys are
    //   pressed, different actions will take place.
    // - Primarily just takes the data from the PointerRoutedEventArgs and uses
    //   it to call _DoMouseWheel, see _DoMouseWheel for more details.
    // Arguments:
    // - args: the event args containing information about t`he mouse wheel event.
    void TermControl::_MouseWheelHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                         Input::PointerRoutedEventArgs const& args)
    {
        if (_closing)
        {
            return;
        }

        _RestorePointerCursorHandlers(*this, nullptr);

        const auto point = args.GetCurrentPoint(*this);

        auto result = _interactivity->MouseWheel(ControlKeyStates{ args.KeyModifiers() },
                                                 point.Properties().MouseWheelDelta(),
                                                 _toTerminalOrigin(point.Position()),
                                                 TermControl::GetPressedMouseButtons(point));
        if (result)
        {
            args.Handled(true);
        }
    }

    // Method Description:
    // - This is part of the solution to GH#979
    // - Manually handle a scrolling event. This is used to help support
    //   scrolling on devices where the touchpad doesn't correctly handle
    //   scrolling inactive windows.
    // Arguments:
    // - location: the location of the mouse during this event. This location is
    //   relative to the origin of the control
    // - delta: the mouse wheel delta that triggered this event.
    // - state: the state for each of the mouse buttons individually (pressed/unpressed)
    bool TermControl::OnMouseWheel(const Windows::Foundation::Point location,
                                   const int32_t delta,
                                   const bool leftButtonDown,
                                   const bool midButtonDown,
                                   const bool rightButtonDown)
    {
        const auto modifiers = _GetPressedModifierKeys();
        TerminalInput::MouseButtonState state{ leftButtonDown,
                                               midButtonDown,
                                               rightButtonDown };
        return _interactivity->MouseWheel(modifiers, delta, _toTerminalOrigin(location), state);
    }

    // Method Description:
    // - Called in response to the core's TransparencyChanged event. We'll use
    //   this to update our background brush.
    // - The Core should have already updated the TintOpacity and UseAcrylic
    //   properties in the _settings.
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    winrt::fire_and_forget TermControl::_coreTransparencyChanged(IInspectable /*sender*/,
                                                                 Control::TransparencyChangedEventArgs /*args*/)
    {
        co_await resume_foreground(Dispatcher());
        try
        {
            _InitializeBackgroundBrush();
            const auto bg = _settings.DefaultBackground();
            _changeBackgroundColor(bg);
        }
        CATCH_LOG();
    }

    void TermControl::_coreReceivedOutput(const IInspectable& /*sender*/,
                                          const IInspectable& /*args*/)
    {
        // Queue up a throttled UpdatePatternLocations call. In the future, we
        // should have the _updatePatternLocations ThrottledFunc internal to
        // ControlCore, and run on that object's dispatcher queue.
        //
        // We're not doing that quite yet, because the Core will eventually
        // be out-of-proc from the UI thread, and won't be able to just use
        // the UI thread as the dispatcher queue thread.
        //
        // THIS IS CALLED ON EVERY STRING OF TEXT OUTPUT TO THE TERMINAL. Think
        // twice before adding anything here.

        _updatePatternLocations->Run();
    }

    // Method Description:
    // - Reset the font size of the terminal to its default size.
    // Arguments:
    // - none
    void TermControl::ResetFontSize()
    {
        _core->ResetFontSize();
    }

    // Method Description:
    // - Adjust the font size of the terminal control.
    // Arguments:
    // - fontSizeDelta: The amount to increase or decrease the font size by.
    void TermControl::AdjustFontSize(int fontSizeDelta)
    {
        _core->AdjustFontSize(fontSizeDelta);
    }

    void TermControl::_ScrollbarChangeHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                              Controls::Primitives::RangeBaseValueChangedEventArgs const& args)
    {
        if (_isInternalScrollBarUpdate || _closing)
        {
            // The update comes from ourselves, more specifically from the
            // terminal. So we don't have to update the terminal because it
            // already knows.
            return;
        }

        const auto newValue = args.NewValue();
        _interactivity->UpdateScrollbar(newValue);

        // User input takes priority over terminal events so cancel
        // any pending scroll bar update if the user scrolls.
        _updateScrollBar->ModifyPending([](auto& update) {
            update.newValue.reset();
        });

        _updatePatternLocations->Run();
    }

    // Method Description:
    // - captures the pointer so that none of the other XAML elements respond to pointer events
    // Arguments:
    // - sender: XAML element that is interacting with pointer
    // - args: pointer data (i.e.: mouse, touch)
    // Return Value:
    // - true if we successfully capture the pointer, false otherwise.
    bool TermControl::_CapturePointer(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        IUIElement uielem;
        if (sender.try_as(uielem))
        {
            uielem.CapturePointer(args.Pointer());
            return true;
        }
        return false;
    }

    // Method Description:
    // - releases the captured pointer because we're done responding to XAML pointer events
    // Arguments:
    // - sender: XAML element that is interacting with pointer
    // - args: pointer data (i.e.: mouse, touch)
    // Return Value:
    // - true if we release capture of the pointer, false otherwise.
    bool TermControl::_ReleasePointerCapture(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& args)
    {
        IUIElement uielem;
        if (sender.try_as(uielem))
        {
            uielem.ReleasePointerCapture(args.Pointer());
            return true;
        }
        return false;
    }

    // Method Description:
    // - Starts new pointer related auto scroll behavior, or continues existing one.
    //      Does nothing when there is already auto scroll associated with another pointer.
    // Arguments:
    // - pointerPoint: info about pointer that causes auto scroll. Pointer's position
    //      is later used to update selection.
    // - scrollVelocity: target velocity of scrolling in characters / sec
    void TermControl::_TryStartAutoScroll(Windows::UI::Input::PointerPoint const& pointerPoint, const double scrollVelocity)
    {
        // Allow only one pointer at the time
        if (!_autoScrollingPointerPoint.has_value() ||
            _autoScrollingPointerPoint.value().PointerId() == pointerPoint.PointerId())
        {
            _autoScrollingPointerPoint = pointerPoint;
            _autoScrollVelocity = scrollVelocity;

            // If this is first time the auto scroll update is about to be called,
            //      kick-start it by initializing its time delta as if it started now
            if (!_lastAutoScrollUpdateTime.has_value())
            {
                _lastAutoScrollUpdateTime = std::chrono::high_resolution_clock::now();
            }

            // Apparently this check is not necessary but greatly improves performance
            if (!_autoScrollTimer.IsEnabled())
            {
                _autoScrollTimer.Start();
            }
        }
    }

    // Method Description:
    // - Stops auto scroll if it's active and is associated with supplied pointer id.
    // Arguments:
    // - pointerId: id of pointer for which to stop auto scroll
    void TermControl::_TryStopAutoScroll(const uint32_t pointerId)
    {
        if (_autoScrollingPointerPoint.has_value() &&
            pointerId == _autoScrollingPointerPoint.value().PointerId())
        {
            _autoScrollingPointerPoint = std::nullopt;
            _autoScrollVelocity = 0;
            _lastAutoScrollUpdateTime = std::nullopt;

            // Apparently this check is not necessary but greatly improves performance
            if (_autoScrollTimer.IsEnabled())
            {
                _autoScrollTimer.Stop();
            }
        }
    }

    // Method Description:
    // - Called continuously to gradually scroll viewport when user is mouse
    //   selecting outside it (to 'follow' the cursor).
    // Arguments:
    // - none
    void TermControl::_UpdateAutoScroll(Windows::Foundation::IInspectable const& /* sender */,
                                        Windows::Foundation::IInspectable const& /* e */)
    {
        if (_autoScrollVelocity != 0)
        {
            const auto timeNow = std::chrono::high_resolution_clock::now();

            if (_lastAutoScrollUpdateTime.has_value())
            {
                static constexpr double microSecPerSec = 1000000.0;
                const double deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(timeNow - _lastAutoScrollUpdateTime.value()).count() / microSecPerSec;
                ScrollBar().Value(ScrollBar().Value() + _autoScrollVelocity * deltaTime);

                if (_autoScrollingPointerPoint.has_value())
                {
                    _SetEndSelectionPointAtCursor(_autoScrollingPointerPoint.value().Position());
                }
            }

            _lastAutoScrollUpdateTime = timeNow;
        }
    }

    // Method Description:
    // - Event handler for the GotFocus event. This is used to...
    //   - enable accessibility notifications for this TermControl
    //   - start blinking the cursor when the window is focused
    //   - update the number of lines to scroll to the value set in the system
    void TermControl::_GotFocusHandler(Windows::Foundation::IInspectable const& /* sender */,
                                       RoutedEventArgs const& /* args */)
    {
        if (_closing)
        {
            return;
        }

        _focused = true;

        InputPane::GetForCurrentView().TryShow();

        // GH#5421: Enable the UiaEngine before checking for the SearchBox
        // That way, new selections are notified to automation clients.
        if (_uiaEngine.get())
        {
            THROW_IF_FAILED(_uiaEngine->Enable());
        }

        // If the searchbox is focused, we don't want TSFInputControl to think
        // it has focus so it doesn't intercept IME input. We also don't want the
        // terminal's cursor to start blinking. So, we'll just return quickly here.
        if (_searchBox && _searchBox->ContainsFocus())
        {
            return;
        }

        if (TSFInputControl() != nullptr)
        {
            TSFInputControl().NotifyFocusEnter();
        }

        if (_cursorTimer.has_value())
        {
            // When the terminal focuses, show the cursor immediately
            _core->CursorOn(true);
            _cursorTimer.value().Start();
        }

        if (_blinkTimer.has_value())
        {
            _blinkTimer.value().Start();
        }

        _interactivity->GainFocus();

        // Only update the appearance here if an unfocused config exists -
        // if an unfocused config does not exist then we never would have switched
        // appearances anyway so there's no need to switch back upon gaining focus
        if (_UnfocusedAppearance)
        {
            UpdateAppearance(_settings);
        }
    }

    // Method Description:
    // - Event handler for the LostFocus event. This is used to...
    //   - disable accessibility notifications for this TermControl
    //   - hide and stop blinking the cursor when the window loses focus.
    void TermControl::_LostFocusHandler(Windows::Foundation::IInspectable const& /* sender */,
                                        RoutedEventArgs const& /* args */)
    {
        if (_closing)
        {
            return;
        }

        _RestorePointerCursorHandlers(*this, nullptr);

        _focused = false;

        if (_uiaEngine.get())
        {
            THROW_IF_FAILED(_uiaEngine->Disable());
        }

        if (TSFInputControl() != nullptr)
        {
            TSFInputControl().NotifyFocusLeave();
        }

        if (_cursorTimer.has_value())
        {
            _cursorTimer.value().Stop();
            _core->CursorOn(false);
        }

        if (_blinkTimer.has_value())
        {
            _blinkTimer.value().Stop();
        }

        // Check if there is an unfocused config we should set the appearance to
        // upon losing focus
        if (_UnfocusedAppearance)
        {
            UpdateAppearance(_UnfocusedAppearance);
        }
    }

    // Method Description:
    // - Triggered when the swapchain changes size. We use this to resize the
    //      terminal buffers to match the new visible size.
    // Arguments:
    // - e: a SizeChangedEventArgs with the new dimensions of the SwapChainPanel
    void TermControl::_SwapChainSizeChanged(winrt::Windows::Foundation::IInspectable const& /*sender*/,
                                            SizeChangedEventArgs const& e)
    {
        if (!_initializedTerminal || _closing)
        {
            return;
        }

        const auto newSize = e.NewSize();
        _core->SizeChanged(newSize.Width, newSize.Height);
    }

    // Method Description:
    // - Triggered when the swapchain changes DPI. When this happens, we're
    //   going to receive 3 events:
    //   - 1. First, a CompositionScaleChanged _for the original scale_. I don't
    //     know why this event happens first. **It also doesn't always happen.**
    //     However, when it does happen, it doesn't give us any useful
    //     information.
    //   - 2. Then, a SizeChanged. During that SizeChanged, either:
    //      - the CompositionScale will still be the original DPI. This happens
    //        when the control is visible as the DPI changes.
    //      - The CompositionScale will be the new DPI. This happens when the
    //        control wasn't focused as the window's DPI changed, so it only got
    //        these messages after XAML updated it's scaling.
    //   - 3. Finally, a CompositionScaleChanged with the _new_ DPI.
    //   - 4. We'll usually get another SizeChanged some time after this last
    //     ScaleChanged. This usually seems to happen after something triggers
    //     the UI to re-layout, like hovering over the scrollbar. This event
    //     doesn't reliably happen immediately after a scale change, so we can't
    //     depend on it (despite the fact that both the scale and size state is
    //     definitely correct in it)
    // - In the 3rd event, we're going to update our font size for the new DPI.
    //   At that point, we know how big the font should be for the new DPI, and
    //   how big the SwapChainPanel will be. If these sizes are different, we'll
    //   need to resize the buffer to fit in the new window.
    // Arguments:
    // - sender: The SwapChainPanel who's DPI changed. This is our _swapchainPanel.
    // - args: This param is unused in the CompositionScaleChanged event.
    void TermControl::_SwapChainScaleChanged(Windows::UI::Xaml::Controls::SwapChainPanel const& sender,
                                             Windows::Foundation::IInspectable const& /*args*/)
    {
        const auto scaleX = sender.CompositionScaleX();

        _core->ScaleChanged(scaleX);
    }

    // Method Description:
    // - Toggle the cursor on and off when called by the cursor blink timer.
    // Arguments:
    // - sender: not used
    // - e: not used
    void TermControl::_CursorTimerTick(Windows::Foundation::IInspectable const& /* sender */,
                                       Windows::Foundation::IInspectable const& /* e */)
    {
        if (!_closing)
        {
            _core->BlinkCursor();
        }
    }

    // Method Description:
    // - Toggle the blinking rendition state when called by the blink timer.
    // Arguments:
    // - sender: not used
    // - e: not used
    void TermControl::_BlinkTimerTick(Windows::Foundation::IInspectable const& /* sender */,
                                      Windows::Foundation::IInspectable const& /* e */)
    {
        if (!_closing)
        {
            _core->BlinkAttributeTick();
        }
    }

    // Method Description:
    // - Sets selection's end position to match supplied cursor position, e.g. while mouse dragging.
    // Arguments:
    // - cursorPosition: in pixels, relative to the origin of the control
    void TermControl::_SetEndSelectionPointAtCursor(Windows::Foundation::Point const& cursorPosition)
    {
        _interactivity->SetEndSelectionPoint(_toTerminalOrigin(cursorPosition));
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
    void TermControl::_ScrollPositionChanged(const IInspectable& /*sender*/,
                                             const Control::ScrollPositionChangedArgs& args)
    {
        // Since this callback fires from non-UI thread, we might be already
        // closed/closing.
        if (_closing.load())
        {
            return;
        }

        ScrollBarUpdate update;
        const auto hiddenContent = args.BufferSize() - args.ViewHeight();
        update.newMaximum = hiddenContent;
        update.newMinimum = 0;
        update.newViewportSize = args.ViewHeight();
        update.newValue = args.ViewTop();

        _updateScrollBar->Run(update);
        _updatePatternLocations->Run();
    }

    // Method Description:
    // - Tells TSFInputControl to redraw the Canvas/TextBlock so it'll update
    //   to be where the current cursor position is.
    // Arguments:
    // - N/A
    void TermControl::_CursorPositionChanged(const IInspectable& /*sender*/,
                                             const IInspectable& /*args*/)
    {
        _tsfTryRedrawCanvas->Run();
    }

    hstring TermControl::Title()
    {
        return _core->Title();
    }

    hstring TermControl::GetProfileName() const
    {
        return _settings.ProfileName();
    }

    hstring TermControl::WorkingDirectory() const
    {
        return _core->WorkingDirectory();
    }

    bool TermControl::BracketedPasteEnabled() const noexcept
    {
        return _core->BracketedPasteEnabled();
    }

    // Method Description:
    // - Given a copy-able selection, get the selected text from the buffer and send it to the
    //     Windows Clipboard (CascadiaWin32:main.cpp).
    // - CopyOnSelect does NOT clear the selection
    // Arguments:
    // - singleLine: collapse all of the text to one line
    // - formats: which formats to copy (defined by action's CopyFormatting arg). nullptr
    //             if we should defer which formats are copied to the global setting
    bool TermControl::CopySelectionToClipboard(bool singleLine, const Windows::Foundation::IReference<CopyFormat>& formats)
    {
        if (_closing)
        {
            return false;
        }

        return _interactivity->CopySelectionToClipboard(singleLine, formats);
    }

    // Method Description:
    // - Initiate a paste operation.
    void TermControl::PasteTextFromClipboard()
    {
        _interactivity->RequestPasteTextFromClipboard();
    }

    void TermControl::Close()
    {
        if (!_closing.exchange(true))
        {
            _core->ReceivedOutput(_coreOutputEventToken);

            _RestorePointerCursorHandlers(*this, nullptr);

            // Disconnect the TSF input control so it doesn't receive EditContext events.
            TSFInputControl().Close();
            _autoScrollTimer.Stop();

            _core->Close();
        }
    }

    // Method Description:
    // - Scrolls the viewport of the terminal and updates the scroll bar accordingly
    // Arguments:
    // - viewTop: the viewTop to scroll to
    void TermControl::ScrollViewport(int viewTop)
    {
        ScrollBar().Value(viewTop);
    }

    int TermControl::ScrollOffset()
    {
        return _core->ScrollOffset();
    }

    // Function Description:
    // - Gets the height of the terminal in lines of text
    // Return Value:
    // - The height of the terminal in lines of text
    int TermControl::ViewHeight() const
    {
        return _core->ViewHeight();
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
    // - a size containing the requested dimensions in pixels.
    winrt::Windows::Foundation::Size TermControl::GetProposedDimensions(IControlSettings const& settings, const uint32_t dpi)
    {
        // If the settings have negative or zero row or column counts, ignore those counts.
        // (The lower TerminalCore layer also has upper bounds as well, but at this layer
        //  we may eventually impose different ones depending on how many pixels we can address.)
        const auto cols = ::base::saturated_cast<float>(std::max(settings.InitialCols(), 1));
        const auto rows = ::base::saturated_cast<float>(std::max(settings.InitialRows(), 1));

        const winrt::Windows::Foundation::Size initialSize{ cols, rows };

        return GetProposedDimensions(initialSize,
                                     settings.FontSize(),
                                     settings.FontWeight(),
                                     settings.FontFace(),
                                     settings.ScrollState(),
                                     settings.Padding(),
                                     dpi);
    }

    // Function Description:
    // - Determines how much space (in pixels) an app would need to reserve to
    //   create a control with the settings stored in the settings param. This
    //   accounts for things like the font size and face, the initialRows and
    //   initialCols, and scrollbar visibility. The returned sized is based upon
    //   the provided DPI value
    // Arguments:
    // - initialSizeInChars: The size to get the proposed dimensions for.
    // - fontHeight: The font height to use to calculate the proposed size for.
    // - fontWeight: The font weight to use to calculate the proposed size for.
    // - fontFace: The font name to use to calculate the proposed size for.
    // - scrollState: The ScrollbarState to use to calculate the proposed size for.
    // - padding: The padding to use to calculate the proposed size for.
    // - dpi: The DPI we should create the terminal at. This affects things such
    //   as font size, scrollbar and other control scaling, etc. Make sure the
    //   caller knows what monitor the control is about to appear on.
    // Return Value:
    // - a size containing the requested dimensions in pixels.
    winrt::Windows::Foundation::Size TermControl::GetProposedDimensions(const winrt::Windows::Foundation::Size& initialSizeInChars,
                                                                        const int32_t& fontHeight,
                                                                        const winrt::Windows::UI::Text::FontWeight& fontWeight,
                                                                        const winrt::hstring& fontFace,
                                                                        const ScrollbarState& scrollState,
                                                                        const winrt::hstring& padding,
                                                                        const uint32_t dpi)
    {
        const auto cols = ::base::saturated_cast<int>(initialSizeInChars.Width);
        const auto rows = ::base::saturated_cast<int>(initialSizeInChars.Height);

        // Initialize our font information.
        // The font width doesn't terribly matter, we'll only be using the
        //      height to look it up
        // The other params here also largely don't matter.
        //      The family is only used to determine if the font is truetype or
        //      not, but DX doesn't use that info at all.
        //      The Codepage is additionally not actually used by the DX engine at all.
        FontInfo actualFont = { fontFace, 0, fontWeight.Weight, { 0, gsl::narrow_cast<short>(fontHeight) }, CP_UTF8, false };
        FontInfoDesired desiredFont = { actualFont };

        // Create a DX engine and initialize it with our font and DPI. We'll
        // then use it to measure how much space the requested rows and columns
        // will take up.
        // TODO: MSFT:21254947 - use a static function to do this instead of
        // instantiating a DxEngine
        auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
        THROW_IF_FAILED(dxEngine->UpdateDpi(dpi));
        THROW_IF_FAILED(dxEngine->UpdateFont(desiredFont, actualFont));

        const auto scale = dxEngine->GetScaling();
        const auto fontSize = actualFont.GetSize();

        // UWP XAML scrollbars aren't guaranteed to be the same size as the
        // ComCtl scrollbars, but it's certainly close enough.
        const auto scrollbarSize = GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);

        double width = cols * fontSize.X;

        // Reserve additional space if scrollbar is intended to be visible
        if (scrollState == ScrollbarState::Visible)
        {
            width += scrollbarSize;
        }

        double height = rows * fontSize.Y;
        auto thickness = _ParseThicknessFromPadding(padding);
        // GH#2061 - make sure to account for the size the padding _will be_ scaled to
        width += scale * (thickness.Left + thickness.Right);
        height += scale * (thickness.Top + thickness.Bottom);

        return { gsl::narrow_cast<float>(width), gsl::narrow_cast<float>(height) };
    }

    // Method Description:
    // - Get the size of a single character of this control. The size is in
    //   DIPs. If you need it in _pixels_, you'll need to multiply by the
    //   current display scaling.
    // Arguments:
    // - <none>
    // Return Value:
    // - The dimensions of a single character of this control, in DIPs
    winrt::Windows::Foundation::Size TermControl::CharacterDimensions() const
    {
        const auto fontSize = _core->GetFont().GetSize();
        return { gsl::narrow_cast<float>(fontSize.X), gsl::narrow_cast<float>(fontSize.Y) };
    }

    // Method Description:
    // - Get the absolute minimum size that this control can be resized to and
    //   still have 1x1 character visible. This includes the space needed for
    //   the scrollbar and the padding.
    // Arguments:
    // - <none>
    // Return Value:
    // - The minimum size that this terminal control can be resized to and still
    //   have a visible character.
    winrt::Windows::Foundation::Size TermControl::MinimumSize()
    {
        if (_initializedTerminal)
        {
            const auto fontSize = _core->GetFont().GetSize();
            double width = fontSize.X;
            double height = fontSize.Y;
            // Reserve additional space if scrollbar is intended to be visible
            if (_settings.ScrollState() == ScrollbarState::Visible)
            {
                width += ScrollBar().ActualWidth();
            }

            // Account for the size of any padding
            const auto padding = GetPadding();
            width += padding.Left + padding.Right;
            height += padding.Top + padding.Bottom;

            return { gsl::narrow_cast<float>(width), gsl::narrow_cast<float>(height) };
        }
        else
        {
            // If the terminal hasn't been initialized yet, then the font size will
            // have dimensions {1, fontSize.Y}, which can mess with consumers of
            // this method. In that case, we'll need to pre-calculate the font
            // width, before we actually have a renderer or swapchain.
            const winrt::Windows::Foundation::Size minSize{ 1, 1 };
            const double scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
            const auto dpi = ::base::saturated_cast<uint32_t>(USER_DEFAULT_SCREEN_DPI * scaleFactor);
            return GetProposedDimensions(minSize,
                                         _settings.FontSize(),
                                         _settings.FontWeight(),
                                         _settings.FontFace(),
                                         _settings.ScrollState(),
                                         _settings.Padding(),
                                         dpi);
        }
    }

    // Method Description:
    // - Adjusts given dimension (width or height) so that it aligns to the character grid.
    //   The snap is always downward.
    // Arguments:
    // - widthOrHeight: if true operates on width, otherwise on height
    // - dimension: a dimension (width or height) to be snapped
    // Return Value:
    // - A dimension that would be aligned to the character grid.
    float TermControl::SnapDimensionToGrid(const bool widthOrHeight, const float dimension)
    {
        const auto fontSize = _core->GetFont().GetSize();
        const auto fontDimension = widthOrHeight ? fontSize.X : fontSize.Y;

        const auto padding = GetPadding();
        auto nonTerminalArea = gsl::narrow_cast<float>(widthOrHeight ?
                                                           padding.Left + padding.Right :
                                                           padding.Top + padding.Bottom);

        if (widthOrHeight && _settings.ScrollState() == ScrollbarState::Visible)
        {
            nonTerminalArea += gsl::narrow_cast<float>(ScrollBar().ActualWidth());
        }

        const auto gridSize = dimension - nonTerminalArea;
        const int cells = static_cast<int>(gridSize / fontDimension);
        return cells * fontDimension + nonTerminalArea;
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
        // std::stod will throw invalid_argument exception if the input is an invalid double value
        // std::stod will throw out_of_range exception if the input value is more than DBL_MAX
        try
        {
            for (; std::getline(tokenStream, token, singleCharDelim) && (paddingPropIndex < thicknessArr.size()); paddingPropIndex++)
            {
                // std::stod internally calls wcstod which handles whitespace prefix (which is ignored)
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
    // - The Microsoft::Terminal::Core::ControlKeyStates representing the modifier key states.
    ControlKeyStates TermControl::_GetPressedModifierKeys() const
    {
        const CoreWindow window = CoreWindow::GetForCurrentThread();
        // DONT USE
        //      != CoreVirtualKeyStates::None
        // OR
        //      == CoreVirtualKeyStates::Down
        // Sometimes with the key down, the state is Down | Locked.
        // Sometimes with the key up, the state is Locked.
        // IsFlagSet(Down) is the only correct solution.

        struct KeyModifier
        {
            VirtualKey vkey;
            ControlKeyStates flags;
        };

        constexpr std::array<KeyModifier, 7> modifiers{ {
            { VirtualKey::RightMenu, ControlKeyStates::RightAltPressed },
            { VirtualKey::LeftMenu, ControlKeyStates::LeftAltPressed },
            { VirtualKey::RightControl, ControlKeyStates::RightCtrlPressed },
            { VirtualKey::LeftControl, ControlKeyStates::LeftCtrlPressed },
            { VirtualKey::Shift, ControlKeyStates::ShiftPressed },
            { VirtualKey::RightWindows, ControlKeyStates::RightWinPressed },
            { VirtualKey::LeftWindows, ControlKeyStates::LeftWinPressed },
        } };

        ControlKeyStates flags;

        for (const auto& mod : modifiers)
        {
            const auto state = window.GetKeyState(mod.vkey);
            const auto isDown = WI_IsFlagSet(state, CoreVirtualKeyStates::Down);

            if (isDown)
            {
                flags |= mod.flags;
            }
        }

        return flags;
    }

    // Method Description:
    // - Gets the corresponding viewport pixel position for the cursor
    //    by excluding the padding.
    // Arguments:
    // - cursorPosition: the (x,y) position of a given cursor (i.e.: mouse cursor).
    //    NOTE: origin (0,0) is top-left.
    // Return Value:
    // - the corresponding viewport terminal position (in pixels) for the given Point parameter
    const til::point TermControl::_toTerminalOrigin(winrt::Windows::Foundation::Point cursorPosition)
    {
        // cursorPosition is DIPs, relative to SwapChainPanel origin
        const til::point cursorPosInDIPs{ til::math::rounding, cursorPosition };
        const til::size marginsInDips{ til::math::rounding, GetPadding().Left, GetPadding().Top };

        // This point is the location of the cursor within the actual grid of characters, in DIPs
        const til::point relativeToMarginInDIPs = cursorPosInDIPs - marginsInDips;

        // Convert it to pixels
        const til::point relativeToMarginInPixels{ relativeToMarginInDIPs * SwapChainPanel().CompositionScaleX() };

        return relativeToMarginInPixels;
    }

    // Method Description:
    // - Composition Completion handler for the TSFInputControl that
    //   handles writing text out to TerminalConnection
    // Arguments:
    // - text: the text to write to TerminalConnection
    // Return Value:
    // - <none>
    void TermControl::_CompositionCompleted(winrt::hstring text)
    {
        if (_closing)
        {
            return;
        }

        _core->SendInput(text);
    }

    // Method Description:
    // - CurrentCursorPosition handler for the TSFInputControl that
    //   handles returning current cursor position.
    // Arguments:
    // - eventArgs: event for storing the current cursor position
    // Return Value:
    // - <none>
    void TermControl::_CurrentCursorPositionHandler(const IInspectable& /*sender*/,
                                                    const CursorPositionEventArgs& eventArgs)
    {
        if (!_initializedTerminal)
        {
            // fake it
            eventArgs.CurrentPosition({ 0, 0 });
            return;
        }

        const til::point cursorPos = _core->CursorPosition();
        Windows::Foundation::Point p = { ::base::ClampedNumeric<float>(cursorPos.x()),
                                         ::base::ClampedNumeric<float>(cursorPos.y()) };
        eventArgs.CurrentPosition(p);
    }

    // Method Description:
    // - FontInfo handler for the TSFInputControl that
    //   handles returning current font information
    // Arguments:
    // - eventArgs: event for storing the current font information
    // Return Value:
    // - <none>
    void TermControl::_FontInfoHandler(const IInspectable& /*sender*/,
                                       const FontInfoEventArgs& eventArgs)
    {
        const auto fontInfo = _core->GetFont();
        eventArgs.FontSize(CharacterDimensions());
        eventArgs.FontFace(fontInfo.GetFaceName());
        ::winrt::Windows::UI::Text::FontWeight weight;
        weight.Weight = static_cast<uint16_t>(fontInfo.GetWeight());
        eventArgs.FontWeight(weight);
    }

    // Method Description:
    // - Calculates speed of single axis of auto scrolling. It has to allow for both
    //      fast and precise selection.
    // Arguments:
    // - cursorDistanceFromBorder: distance from viewport border to cursor, in pixels. Must be non-negative.
    // Return Value:
    // - positive speed in characters / sec
    double TermControl::_GetAutoScrollSpeed(double cursorDistanceFromBorder) const
    {
        // The numbers below just feel well, feel free to change.
        // TODO: Maybe account for space beyond border that user has available
        return std::pow(cursorDistanceFromBorder, 2.0) / 25.0 + 2.0;
    }

    // Method Description:
    // - Async handler for the "Drop" event. If a file was dropped onto our
    //   root, we'll try to get the path of the file dropped onto us, and write
    //   the full path of the file to our terminal connection. Like conhost, if
    //   the path contains a space, we'll wrap the path in quotes.
    // - Unlike conhost, if multiple files are dropped onto the terminal, we'll
    //   write all the paths to the terminal, separated by spaces.
    // Arguments:
    // - e: The DragEventArgs from the Drop event
    // Return Value:
    // - <none>
    winrt::fire_and_forget TermControl::_DragDropHandler(Windows::Foundation::IInspectable /*sender*/,
                                                         DragEventArgs e)
    {
        if (_closing)
        {
            return;
        }

        if (e.DataView().Contains(StandardDataFormats::ApplicationLink()))
        {
            try
            {
                Windows::Foundation::Uri link{ co_await e.DataView().GetApplicationLinkAsync() };
                _core->PasteText(link.AbsoluteUri());
            }
            CATCH_LOG();
        }
        else if (e.DataView().Contains(StandardDataFormats::WebLink()))
        {
            try
            {
                Windows::Foundation::Uri link{ co_await e.DataView().GetWebLinkAsync() };
                _core->PasteText(link.AbsoluteUri());
            }
            CATCH_LOG();
        }
        else if (e.DataView().Contains(StandardDataFormats::Text()))
        {
            try
            {
                auto text{ co_await e.DataView().GetTextAsync() };
                _core->PasteText(text);
            }
            CATCH_LOG();
        }
        // StorageItem must be last. Some applications put hybrid data format items
        // in a drop message and we'll eat a crash when we request them.
        // Those applications usually include Text as well, so having storage items
        // last makes sure we'll hit text before getting to them.
        else if (e.DataView().Contains(StandardDataFormats::StorageItems()))
        {
            Windows::Foundation::Collections::IVectorView<Windows::Storage::IStorageItem> items;
            try
            {
                items = co_await e.DataView().GetStorageItemsAsync();
            }
            CATCH_LOG();

            if (items.Size() > 0)
            {
                std::wstring allPaths;
                for (auto item : items)
                {
                    // Join the paths with spaces
                    if (!allPaths.empty())
                    {
                        allPaths += L" ";
                    }

                    std::wstring fullPath{ item.Path() };
                    const auto containsSpaces = std::find(fullPath.begin(),
                                                          fullPath.end(),
                                                          L' ') != fullPath.end();

                    if (containsSpaces)
                    {
                        fullPath.insert(0, L"\"");
                        fullPath += L"\"";
                    }

                    allPaths += fullPath;
                }
                _core->PasteText(winrt::hstring{ allPaths });
            }
        }
    }

    // Method Description:
    // - Handle the DragOver event. We'll signal that the drag operation we
    //   support is the "copy" operation, and we'll also customize the
    //   appearance of the drag-drop UI, by removing the preview and setting a
    //   custom caption. For more information, see
    //   https://docs.microsoft.com/en-us/windows/uwp/design/input/drag-and-drop#customize-the-ui
    // Arguments:
    // - e: The DragEventArgs from the DragOver event
    // Return Value:
    // - <none>
    void TermControl::_DragOverHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                       DragEventArgs const& e)
    {
        if (_closing)
        {
            return;
        }

        // We can only handle drag/dropping StorageItems (files) and plain Text
        // currently. If the format on the clipboard is anything else, returning
        // early here will prevent the drag/drop from doing anything.
        if (!(e.DataView().Contains(StandardDataFormats::StorageItems()) ||
              e.DataView().Contains(StandardDataFormats::Text())))
        {
            return;
        }

        // Make sure to set the AcceptedOperation, so that we can later receive the path in the Drop event
        e.AcceptedOperation(DataPackageOperation::Copy);

        // Sets custom UI text
        if (e.DataView().Contains(StandardDataFormats::StorageItems()))
        {
            e.DragUIOverride().Caption(RS_(L"DragFileCaption"));
        }
        else if (e.DataView().Contains(StandardDataFormats::Text()))
        {
            e.DragUIOverride().Caption(RS_(L"DragTextCaption"));
        }

        // Sets if the caption is visible
        e.DragUIOverride().IsCaptionVisible(true);
        // Sets if the dragged content is visible
        e.DragUIOverride().IsContentVisible(false);
        // Sets if the glyph is visible
        e.DragUIOverride().IsGlyphVisible(false);
    }

    // Method description:
    // - Checks if the uri is valid and sends an event if so
    // Arguments:
    // - The uri
    winrt::fire_and_forget TermControl::_HyperlinkHandler(IInspectable /*sender*/,
                                                          Control::OpenHyperlinkEventArgs args)
    {
        // Save things we need to resume later.
        auto strongThis{ get_strong() };

        // Pop the rest of this function to the tail of the UI thread
        // Just in case someone was holding a lock when they called us and
        // the handlers decide to do something that take another lock
        // (like ShellExecute pumping our messaging thread...GH#7994)
        co_await Dispatcher();

        _OpenHyperlinkHandlers(*strongThis, args);
    }

    // Method Description:
    // - Produces the error dialog that notifies the user that rendering cannot proceed.
    winrt::fire_and_forget TermControl::_RendererEnteredErrorState(IInspectable /*sender*/,
                                                                   IInspectable /*args*/)
    {
        auto strongThis{ get_strong() };
        co_await Dispatcher(); // pop up onto the UI thread

        if (auto loadedUiElement{ FindName(L"RendererFailedNotice") })
        {
            if (auto uiElement{ loadedUiElement.try_as<::winrt::Windows::UI::Xaml::UIElement>() })
            {
                uiElement.Visibility(Visibility::Visible);
            }
        }
    }

    // Method Description:
    // - Responds to the Click event on the button that will re-enable the renderer.
    void TermControl::_RenderRetryButton_Click(IInspectable const& /*sender*/, IInspectable const& /*args*/)
    {
        // It's already loaded if we get here, so just hide it.
        RendererFailedNotice().Visibility(Visibility::Collapsed);
        _core->ResumeRendering();
    }

    IControlSettings TermControl::Settings() const
    {
        return _settings;
    }

    void TermControl::Settings(IControlSettings newSettings)
    {
        _settings = newSettings;
    }

    Windows::Foundation::IReference<winrt::Windows::UI::Color> TermControl::TabColor() noexcept
    {
        // NOTE TO FUTURE READERS: TabColor is down in the Core for the
        // hypothetical future where we allow an application to set the tab
        // color with VT sequences like they're currently allowed to with the
        // title.
        return _core->TabColor();
    }

    // Method Description:
    // - Gets the internal taskbar state value
    // Return Value:
    // - The taskbar state of this control
    const size_t TermControl::TaskbarState() const noexcept
    {
        return _core->TaskbarState();
    }

    // Method Description:
    // - Gets the internal taskbar progress value
    // Return Value:
    // - The taskbar progress of this control
    const size_t TermControl::TaskbarProgress() const noexcept
    {
        return _core->TaskbarProgress();
    }

    // Method Description:
    // - Checks whether the control is in a read-only mode (in this mode node input is sent to connection).
    // Return Value:
    // - True if the mode is read-only
    bool TermControl::ReadOnly() const noexcept
    {
        return _core->IsInReadOnlyMode();
    }

    // Method Description:
    // - Toggles the read-only flag, raises event describing the value change
    void TermControl::ToggleReadOnly()
    {
        _core->ToggleReadOnlyMode();
        _ReadOnlyChangedHandlers(*this, winrt::box_value(_core->IsInReadOnlyMode()));
    }

    // Method Description:
    // - Handle a mouse exited event, specifically clearing last hovered cell
    // and removing selection from hyper link if exists
    // Arguments:
    // - sender: not used
    // - args: event data
    void TermControl::_PointerExitedHandler(Windows::Foundation::IInspectable const& /*sender*/, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& /*e*/)
    {
        _core->UpdateHoveredCell(std::nullopt);
    }

    winrt::fire_and_forget TermControl::_hoveredHyperlinkChanged(IInspectable sender,
                                                                 IInspectable args)
    {
        auto weakThis{ get_weak() };
        co_await resume_foreground(Dispatcher());
        if (auto self{ weakThis.get() })
        {
            auto lastHoveredCell = _core->GetHoveredCell();
            if (lastHoveredCell.has_value())
            {
                const auto uriText = _core->GetHoveredUriText();
                if (!uriText.empty())
                {
                    // Update the tooltip with the URI
                    HoveredUri().Text(uriText);

                    // Set the border thickness so it covers the entire cell
                    const auto charSizeInPixels = CharacterDimensions();
                    const auto htInDips = charSizeInPixels.Height / SwapChainPanel().CompositionScaleY();
                    const auto wtInDips = charSizeInPixels.Width / SwapChainPanel().CompositionScaleX();
                    const Thickness newThickness{ wtInDips, htInDips, 0, 0 };
                    HyperlinkTooltipBorder().BorderThickness(newThickness);

                    // Compute the location of the top left corner of the cell in DIPS
                    const til::size marginsInDips{ til::math::rounding, GetPadding().Left, GetPadding().Top };
                    const til::point startPos{ *lastHoveredCell };
                    const til::size fontSize{ _core->GetFont().GetSize() };
                    const til::point posInPixels{ startPos * fontSize };
                    const til::point posInDIPs{ posInPixels / SwapChainPanel().CompositionScaleX() };
                    const til::point locationInDIPs{ posInDIPs + marginsInDips };

                    // Move the border to the top left corner of the cell
                    OverlayCanvas().SetLeft(HyperlinkTooltipBorder(),
                                            (locationInDIPs.x() - SwapChainPanel().ActualOffset().x));
                    OverlayCanvas().SetTop(HyperlinkTooltipBorder(),
                                           (locationInDIPs.y() - SwapChainPanel().ActualOffset().y));
                }
            }
        }
    }

    void TermControl::_coreFontSizeChanged(const int fontWidth,
                                           const int fontHeight,
                                           const bool isInitialChange)
    {
        // Don't try to inspect the core here. The Core is raising this while
        // it's holding its write lock. If the handlers calls back to some
        // method on the TermControl on the same thread, and that _method_ calls
        // to ControlCore, we might be in danger of deadlocking.
        _FontSizeChangedHandlers(fontWidth, fontHeight, isInitialChange);
    }

    void TermControl::_coreRaisedNotice(const IInspectable& /*sender*/,
                                        const Control::NoticeEventArgs& eventArgs)
    {
        // Don't try to inspect the core here. The Core might be raising this
        // while it's holding its write lock. If the handlers calls back to some
        // method on the TermControl on the same thread, and _that_ method calls
        // to ControlCore, we might be in danger of deadlocking.
        _RaiseNoticeHandlers(*this, eventArgs);
    }

    TerminalInput::MouseButtonState TermControl::GetPressedMouseButtons(const winrt::Windows::UI::Input::PointerPoint point)
    {
        return TerminalInput::MouseButtonState{ point.Properties().IsLeftButtonPressed(),
                                                point.Properties().IsMiddleButtonPressed(),
                                                point.Properties().IsRightButtonPressed() };
    }

    unsigned int TermControl::GetPointerUpdateKind(const winrt::Windows::UI::Input::PointerPoint point)
    {
        const auto props = point.Properties();

        // Which mouse button changed state (and how)
        unsigned int uiButton{};
        switch (props.PointerUpdateKind())
        {
        case winrt::Windows::UI::Input::PointerUpdateKind::LeftButtonPressed:
            uiButton = WM_LBUTTONDOWN;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::LeftButtonReleased:
            uiButton = WM_LBUTTONUP;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::MiddleButtonPressed:
            uiButton = WM_MBUTTONDOWN;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::MiddleButtonReleased:
            uiButton = WM_MBUTTONUP;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::RightButtonPressed:
            uiButton = WM_RBUTTONDOWN;
            break;
        case winrt::Windows::UI::Input::PointerUpdateKind::RightButtonReleased:
            uiButton = WM_RBUTTONUP;
            break;
        default:
            uiButton = WM_MOUSEMOVE;
        }

        return uiButton;
    }

    void TermControl::_coreWarningBell(const IInspectable& /*sender*/, const IInspectable& /*args*/)
    {
        _playWarningBell->Run();
    }
}
