// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "TermControl.h"

#include <unicode.hpp>
#include <Utf16Parser.hpp>
#include <LibraryResources.h>

#include "TermControlAutomationPeer.h"
#include "../../types/inc/GlyphWidth.hpp"
#include "../../renderer/atlas/AtlasEngine.h"

#include "TermControl.g.cpp"

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
// This is already throttled primarily in the ControlCore, with a timeout of 100ms. We're adding another smaller one here, as the (potentially x-proc) call will come in off the UI thread
constexpr const auto TsfRedrawInterval = std::chrono::milliseconds(8);

// The minimum delay between updating the locations of regex patterns
constexpr const auto UpdatePatternLocationsInterval = std::chrono::milliseconds(500);

// The minimum delay between emitting warning bells
constexpr const auto TerminalWarningBellInterval = std::chrono::milliseconds(1000);

DEFINE_ENUM_FLAG_OPERATORS(winrt::Microsoft::Terminal::Control::CopyFormat);

DEFINE_ENUM_FLAG_OPERATORS(winrt::Microsoft::Terminal::Control::MouseButtonState);

namespace winrt::Microsoft::Terminal::Control::implementation
{
    TermControl::TermControl(IControlSettings settings,
                             Control::IControlAppearance unfocusedAppearance,
                             TerminalConnection::ITerminalConnection connection) :
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

        _interactivity = winrt::make<implementation::ControlInteractivity>(settings, unfocusedAppearance, connection);
        _core = _interactivity.Core();

        // These events might all be triggered by the connection, but that
        // should be drained and closed before we complete destruction. So these
        // are safe.
        _core.ScrollPositionChanged({ this, &TermControl::_ScrollPositionChanged });
        _core.WarningBell({ this, &TermControl::_coreWarningBell });
        _core.CursorPositionChanged({ this, &TermControl::_CursorPositionChanged });

        // This event is specifically triggered by the renderer thread, a BG thread. Use a weak ref here.
        _core.RendererEnteredErrorState({ get_weak(), &TermControl::_RendererEnteredErrorState });

        // These callbacks can only really be triggered by UI interactions. So
        // they don't need weak refs - they can't be triggered unless we're
        // alive.
        _core.BackgroundColorChanged({ this, &TermControl::_coreBackgroundColorChanged });
        _core.FontSizeChanged({ this, &TermControl::_coreFontSizeChanged });
        _core.TransparencyChanged({ this, &TermControl::_coreTransparencyChanged });
        _core.RaiseNotice({ this, &TermControl::_coreRaisedNotice });
        _core.HoveredHyperlinkChanged({ this, &TermControl::_hoveredHyperlinkChanged });
        _core.FoundMatch({ this, &TermControl::_coreFoundMatch });
        _core.UpdateSelectionMarkers({ this, &TermControl::_updateSelectionMarkers });
        _core.OpenHyperlink({ this, &TermControl::_HyperlinkHandler });
        _interactivity.OpenHyperlink({ this, &TermControl::_HyperlinkHandler });
        _interactivity.ScrollPositionChanged({ this, &TermControl::_ScrollPositionChanged });

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

        // Get our dispatcher. This will get us the same dispatcher as
        // TermControl::Dispatcher().
        auto dispatcher = winrt::Windows::System::DispatcherQueue::GetForCurrentThread();

        // These three throttled functions are triggered by terminal output and interact with the UI.
        // Since Close() is the point after which we are removed from the UI, but before the
        // destructor has run, we MUST check control->_IsClosing() before actually doing anything.
        _playWarningBell = std::make_shared<ThrottledFuncLeading>(
            dispatcher,
            TerminalWarningBellInterval,
            [weakThis = get_weak()]() {
                if (auto control{ weakThis.get() }; !control->_IsClosing())
                {
                    control->_WarningBellHandlers(*control, nullptr);
                }
            });

        _updateScrollBar = std::make_shared<ThrottledFuncTrailing<ScrollBarUpdate>>(
            dispatcher,
            ScrollBarUpdateInterval,
            [weakThis = get_weak()](const auto& update) {
                if (auto control{ weakThis.get() }; !control->_IsClosing())
                {
                    control->_throttledUpdateScrollbar(update);
                }
            });

        static constexpr auto AutoScrollUpdateInterval = std::chrono::microseconds(static_cast<int>(1.0 / 30.0 * 1000000));
        _autoScrollTimer.Interval(AutoScrollUpdateInterval);
        _autoScrollTimer.Tick({ this, &TermControl::_UpdateAutoScroll });

        _ApplyUISettings();
    }

    void TermControl::_throttledUpdateScrollbar(const ScrollBarUpdate& update)
    {
        // Assumptions:
        // * we're already not closing
        // * caller already checked weak ptr to make sure we're still alive

        _isInternalScrollBarUpdate = true;

        auto scrollBar = ScrollBar();
        if (update.newValue)
        {
            scrollBar.Value(*update.newValue);
        }
        scrollBar.Maximum(update.newMaximum);
        scrollBar.Minimum(update.newMinimum);
        scrollBar.ViewportSize(update.newViewportSize);
        // scroll one full screen worth at a time when the scroll bar is clicked
        scrollBar.LargeChange(std::max(update.newViewportSize - 1, 0.));

        _isInternalScrollBarUpdate = false;

        if (_showMarksInScrollbar)
        {
            // Update scrollbar marks
            ScrollBarCanvas().Children().Clear();
            const auto marks{ _core.ScrollMarks() };
            const auto fullHeight{ ScrollBarCanvas().ActualHeight() };
            const auto totalBufferRows{ update.newMaximum + update.newViewportSize };

            for (const auto m : marks)
            {
                Windows::UI::Xaml::Shapes::Rectangle r;
                Media::SolidColorBrush brush{};
                // Sneaky: technically, a mark doesn't need to have a color set,
                // it might want to just use the color from the palette for that
                // kind of mark. Fortunately, ControlCore is kind enough to
                // pre-evaluate that for us, and shove the real value into the
                // Color member, regardless if the mark has a literal value set.
                brush.Color(static_cast<til::color>(m.Color.Color));
                r.Fill(brush);
                r.Width(16.0f / 3.0f); // pip width - 1/3rd of the scrollbar width.
                r.Height(2);
                const auto markRow = m.Start.Y;
                const auto fractionalHeight = markRow / totalBufferRows;
                const auto relativePos = fractionalHeight * fullHeight;
                ScrollBarCanvas().Children().Append(r);
                Windows::UI::Xaml::Controls::Canvas::SetTop(r, relativePos);
            }
        }
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
                if (_core.HasSelection())
                {
                    // Currently we populate the search box only if a single line is selected.
                    // Empirically, multi-line selection works as well on sample scenarios,
                    // but since code paths differ, extra work is required to ensure correctness.
                    auto bufferText = _core.SelectedText(true);
                    if (bufferText.Size() == 1)
                    {
                        const auto selectedLine{ bufferText.GetAt(0) };
                        _searchBox->PopulateTextbox(selectedLine);
                    }
                }

                _searchBox->SetFocusOnTextbox();
            }
        }
    }

    void TermControl::SearchMatch(const bool goForward)
    {
        if (_IsClosing())
        {
            return;
        }
        if (!_searchBox)
        {
            CreateSearchBoxControl();
        }
        else
        {
            _core.Search(_searchBox->TextBox().Text(), goForward, false);
        }
    }

    // Method Description:
    // Find if search box text edit currently is in focus
    // Return Value:
    // - true, if search box text edit is in focus
    bool TermControl::SearchBoxEditInFocus() const
    {
        if (!_searchBox)
        {
            return false;
        }

        return _searchBox->TextBox().FocusState() == FocusState::Keyboard;
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
        _core.Search(text, goForward, caseSensitive);
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
                                             const RoutedEventArgs& /*args*/)
    {
        _searchBox->Visibility(Visibility::Collapsed);

        // Set focus back to terminal control
        this->Focus(FocusState::Programmatic);
    }

    winrt::fire_and_forget TermControl::UpdateControlSettings(IControlSettings settings)
    {
        return UpdateControlSettings(settings, _core.UnfocusedAppearance());
    }
    // Method Description:
    // - Given Settings having been updated, applies the settings to the current terminal.
    // Return Value:
    // - <none>
    winrt::fire_and_forget TermControl::UpdateControlSettings(IControlSettings settings, IControlAppearance unfocusedAppearance)
    {
        auto weakThis{ get_weak() };

        // Dispatch a call to the UI thread to apply the new settings to the
        // terminal.
        co_await wil::resume_foreground(Dispatcher());

        _core.UpdateSettings(settings, unfocusedAppearance);

        _UpdateSettingsFromUIThread();

        _UpdateAppearanceFromUIThread(_focused ? _core.FocusedAppearance() : _core.UnfocusedAppearance());
    }

    // Method Description:
    // - Dispatches a call to the UI thread and updates the appearance
    // Arguments:
    // - newAppearance: the new appearance to set
    winrt::fire_and_forget TermControl::UpdateAppearance(IControlAppearance newAppearance)
    {
        // Dispatch a call to the UI thread
        co_await wil::resume_foreground(Dispatcher());

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
    void TermControl::_UpdateSettingsFromUIThread()
    {
        if (_IsClosing())
        {
            return;
        }

        // Update our control settings
        _ApplyUISettings();
    }

    // Method Description:
    // - Updates the appearance
    // - INVARIANT: This method must be called from the UI thread.
    // Arguments:
    // - newAppearance: the new appearance to set
    void TermControl::_UpdateAppearanceFromUIThread(Control::IControlAppearance newAppearance)
    {
        if (_IsClosing())
        {
            return;
        }

        _SetBackgroundImage(newAppearance);

        // Update our control settings
        const auto bg = newAppearance.DefaultBackground();

        // In the future, this might need to be changed to a
        // _InitializeBackgroundBrush call instead, because we may need to
        // switch from a solid color brush to an acrylic one.
        _changeBackgroundColor(bg);

        // Update selection markers
        Windows::UI::Xaml::Media::SolidColorBrush cursorColorBrush{ til::color{ newAppearance.CursorColor() } };
        SelectionStartMarker().Fill(cursorColorBrush);
        SelectionEndMarker().Fill(cursorColorBrush);

        // Set TSF Foreground
        Media::SolidColorBrush foregroundBrush{};
        if (_core.Settings().UseBackgroundImageForWindow())
        {
            foregroundBrush.Color(Windows::UI::Colors::Transparent());
        }
        else
        {
            foregroundBrush.Color(static_cast<til::color>(newAppearance.DefaultForeground()));
        }
        TSFInputControl().Foreground(foregroundBrush);

        _core.ApplyAppearance(_focused);
    }

    // Method Description:
    // - Writes the given sequence as input to the active terminal connection,
    // Arguments:
    // - wstr: the string of characters to write to the terminal connection.
    // Return Value:
    // - <none>
    void TermControl::SendInput(const winrt::hstring& wstr)
    {
        _core.SendInput(wstr);
    }
    void TermControl::ClearBuffer(Control::ClearBufferType clearType)
    {
        _core.ClearBuffer(clearType);
    }

    void TermControl::ToggleShaderEffects()
    {
        _core.ToggleShaderEffects();
    }

    // Method Description:
    // - Style our UI elements based on the values in our settings, and set up
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

        // settings might be out-of-proc in the future
        auto settings{ _core.Settings() };

        // Apply padding as swapChainPanel's margin
        const auto newMargin = ParseThicknessFromPadding(settings.Padding());
        SwapChainPanel().Margin(newMargin);

        TSFInputControl().Margin(newMargin);

        // Apply settings for scrollbar
        if (settings.ScrollState() == ScrollbarState::Hidden)
        {
            // In the scenario where the user has turned off the OS setting to automatically hide scrollbars, the
            // Terminal scrollbar would still be visible; so, we need to set the control's visibility accordingly to
            // achieve the intended effect.
            ScrollBar().IndicatorMode(Controls::Primitives::ScrollingIndicatorMode::None);
            ScrollBar().Visibility(Visibility::Collapsed);
            ScrollMarksGrid().Visibility(Visibility::Collapsed);
        }
        else // (default or Visible)
        {
            // Default behavior
            ScrollBar().IndicatorMode(Controls::Primitives::ScrollingIndicatorMode::MouseIndicator);
            ScrollBar().Visibility(Visibility::Visible);
            ScrollMarksGrid().Visibility(Visibility::Visible);
        }

        _interactivity.UpdateSettings();
        if (_automationPeer)
        {
            _automationPeer.SetControlPadding(Core::Padding{ newMargin.Left,
                                                             newMargin.Top,
                                                             newMargin.Right,
                                                             newMargin.Bottom });
        }

        _showMarksInScrollbar = settings.ShowMarks();
        // Clear out all the current marks
        ScrollBarCanvas().Children().Clear();
        // When we hot reload the settings, the core will send us a scrollbar
        // update. If we enabled scrollbar marks, then great, when we handle
        // that message, we'll redraw them.
    }

    // Method Description:
    // - Sets background image and applies its settings (stretch, opacity and alignment)
    // - Checks path validity
    // Arguments:
    // - newAppearance
    // Return Value:
    // - <none>
    void TermControl::_SetBackgroundImage(const IControlAppearance& newAppearance)
    {
        if (newAppearance.BackgroundImage().empty() || _core.Settings().UseBackgroundImageForWindow())
        {
            BackgroundImage().Source(nullptr);
            return;
        }

        Windows::Foundation::Uri imageUri{ nullptr };
        try
        {
            imageUri = Windows::Foundation::Uri{ newAppearance.BackgroundImage() };
        }
        catch (...)
        {
            LOG_CAUGHT_EXCEPTION();
            BackgroundImage().Source(nullptr);
            return;
        }

        // Check if the image brush is already pointing to the image
        // in the modified settings; if it isn't (or isn't there),
        // set a new image source for the brush
        auto imageSource = BackgroundImage().Source().try_as<Media::Imaging::BitmapImage>();

        if (imageSource == nullptr ||
            imageSource.UriSource() == nullptr ||
            !imageSource.UriSource().Equals(imageUri))
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

    // Method Description:
    // - Set up each layer's brush used to display the control's background.
    // - Respects the settings for acrylic, background image and opacity from
    //   _settings.
    //   * If acrylic is not enabled, setup a solid color background, otherwise
    //       use bgcolor as acrylic's tint
    // - Avoids image flickering and acrylic brush redraw if settings are changed
    //   but the appropriate brush is still in place.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermControl::_InitializeBackgroundBrush()
    {
        auto settings{ _core.Settings() };
        auto bgColor = til::color{ _core.FocusedAppearance().DefaultBackground() };

        auto transparentBg = settings.UseBackgroundImageForWindow();
        if (transparentBg)
        {
            bgColor = Windows::UI::Colors::Transparent();
        }
        // GH#11743: Make sure to use the Core's current UseAcrylic value, not
        // the one from the settings. The Core's runtime UseAcrylic may have
        // changed from what was in the original settings.
        if (_core.UseAcrylic() && !transparentBg)
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
            acrylic.FallbackColor(bgColor);
            acrylic.TintColor(bgColor);

            // Apply brush settings
            acrylic.TintOpacity(_core.Opacity());

            // Apply brush to control if it's not already there
            if (RootGrid().Background() != acrylic)
            {
                RootGrid().Background(acrylic);
            }
        }
        else
        {
            Media::SolidColorBrush solidColor{};
            solidColor.Opacity(_core.Opacity());
            solidColor.Color(bgColor);

            RootGrid().Background(solidColor);
        }

        BackgroundBrush(RootGrid().Background());
    }

    // Method Description:
    // - Handler for the core's BackgroundColorChanged event. Updates the color
    //   of our background brush to match.
    // - Hops over to the UI thread to do this work.
    // Arguments:
    // <unused>
    // Return Value:
    // - <none>
    winrt::fire_and_forget TermControl::_coreBackgroundColorChanged(const IInspectable& /*sender*/,
                                                                    const IInspectable& /*args*/)
    {
        auto weakThis{ get_weak() };
        co_await wil::resume_foreground(Dispatcher());
        if (auto control{ weakThis.get() })
        {
            til::color newBgColor{ _core.BackgroundColor() };
            _changeBackgroundColor(newBgColor);
        }
    }

    // Method Description:
    // - Update the color of the background brush we're using. This does _not_
    //   update the opacity, or what type of brush it is.
    // - INVARIANT: This needs to be called on the UI thread.
    // Arguments:
    // - bg: the new color to use as the background color.
    void TermControl::_changeBackgroundColor(til::color bg)
    {
        auto transparent_bg = _core.Settings().UseBackgroundImageForWindow();
        if (transparent_bg)
        {
            bg = Windows::UI::Colors::Transparent();
        }

        if (auto acrylic = RootGrid().Background().try_as<Media::AcrylicBrush>())
        {
            acrylic.FallbackColor(bg);
            acrylic.TintColor(bg);
        }
        else if (auto solidColor = RootGrid().Background().try_as<Media::SolidColorBrush>())
        {
            solidColor.Color(bg);
        }

        BackgroundBrush(RootGrid().Background());

        // Don't use the normal BackgroundBrush() Observable Property setter
        // here. (e.g. `BackgroundBrush()`). The one from the macro will
        // automatically ignore changes where the value doesn't _actually_
        // change. In our case, most of the time when changing the colors of the
        // background, the _Brush_ itself doesn't change, we simply change the
        // Color() of the brush. This results in the event not getting bubbled
        // up.
        //
        // Firing it manually makes sure it does.
        _BackgroundBrush = RootGrid().Background();
        _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"BackgroundBrush" });
    }

    // Method Description:
    // - Update the opacity of the background brush we're using. This does _not_
    //   update the color, or what type of brush it is.
    // - INVARIANT: This needs to be called on the UI thread.
    void TermControl::_changeBackgroundOpacity()
    {
        const auto opacity{ _core.Opacity() };
        const auto useAcrylic{ _core.UseAcrylic() };

        // GH#11743, #11619: If we're changing whether or not acrylic is used,
        // then just entirely reinitialize the brush. The primary way that this
        // happens is on Windows 10, where we need to enable acrylic when the
        // user asks for <100% opacity. Even when we remove this Windows 10
        // fallback, we may still need this for something like changing if
        // acrylic is enabled at runtime (GH#2531)
        if (auto acrylic = RootGrid().Background().try_as<Media::AcrylicBrush>())
        {
            if (!useAcrylic)
            {
                _InitializeBackgroundBrush();
                return;
            }
            acrylic.TintOpacity(opacity);
        }
        else if (auto solidColor = RootGrid().Background().try_as<Media::SolidColorBrush>())
        {
            if (useAcrylic)
            {
                _InitializeBackgroundBrush();
                return;
            }
            solidColor.Opacity(opacity);
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
    {
        // MSFT 33353327: We're purposefully not using _initializedTerminal to ensure we're fully initialized.
        // Doing so makes us return nullptr when XAML requests an automation peer.
        // Instead, we need to give XAML an automation peer, then fix it later.
        if (!_IsClosing())
        {
            // create a custom automation peer with this code pattern:
            // (https://docs.microsoft.com/en-us/windows/uwp/design/accessibility/custom-automation-peers)
            if (const auto& interactivityAutoPeer{ _interactivity.OnCreateAutomationPeer() })
            {
                const auto margins{ SwapChainPanel().Margin() };
                const Core::Padding padding{ margins.Left,
                                             margins.Top,
                                             margins.Right,
                                             margins.Bottom };
                _automationPeer = winrt::make<implementation::TermControlAutomationPeer>(this, padding, interactivityAutoPeer);
                return _automationPeer;
            }
        }
        return nullptr;
    }

    // This is needed for TermControlAutomationPeer. We probably could find a
    // clever way around asking the core for this.
    til::point TermControl::GetFontSize() const
    {
        return { til::math::rounding, _core.FontSize().Width, _core.FontSize().Height };
    }

    const Windows::UI::Xaml::Thickness TermControl::GetPadding()
    {
        return SwapChainPanel().Margin();
    }

    TerminalConnection::ConnectionState TermControl::ConnectionState() const
    {
        return _core.ConnectionState();
    }

    winrt::fire_and_forget TermControl::RenderEngineSwapChainChanged(IInspectable /*sender*/, IInspectable /*args*/)
    {
        // This event is only registered during terminal initialization,
        // so we don't need to check _initializedTerminal.
        // We also don't lock for things that come back from the renderer.
        auto weakThis{ get_weak() };

        co_await wil::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            const auto chainHandle = reinterpret_cast<HANDLE>(control->_core.SwapChainHandle());
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
        co_await wil::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            winrt::hstring message;
            if (HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) == hr ||
                HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND) == hr)
            {
                message = { fmt::format(std::wstring_view{ RS_(L"PixelShaderNotFound") },
                                        (_focused ? _core.FocusedAppearance() : _core.UnfocusedAppearance()).PixelShaderPath()) };
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
        _core.RendererWarning({ get_weak(), &TermControl::_RendererWarning });

        const auto coreInitialized = _core.Initialize(panelWidth,
                                                      panelHeight,
                                                      panelScaleX);
        if (!coreInitialized)
        {
            return false;
        }
        _interactivity.Initialize();

        _AttachDxgiSwapChainToXaml(reinterpret_cast<HANDLE>(_core.SwapChainHandle()));

        // Tell the DX Engine to notify us when the swap chain changes. We do
        // this after we initially set the swapchain so as to avoid unnecessary
        // callbacks (and locking problems)
        _core.SwapChainChanged({ get_weak(), &TermControl::RenderEngineSwapChainChanged });

        // !! LOAD BEARING !!
        // Make sure you enable painting _AFTER_ calling _AttachDxgiSwapChainToXaml
        //
        // If you EnablePainting first, then you almost certainly won't have any
        // problems when running in Debug. However, in Release, you'll run into
        // issues where the Renderer starts trying to paint before we've
        // actually attached the swapchain to anything, and the DxEngine is not
        // prepared to handle that.
        _core.EnablePainting();

        auto bufferHeight = _core.BufferHeight();

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
            _cursorTimer.emplace(std::move(cursorTimer));
            // As of GH#6586, don't start the cursor timer immediately, and
            // don't show the cursor initially. We'll show the cursor and start
            // the timer when the control is first focused.
            //
            // As of GH#11411, turn on the cursor if we've already been marked
            // as focused. We suspect that it's possible for the Focused event
            // to fire before the LayoutUpdated. In that case, the
            // _GotFocusHandler would mark us _focused, but find that a
            // _cursorTimer doesn't exist, and it would never turn on the
            // cursor. To mitigate, we'll initialize the cursor's 'on' state
            // with `_focused` here.
            _core.CursorOn(_focused);
        }
        else
        {
            // The user has disabled cursor blinking
            _cursorTimer = std::nullopt;
        }

        // Set up blinking attributes
        auto animationsEnabled = TRUE;
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
        _UpdateAppearanceFromUIThread(_core.FocusedAppearance());

        _initializedTerminal = true;

        // MSFT 33353327: If the AutomationPeer was created before we were done initializing,
        // make sure it's properly set up now.
        if (_automationPeer)
        {
            _automationPeer.UpdateControlBounds();
            const auto margins{ GetPadding() };
            _automationPeer.SetControlPadding(Core::Padding{ margins.Left,
                                                             margins.Top,
                                                             margins.Right,
                                                             margins.Bottom });
        }

        // Likewise, run the event handlers outside of lock (they could
        // be reentrant)
        _InitializedHandlers(*this, nullptr);
        return true;
    }

    void TermControl::_CharacterHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                        const Input::CharacterReceivedRoutedEventArgs& e)
    {
        if (_IsClosing())
        {
            return;
        }

        _HidePointerCursorHandlers(*this, nullptr);

        const auto ch = e.Character();
        const auto keyStatus = e.KeyStatus();
        const auto scanCode = gsl::narrow_cast<WORD>(keyStatus.ScanCode);
        auto modifiers = _GetPressedModifierKeys();

        if (keyStatus.IsExtendedKey)
        {
            modifiers |= ControlKeyStates::EnhancedKey;
        }

        const auto handled = _core.SendCharEvent(ch, scanCode, modifiers);
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
        if (_core.IsInReadOnlyMode())
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
        else if ((vkey == VK_F7 || vkey == VK_SPACE) && down)
        {
            // Manually generate an F7 event into the key bindings or terminal.
            //   This is required as part of GH#638.
            // Or do so for alt+space; only send to terminal when explicitly unbound
            //  That is part of #GH7125
            auto bindings{ _core.Settings().KeyBindings() };
            auto isUnbound = false;
            const KeyChord kc = {
                modifiers.IsCtrlPressed(),
                modifiers.IsAltPressed(),
                modifiers.IsShiftPressed(),
                modifiers.IsWinPressed(),
                gsl::narrow_cast<WORD>(vkey),
                0
            };

            if (bindings)
            {
                handled = bindings.TryKeyChord(kc);

                if (!handled)
                {
                    isUnbound = bindings.IsKeyChordExplicitlyUnbound(kc);
                }
            }

            const auto sendToTerminal = vkey == VK_F7 || (vkey == VK_SPACE && isUnbound);

            if (!handled && sendToTerminal)
            {
                // _TrySendKeyEvent pretends it didn't handle F7 for some unknown reason.
                (void)_TrySendKeyEvent(gsl::narrow_cast<WORD>(vkey), scanCode, modifiers, true);
                // GH#6438: Note that we're _not_ sending the key up here - that'll
                // get passed through XAML to our KeyUp handler normally.
                handled = true;
            }
        }
        return handled;
    }

    void TermControl::_KeyDownHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                      const Input::KeyRoutedEventArgs& e)
    {
        _KeyHandler(e, true);
    }

    void TermControl::_KeyUpHandler(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                    const Input::KeyRoutedEventArgs& e)
    {
        _KeyHandler(e, false);
    }

    void TermControl::_KeyHandler(const Input::KeyRoutedEventArgs& e, const bool keyDown)
    {
        // If the current focused element is a child element of searchbox,
        // we do not send this event up to terminal
        if (_searchBox && _searchBox->ContainsFocus())
        {
            return;
        }

        const auto keyStatus = e.KeyStatus();
        const auto vkey = gsl::narrow_cast<WORD>(e.OriginalKey());
        const auto scanCode = gsl::narrow_cast<WORD>(keyStatus.ScanCode);
        auto modifiers = _GetPressedModifierKeys();

        if (keyStatus.IsExtendedKey)
        {
            modifiers |= ControlKeyStates::EnhancedKey;
        }

        // GH#11076:
        // For some weird reason we sometimes receive a WM_KEYDOWN
        // message without vkey or scanCode if a user drags a tab.
        // The KeyChord constructor has a debug assertion ensuring that all KeyChord
        // either have a valid vkey/scanCode. This is important, because this prevents
        // accidental insertion of invalid KeyChords into classes like ActionMap.
        if (!vkey && !scanCode)
        {
            e.Handled(true);
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
        if (_IsClosing() || vkey == VK_LWIN || vkey == VK_RWIN)
        {
            e.Handled(true);
            return;
        }

        // Short-circuit isReadOnly check to avoid warning dialog
        if (_core.IsInReadOnlyMode())
        {
            e.Handled(!keyDown || _TryHandleKeyBinding(vkey, scanCode, modifiers));
            return;
        }

        // Alt-Numpad# input will send us a character once the user releases
        // Alt, so we should be ignoring the individual keydowns. The character
        // will be sent through the TSFInputControl. See GH#1401 for more
        // details
        if (modifiers.IsAltPressed() &&
            (vkey >= VK_NUMPAD0 && vkey <= VK_NUMPAD9))
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
        e.Handled(vkey == VK_TAB);
    }

    // Method Description:
    // - Attempt to handle this key combination as a key binding
    // Arguments:
    // - vkey: The vkey of the key pressed.
    // - scanCode: The scan code of the key pressed.
    // - modifiers: The ControlKeyStates representing the modifier key states.
    bool TermControl::_TryHandleKeyBinding(const WORD vkey, const WORD scanCode, ::Microsoft::Terminal::Core::ControlKeyStates modifiers) const
    {
        // TODO: GH#5000
        // The Core owning the keybindings is weird. That's for sure. In the
        // future, we may want to pass the keybindings into the control
        // separately, so the control can have a pointer to an in-proc
        // Keybindings object, rather than routing through the ControlCore.
        // (see GH#5000)
        auto bindings = _core.Settings().KeyBindings();
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
            scanCode,
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
    void TermControl::_ClearKeyboardState(const WORD vkey, const WORD scanCode) noexcept
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
        const auto window = CoreWindow::GetForCurrentThread();

        if (vkey == VK_ESCAPE ||
            vkey == VK_RETURN)
        {
            TSFInputControl().ClearBuffer();
        }

        // If the terminal translated the key, mark the event as handled.
        // This will prevent the system from trying to get the character out
        // of it and sending us a CharacterReceived event.
        const auto handled = vkey ?
                                 _core.TrySendKeyEvent(vkey,
                                                       scanCode,
                                                       modifiers,
                                                       keyDown) :
                                 true;

        if (vkey && keyDown && _automationPeer)
        {
            get_self<TermControlAutomationPeer>(_automationPeer)->RecordKeyEvent(vkey);
        }

        if (_cursorTimer)
        {
            // Manually show the cursor when a key is pressed. Restarting
            // the timer prevents flickering.
            _core.CursorOn(_core.SelectionMode() != SelectionInteractionMode::Mark);
            _cursorTimer->Start();
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
    void TermControl::_PointerPressedHandler(const Windows::Foundation::IInspectable& sender,
                                             const Input::PointerRoutedEventArgs& args)
    {
        if (_IsClosing())
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

        // Mark that this pointer event actually started within our bounds.
        // We'll need this later, for PointerMoved events.
        _pointerPressedInBounds = true;

        if (type == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            const auto contactRect = point.Properties().ContactRect();
            auto anchor = til::point{ til::math::rounding, contactRect.X, contactRect.Y };
            _interactivity.TouchPressed(anchor.to_core_point());
        }
        else
        {
            const auto cursorPosition = point.Position();
            _interactivity.PointerPressed(TermControl::GetPressedMouseButtons(point),
                                          TermControl::GetPointerUpdateKind(point),
                                          point.Timestamp(),
                                          ControlKeyStates{ args.KeyModifiers() },
                                          _toTerminalOrigin(cursorPosition).to_core_point());
        }

        args.Handled(true);
    }

    // Method Description:
    // - handle a mouse moved event. Specifically handling mouse drag to update selection process.
    // Arguments:
    // - sender: not used
    // - args: event data
    void TermControl::_PointerMovedHandler(const Windows::Foundation::IInspectable& /*sender*/,
                                           const Input::PointerRoutedEventArgs& args)
    {
        if (_IsClosing())
        {
            return;
        }

        _RestorePointerCursorHandlers(*this, nullptr);

        const auto ptr = args.Pointer();
        const auto point = args.GetCurrentPoint(*this);
        const auto cursorPosition = point.Position();
        const auto pixelPosition = _toTerminalOrigin(cursorPosition);
        const auto type = ptr.PointerDeviceType();

        if (!_focused && _core.Settings().FocusFollowMouse())
        {
            _FocusFollowMouseRequestedHandlers(*this, nullptr);
        }

        if (type == Windows::Devices::Input::PointerDeviceType::Mouse ||
            type == Windows::Devices::Input::PointerDeviceType::Pen)
        {
            _interactivity.PointerMoved(TermControl::GetPressedMouseButtons(point),
                                        TermControl::GetPointerUpdateKind(point),
                                        ControlKeyStates(args.KeyModifiers()),
                                        _focused,
                                        pixelPosition.to_core_point(),
                                        _pointerPressedInBounds);

            // GH#9109 - Only start an auto-scroll when the drag actually
            // started within our bounds. Otherwise, someone could start a drag
            // outside the terminal control, drag into the padding, and trick us
            // into starting to scroll.
            if (_focused && _pointerPressedInBounds && point.Properties().IsLeftButtonPressed())
            {
                // We want to find the distance relative to the bounds of the
                // SwapChainPanel, not the entire control. If they drag out of
                // the bounds of the text, into the padding, we still what that
                // to auto-scroll
                const auto cursorBelowBottomDist = cursorPosition.Y - SwapChainPanel().Margin().Top - SwapChainPanel().ActualHeight();
                const auto cursorAboveTopDist = -1 * cursorPosition.Y + SwapChainPanel().Margin().Top;

                constexpr auto MinAutoScrollDist = 2.0; // Arbitrary value
                auto newAutoScrollVelocity = 0.0;
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

            _interactivity.TouchMoved(newTouchPoint.to_core_point(), _focused);
        }

        args.Handled(true);
    }

    // Method Description:
    // - Event handler for the PointerReleased event. We use this to de-anchor
    //   touch events, to stop scrolling via touch.
    // Arguments:
    // - sender: the XAML element responding to the pointer input
    // - args: event data
    void TermControl::_PointerReleasedHandler(const Windows::Foundation::IInspectable& sender,
                                              const Input::PointerRoutedEventArgs& args)
    {
        if (_IsClosing())
        {
            return;
        }

        _pointerPressedInBounds = false;

        const auto ptr = args.Pointer();
        const auto point = args.GetCurrentPoint(*this);
        const auto cursorPosition = point.Position();
        const auto pixelPosition = _toTerminalOrigin(cursorPosition);
        const auto type = ptr.PointerDeviceType();

        _ReleasePointerCapture(sender, args);

        if (type == Windows::Devices::Input::PointerDeviceType::Mouse ||
            type == Windows::Devices::Input::PointerDeviceType::Pen)
        {
            _interactivity.PointerReleased(TermControl::GetPressedMouseButtons(point),
                                           TermControl::GetPointerUpdateKind(point),
                                           ControlKeyStates(args.KeyModifiers()),
                                           pixelPosition.to_core_point());
        }
        else if (type == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            _interactivity.TouchReleased();
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
    void TermControl::_MouseWheelHandler(const Windows::Foundation::IInspectable& /*sender*/,
                                         const Input::PointerRoutedEventArgs& args)
    {
        if (_IsClosing())
        {
            return;
        }

        _RestorePointerCursorHandlers(*this, nullptr);

        const auto point = args.GetCurrentPoint(*this);
        // GH#10329 - we don't need to handle horizontal scrolls. Only vertical ones.
        // So filter out the horizontal ones.
        if (point.Properties().IsHorizontalMouseWheel())
        {
            return;
        }

        auto result = _interactivity.MouseWheel(ControlKeyStates{ args.KeyModifiers() },
                                                point.Properties().MouseWheelDelta(),
                                                _toTerminalOrigin(point.Position()).to_core_point(),
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

        Control::MouseButtonState state{};
        WI_SetFlagIf(state, Control::MouseButtonState::IsLeftButtonDown, leftButtonDown);
        WI_SetFlagIf(state, Control::MouseButtonState::IsMiddleButtonDown, midButtonDown);
        WI_SetFlagIf(state, Control::MouseButtonState::IsRightButtonDown, rightButtonDown);

        return _interactivity.MouseWheel(modifiers, delta, _toTerminalOrigin(location).to_core_point(), state);
    }

    // Method Description:
    // - Called in response to the core's TransparencyChanged event. We'll use
    //   this to update our background brush.
    // - The Core should have already updated the TintOpacity and UseAcrylic
    //   properties in the _settings->
    // Arguments:
    // - <unused>
    // Return Value:
    // - <none>
    winrt::fire_and_forget TermControl::_coreTransparencyChanged(IInspectable /*sender*/,
                                                                 Control::TransparencyChangedEventArgs /*args*/)
    {
        co_await wil::resume_foreground(Dispatcher());
        try
        {
            _changeBackgroundOpacity();
        }
        CATCH_LOG();
    }

    // Method Description:
    // - Reset the font size of the terminal to its default size.
    // Arguments:
    // - none
    void TermControl::ResetFontSize()
    {
        _core.ResetFontSize();
    }

    // Method Description:
    // - Adjust the font size of the terminal control.
    // Arguments:
    // - fontSizeDelta: The amount to increase or decrease the font size by.
    void TermControl::AdjustFontSize(int fontSizeDelta)
    {
        _core.AdjustFontSize(fontSizeDelta);
    }

    void TermControl::_ScrollbarChangeHandler(const Windows::Foundation::IInspectable& /*sender*/,
                                              const Controls::Primitives::RangeBaseValueChangedEventArgs& args)
    {
        if (_isInternalScrollBarUpdate || _IsClosing())
        {
            // The update comes from ourselves, more specifically from the
            // terminal. So we don't have to update the terminal because it
            // already knows.
            return;
        }

        const auto newValue = args.NewValue();
        _interactivity.UpdateScrollbar(newValue);

        // User input takes priority over terminal events so cancel
        // any pending scroll bar update if the user scrolls.
        _updateScrollBar->ModifyPending([](auto& update) {
            update.newValue.reset();
        });
    }

    // Method Description:
    // - captures the pointer so that none of the other XAML elements respond to pointer events
    // Arguments:
    // - sender: XAML element that is interacting with pointer
    // - args: pointer data (i.e.: mouse, touch)
    // Return Value:
    // - true if we successfully capture the pointer, false otherwise.
    bool TermControl::_CapturePointer(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& args)
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
    bool TermControl::_ReleasePointerCapture(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::Input::PointerRoutedEventArgs& args)
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
    void TermControl::_TryStartAutoScroll(const Windows::UI::Input::PointerPoint& pointerPoint, const double scrollVelocity)
    {
        // Allow only one pointer at the time
        if (!_autoScrollingPointerPoint ||
            _autoScrollingPointerPoint->PointerId() == pointerPoint.PointerId())
        {
            _autoScrollingPointerPoint = pointerPoint;
            _autoScrollVelocity = scrollVelocity;

            // If this is first time the auto scroll update is about to be called,
            //      kick-start it by initializing its time delta as if it started now
            if (!_lastAutoScrollUpdateTime)
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
        if (_autoScrollingPointerPoint &&
            pointerId == _autoScrollingPointerPoint->PointerId())
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
    void TermControl::_UpdateAutoScroll(const Windows::Foundation::IInspectable& /* sender */,
                                        const Windows::Foundation::IInspectable& /* e */)
    {
        if (_autoScrollVelocity != 0)
        {
            const auto timeNow = std::chrono::high_resolution_clock::now();

            if (_lastAutoScrollUpdateTime)
            {
                static constexpr auto microSecPerSec = 1000000.0;
                const auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(timeNow - *_lastAutoScrollUpdateTime).count() / microSecPerSec;
                ScrollBar().Value(ScrollBar().Value() + _autoScrollVelocity * deltaTime);

                if (_autoScrollingPointerPoint)
                {
                    _SetEndSelectionPointAtCursor(_autoScrollingPointerPoint->Position());
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
    void TermControl::_GotFocusHandler(const Windows::Foundation::IInspectable& /* sender */,
                                       const RoutedEventArgs& /* args */)
    {
        if (_IsClosing())
        {
            return;
        }

        _focused = true;

        InputPane::GetForCurrentView().TryShow();

        // GH#5421: Enable the UiaEngine before checking for the SearchBox
        // That way, new selections are notified to automation clients.
        // The _uiaEngine lives in _interactivity, so call into there to enable it.
        _interactivity.GotFocus();

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

        if (_cursorTimer)
        {
            // When the terminal focuses, show the cursor immediately
            _core.CursorOn(_core.SelectionMode() != SelectionInteractionMode::Mark);
            _cursorTimer->Start();
        }

        if (_blinkTimer)
        {
            _blinkTimer->Start();
        }

        // Only update the appearance here if an unfocused config exists - if an
        // unfocused config does not exist then we never would have switched
        // appearances anyway so there's no need to switch back upon gaining
        // focus
        if (_core.HasUnfocusedAppearance())
        {
            UpdateAppearance(_core.FocusedAppearance());
        }
    }

    // Method Description:
    // - Event handler for the LostFocus event. This is used to...
    //   - disable accessibility notifications for this TermControl
    //   - hide and stop blinking the cursor when the window loses focus.
    void TermControl::_LostFocusHandler(const Windows::Foundation::IInspectable& /* sender */,
                                        const RoutedEventArgs& /* args */)
    {
        if (_IsClosing())
        {
            return;
        }

        _RestorePointerCursorHandlers(*this, nullptr);

        _focused = false;

        // This will disable the accessibility notifications, because the
        // UiaEngine lives in ControlInteractivity
        _interactivity.LostFocus();

        if (TSFInputControl() != nullptr)
        {
            TSFInputControl().NotifyFocusLeave();
        }

        if (_cursorTimer)
        {
            _cursorTimer->Stop();
            _core.CursorOn(false);
        }

        if (_blinkTimer)
        {
            _blinkTimer->Stop();
        }

        // Check if there is an unfocused config we should set the appearance to
        // upon losing focus
        if (_core.HasUnfocusedAppearance())
        {
            UpdateAppearance(_core.UnfocusedAppearance());
        }
    }

    // Method Description:
    // - Triggered when the swapchain changes size. We use this to resize the
    //      terminal buffers to match the new visible size.
    // Arguments:
    // - e: a SizeChangedEventArgs with the new dimensions of the SwapChainPanel
    void TermControl::_SwapChainSizeChanged(const winrt::Windows::Foundation::IInspectable& /*sender*/,
                                            const SizeChangedEventArgs& e)
    {
        if (!_initializedTerminal || _IsClosing())
        {
            return;
        }

        const auto newSize = e.NewSize();
        _core.SizeChanged(newSize.Width, newSize.Height);

        if (_automationPeer)
        {
            _automationPeer.UpdateControlBounds();
        }
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
    void TermControl::_SwapChainScaleChanged(const Windows::UI::Xaml::Controls::SwapChainPanel& sender,
                                             const Windows::Foundation::IInspectable& /*args*/)
    {
        const auto scaleX = sender.CompositionScaleX();

        _core.ScaleChanged(scaleX);
    }

    // Method Description:
    // - Toggle the cursor on and off when called by the cursor blink timer.
    // Arguments:
    // - sender: not used
    // - e: not used
    void TermControl::_CursorTimerTick(const Windows::Foundation::IInspectable& /* sender */,
                                       const Windows::Foundation::IInspectable& /* e */)
    {
        if (!_IsClosing())
        {
            _core.BlinkCursor();
        }
    }

    // Method Description:
    // - Toggle the blinking rendition state when called by the blink timer.
    // Arguments:
    // - sender: not used
    // - e: not used
    void TermControl::_BlinkTimerTick(const Windows::Foundation::IInspectable& /* sender */,
                                      const Windows::Foundation::IInspectable& /* e */)
    {
        if (!_IsClosing())
        {
            _core.BlinkAttributeTick();
        }
    }

    // Method Description:
    // - Sets selection's end position to match supplied cursor position, e.g. while mouse dragging.
    // Arguments:
    // - cursorPosition: in pixels, relative to the origin of the control
    void TermControl::_SetEndSelectionPointAtCursor(const Windows::Foundation::Point& cursorPosition)
    {
        _interactivity.SetEndSelectionPoint(_toTerminalOrigin(cursorPosition).to_core_point());
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
        ScrollBarUpdate update;
        const auto hiddenContent = args.BufferSize() - args.ViewHeight();
        update.newMaximum = hiddenContent;
        update.newMinimum = 0;
        update.newViewportSize = args.ViewHeight();
        update.newValue = args.ViewTop();

        _updateScrollBar->Run(update);

        // if a selection marker is already visible,
        // update the position of those markers
        if (SelectionStartMarker().Visibility() == Visibility::Visible || SelectionEndMarker().Visibility() == Visibility::Visible)
        {
            _updateSelectionMarkers(nullptr, winrt::make<UpdateSelectionMarkersEventArgs>(false));
        }
    }

    // Method Description:
    // - Tells TSFInputControl to redraw the Canvas/TextBlock so it'll update
    //   to be where the current cursor position is.
    // Arguments:
    // - N/A
    winrt::fire_and_forget TermControl::_CursorPositionChanged(const IInspectable& /*sender*/,
                                                               const IInspectable& /*args*/)
    {
        // Prior to GH#10187, this fired a trailing throttled func to update the
        // TSF canvas only every 100ms. Now, the throttling occurs on the
        // ControlCore side. If we're told to update the cursor position, we can
        // just go ahead and do it.
        // This can come in off the COM thread - hop back to the UI thread.
        auto weakThis{ get_weak() };
        co_await wil::resume_foreground(Dispatcher());
        if (auto control{ weakThis.get() }; !control->_IsClosing())
        {
            control->TSFInputControl().TryRedrawCanvas();
        }
    }

    hstring TermControl::Title()
    {
        return _core.Title();
    }

    hstring TermControl::GetProfileName() const
    {
        return _core.Settings().ProfileName();
    }

    hstring TermControl::WorkingDirectory() const
    {
        return _core.WorkingDirectory();
    }

    bool TermControl::BracketedPasteEnabled() const noexcept
    {
        return _core.BracketedPasteEnabled();
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
        if (_IsClosing())
        {
            return false;
        }

        const auto successfulCopy = _interactivity.CopySelectionToClipboard(singleLine, formats);
        _core.ClearSelection();
        return successfulCopy;
    }

    // Method Description:
    // - Initiate a paste operation.
    void TermControl::PasteTextFromClipboard()
    {
        _interactivity.RequestPasteTextFromClipboard();
    }

    void TermControl::SelectAll()
    {
        _core.SelectAll();
    }

    bool TermControl::ToggleBlockSelection()
    {
        return _core.ToggleBlockSelection();
    }

    void TermControl::ToggleMarkMode()
    {
        _core.ToggleMarkMode();
    }

    bool TermControl::SwitchSelectionEndpoint()
    {
        return _core.SwitchSelectionEndpoint();
    }

    void TermControl::Close()
    {
        if (!_IsClosing())
        {
            _closing = true;

            _RestorePointerCursorHandlers(*this, nullptr);

            // Disconnect the TSF input control so it doesn't receive EditContext events.
            TSFInputControl().Close();
            _autoScrollTimer.Stop();

            _core.Close();
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

    int TermControl::ScrollOffset() const
    {
        return _core.ScrollOffset();
    }

    // Function Description:
    // - Gets the height of the terminal in lines of text
    // Return Value:
    // - The height of the terminal in lines of text
    int TermControl::ViewHeight() const
    {
        return _core.ViewHeight();
    }

    int TermControl::BufferHeight() const
    {
        return _core.BufferHeight();
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
    winrt::Windows::Foundation::Size TermControl::GetProposedDimensions(const IControlSettings& settings, const uint32_t dpi)
    {
        // If the settings have negative or zero row or column counts, ignore those counts.
        // (The lower TerminalCore layer also has upper bounds as well, but at this layer
        //  we may eventually impose different ones depending on how many pixels we can address.)
        const auto cols = ::base::saturated_cast<float>(std::max(settings.InitialCols(), 1));
        const auto rows = ::base::saturated_cast<float>(std::max(settings.InitialRows(), 1));

        const winrt::Windows::Foundation::Size initialSize{ cols, rows };

        return GetProposedDimensions(settings, dpi, initialSize);
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
    winrt::Windows::Foundation::Size TermControl::GetProposedDimensions(const IControlSettings& settings, const uint32_t dpi, const winrt::Windows::Foundation::Size& initialSizeInChars)
    {
        const auto cols = ::base::saturated_cast<int>(initialSizeInChars.Width);
        const auto rows = ::base::saturated_cast<int>(initialSizeInChars.Height);
        const auto fontSize = settings.FontSize();
        const auto fontWeight = settings.FontWeight();
        const auto fontFace = settings.FontFace();
        const auto scrollState = settings.ScrollState();
        const auto padding = settings.Padding();

        // Initialize our font information.
        // The font width doesn't terribly matter, we'll only be using the
        //      height to look it up
        // The other params here also largely don't matter.
        //      The family is only used to determine if the font is truetype or
        //      not, but DX doesn't use that info at all.
        //      The Codepage is additionally not actually used by the DX engine at all.
        FontInfo actualFont = { fontFace, 0, fontWeight.Weight, { 0, fontSize }, CP_UTF8, false };
        FontInfoDesired desiredFont = { actualFont };

        // Create a DX engine and initialize it with our font and DPI. We'll
        // then use it to measure how much space the requested rows and columns
        // will take up.
        // TODO: MSFT:21254947 - use a static function to do this instead of
        // instantiating a DxEngine/AtlasEngine.
        // GH#10211 - UNDER NO CIRCUMSTANCE should this fail. If it does, the
        // whole app will crash instantaneously on launch, which is no good.
        double scale;
        if (Feature_AtlasEngine::IsEnabled() && settings.UseAtlasEngine())
        {
            auto engine = std::make_unique<::Microsoft::Console::Render::AtlasEngine>();
            LOG_IF_FAILED(engine->UpdateDpi(dpi));
            LOG_IF_FAILED(engine->UpdateFont(desiredFont, actualFont));
            scale = engine->GetScaling();
        }
        else
        {
            auto engine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
            LOG_IF_FAILED(engine->UpdateDpi(dpi));
            LOG_IF_FAILED(engine->UpdateFont(desiredFont, actualFont));
            scale = engine->GetScaling();
        }

        const auto actualFontSize = actualFont.GetSize();

        // UWP XAML scrollbars aren't guaranteed to be the same size as the
        // ComCtl scrollbars, but it's certainly close enough.
        const auto scrollbarSize = GetSystemMetricsForDpi(SM_CXVSCROLL, dpi);

        double width = cols * actualFontSize.X;

        // Reserve additional space if scrollbar is intended to be visible
        if (scrollState == ScrollbarState::Visible)
        {
            width += scrollbarSize;
        }

        double height = rows * actualFontSize.Y;
        const auto thickness = ParseThicknessFromPadding(padding);
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
        return _core.FontSize();
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
            const auto fontSize = _core.FontSize();
            double width = fontSize.Width;
            double height = fontSize.Height;
            // Reserve additional space if scrollbar is intended to be visible
            if (_core.Settings().ScrollState() == ScrollbarState::Visible)
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
            const auto scaleFactor = DisplayInformation::GetForCurrentView().RawPixelsPerViewPixel();
            const auto dpi = ::base::saturated_cast<uint32_t>(USER_DEFAULT_SCREEN_DPI * scaleFactor);
            return GetProposedDimensions(_core.Settings(), dpi, minSize);
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
        const auto fontSize = _core.FontSize();
        const auto fontDimension = widthOrHeight ? fontSize.Width : fontSize.Height;

        const auto padding = GetPadding();
        auto nonTerminalArea = gsl::narrow_cast<float>(widthOrHeight ?
                                                           padding.Left + padding.Right :
                                                           padding.Top + padding.Bottom);

        if (widthOrHeight && _core.Settings().ScrollState() == ScrollbarState::Visible)
        {
            nonTerminalArea += gsl::narrow_cast<float>(ScrollBar().ActualWidth());
        }

        const auto gridSize = dimension - nonTerminalArea;
        const auto cells = static_cast<int>(gridSize / fontDimension);
        return cells * fontDimension + nonTerminalArea;
    }

    // Method Description:
    // - Forwards window visibility changing event down into the control core
    //   to eventually let the hosting PTY know whether the window is visible or
    //   not (which can be relevant to `::GetConsoleWindow()` calls.)
    // Arguments:
    // - showOrHide: Show is true; hide is false.
    // Return Value:
    // - <none>
    void TermControl::WindowVisibilityChanged(const bool showOrHide)
    {
        _core.WindowVisibilityChanged(showOrHide);
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
    Windows::UI::Xaml::Thickness TermControl::ParseThicknessFromPadding(const hstring padding)
    {
        const auto singleCharDelim = L',';
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
    ControlKeyStates TermControl::_GetPressedModifierKeys() noexcept
    {
        const auto window = CoreWindow::GetForCurrentThread();
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

        constexpr std::array<KeyModifier, 3> modalities{ {
            { VirtualKey::CapitalLock, ControlKeyStates::CapslockOn },
            { VirtualKey::NumberKeyLock, ControlKeyStates::NumlockOn },
            { VirtualKey::Scroll, ControlKeyStates::ScrolllockOn },
        } };

        for (const auto& mod : modalities)
        {
            const auto state = window.GetKeyState(mod.vkey);
            const auto isLocked = WI_IsFlagSet(state, CoreVirtualKeyStates::Locked);

            if (isLocked)
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
        const auto relativeToMarginInDIPs = cursorPosInDIPs - marginsInDips;

        // Convert it to pixels
        const auto scale = SwapChainPanel().CompositionScaleX();
        const til::point relativeToMarginInPixels{
            til::math::flooring,
            relativeToMarginInDIPs.x * scale,
            relativeToMarginInDIPs.y * scale,
        };

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
        if (_IsClosing())
        {
            return;
        }

        _core.SendInput(text);
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

        const auto cursorPos = _core.CursorPosition();
        eventArgs.CurrentPosition({ static_cast<float>(cursorPos.X), static_cast<float>(cursorPos.Y) });
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
        eventArgs.FontSize(CharacterDimensions());
        eventArgs.FontFace(_core.FontFaceName());
        ::winrt::Windows::UI::Text::FontWeight weight;
        weight.Weight = _core.FontWeight();
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
        if (_IsClosing())
        {
            co_return;
        }

        if (e.DataView().Contains(StandardDataFormats::ApplicationLink()))
        {
            try
            {
                auto link{ co_await e.DataView().GetApplicationLinkAsync() };
                _core.PasteText(link.AbsoluteUri());
            }
            CATCH_LOG();
        }
        else if (e.DataView().Contains(StandardDataFormats::WebLink()))
        {
            try
            {
                auto link{ co_await e.DataView().GetWebLinkAsync() };
                _core.PasteText(link.AbsoluteUri());
            }
            CATCH_LOG();
        }
        else if (e.DataView().Contains(StandardDataFormats::Text()))
        {
            try
            {
                auto text{ co_await e.DataView().GetTextAsync() };
                _core.PasteText(text);
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

                    // Fix path for WSL
                    // In the fullness of time, we should likely plumb this up
                    // to the TerminalApp layer, and have it make the decision
                    // if this control should have it's path mangled (and do the
                    // mangling), rather than exposing the source concept to the
                    // Control layer.
                    //
                    // However, it's likely that the control layer may need to
                    // know about the source anyways in the future, to support
                    // GH#3158
                    if (_interactivity.ManglePathsForWsl())
                    {
                        std::replace(fullPath.begin(), fullPath.end(), L'\\', L'/');

                        if (fullPath.size() >= 2 && fullPath.at(1) == L':')
                        {
                            // C:/foo/bar -> Cc/foo/bar
                            fullPath.at(1) = til::tolower_ascii(fullPath.at(0));
                            // Cc/foo/bar -> /mnt/c/foo/bar
                            fullPath.replace(0, 1, L"/mnt/");
                        }
                        else
                        {
                            static constexpr std::wstring_view wslPathPrefixes[] = { L"//wsl.localhost/", L"//wsl$/" };
                            for (auto prefix : wslPathPrefixes)
                            {
                                if (til::starts_with(fullPath, prefix))
                                {
                                    if (const auto idx = fullPath.find(L'/', prefix.size()); idx != std::wstring::npos)
                                    {
                                        // //wsl.localhost/Ubuntu-18.04/foo/bar -> /foo/bar
                                        fullPath.erase(0, idx);
                                    }
                                    else
                                    {
                                        // //wsl.localhost/Ubuntu-18.04 -> /
                                        fullPath = L"/";
                                    }
                                    break;
                                }
                            }
                        }
                    }

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

                _core.PasteText(winrt::hstring{ allPaths });
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
    void TermControl::_DragOverHandler(const Windows::Foundation::IInspectable& /*sender*/,
                                       const DragEventArgs& e)
    {
        if (_IsClosing())
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
    void TermControl::_RenderRetryButton_Click(const IInspectable& /*sender*/, const IInspectable& /*args*/)
    {
        // It's already loaded if we get here, so just hide it.
        RendererFailedNotice().Visibility(Visibility::Collapsed);
        _core.ResumeRendering();
    }

    IControlSettings TermControl::Settings() const
    {
        // TODO: GH#5000
        // We still need this in a couple places:
        // - Pane.cpp uses this for parsing out the StartingTitle, Commandline,
        //   etc for Pane::GetTerminalArgsForPane.
        // - TerminalTab::_CreateToolTipTitle uses the ProfileName for the
        //   tooltip for the tab.
        //
        // These both happen on the UI thread right now. In the future, when we
        // have to hop across the process boundary to get at the core settings,
        // it may make sense to cache these values inside the TermControl
        // itself, so it can do the hop once when it's first setup, rather than
        // when it's needed by the UI thread.
        return _core.Settings();
    }

    Windows::Foundation::IReference<winrt::Windows::UI::Color> TermControl::TabColor() noexcept
    {
        // NOTE TO FUTURE READERS: TabColor is down in the Core for the
        // hypothetical future where we allow an application to set the tab
        // color with VT sequences like they're currently allowed to with the
        // title.
        return _core.TabColor();
    }

    // Method Description:
    // - Gets the internal taskbar state value
    // Return Value:
    // - The taskbar state of this control
    const uint64_t TermControl::TaskbarState() const noexcept
    {
        return _core.TaskbarState();
    }

    // Method Description:
    // - Gets the internal taskbar progress value
    // Return Value:
    // - The taskbar progress of this control
    const uint64_t TermControl::TaskbarProgress() const noexcept
    {
        return _core.TaskbarProgress();
    }

    void TermControl::BellLightOn()
    {
        // Initialize the animation if it does not exist
        // We only initialize here instead of in the ctor because depending on the bell style setting,
        // we may never need this animation
        if (!_bellLightAnimation)
        {
            _bellLightAnimation = Window::Current().Compositor().CreateScalarKeyFrameAnimation();
            // Add key frames and a duration to our bell light animation
            _bellLightAnimation.InsertKeyFrame(0.0, 2.0);
            _bellLightAnimation.InsertKeyFrame(1.0, 1.0);
            _bellLightAnimation.Duration(winrt::Windows::Foundation::TimeSpan(std::chrono::milliseconds(TerminalWarningBellInterval)));
        }

        // Similar to the animation, only initialize the timer here
        if (!_bellLightTimer)
        {
            _bellLightTimer = {};
            _bellLightTimer.Interval(std::chrono::milliseconds(TerminalWarningBellInterval));
            _bellLightTimer.Tick({ get_weak(), &TermControl::_BellLightOff });
        }

        Windows::Foundation::Numerics::float2 zeroSize{ 0, 0 };
        // If the grid has 0 size or if the bell timer is
        // already active, do nothing
        if (RootGrid().ActualSize() != zeroSize && !_bellLightTimer.IsEnabled())
        {
            // Start the timer, when the timer ticks we switch off the light
            _bellLightTimer.Start();

            // Switch on the light and animate the intensity to fade out
            VisualBellLight::SetIsTarget(RootGrid(), true);
            BellLight().CompositionLight().StartAnimation(L"Intensity", _bellLightAnimation);
        }
    }

    void TermControl::_BellLightOff(const Windows::Foundation::IInspectable& /* sender */,
                                    const Windows::Foundation::IInspectable& /* e */)
    {
        if (_bellLightTimer)
        {
            // Stop the timer and switch off the light
            _bellLightTimer.Stop();

            if (!_IsClosing())
            {
                VisualBellLight::SetIsTarget(RootGrid(), false);
            }
        }
    }

    // Method Description:
    // - Checks whether the control is in a read-only mode (in this mode node input is sent to connection).
    // Return Value:
    // - True if the mode is read-only
    bool TermControl::ReadOnly() const noexcept
    {
        return _core.IsInReadOnlyMode();
    }

    // Method Description:
    // - Toggles the read-only flag, raises event describing the value change
    void TermControl::ToggleReadOnly()
    {
        _core.ToggleReadOnlyMode();
        _ReadOnlyChangedHandlers(*this, winrt::box_value(_core.IsInReadOnlyMode()));
    }

    // Method Description:
    // - Handle a mouse exited event, specifically clearing last hovered cell
    // and removing selection from hyper link if exists
    // Arguments:
    // - sender: not used
    // - args: event data
    void TermControl::_PointerExitedHandler(const Windows::Foundation::IInspectable& /*sender*/,
                                            const Windows::UI::Xaml::Input::PointerRoutedEventArgs& /*e*/)
    {
        _core.ClearHoveredCell();
    }

    winrt::fire_and_forget TermControl::_hoveredHyperlinkChanged(IInspectable /*sender*/,
                                                                 IInspectable /*args*/)
    {
        auto weakThis{ get_weak() };
        co_await wil::resume_foreground(Dispatcher());
        if (auto self{ weakThis.get() })
        {
            auto lastHoveredCell = _core.HoveredCell();
            if (lastHoveredCell)
            {
                const auto uriText = _core.HoveredUriText();
                if (!uriText.empty())
                {
                    const auto panel = SwapChainPanel();
                    const auto scale = panel.CompositionScaleX();
                    const auto offset = panel.ActualOffset();

                    // Update the tooltip with the URI
                    HoveredUri().Text(uriText);

                    // Set the border thickness so it covers the entire cell
                    const auto charSizeInPixels = CharacterDimensions();
                    const auto htInDips = charSizeInPixels.Height / scale;
                    const auto wtInDips = charSizeInPixels.Width / scale;
                    const Thickness newThickness{ wtInDips, htInDips, 0, 0 };
                    HyperlinkTooltipBorder().BorderThickness(newThickness);

                    // Compute the location of the top left corner of the cell in DIPS
                    const til::point locationInDIPs{ _toPosInDips(lastHoveredCell.Value()) };

                    // Move the border to the top left corner of the cell
                    OverlayCanvas().SetLeft(HyperlinkTooltipBorder(), locationInDIPs.x - offset.x);
                    OverlayCanvas().SetTop(HyperlinkTooltipBorder(), locationInDIPs.y - offset.y);
                }
            }
        }
    }

    winrt::fire_and_forget TermControl::_updateSelectionMarkers(IInspectable /*sender*/, Control::UpdateSelectionMarkersEventArgs args)
    {
        auto weakThis{ get_weak() };
        co_await resume_foreground(Dispatcher());
        if (weakThis.get() && args)
        {
            if (_core.HasSelection() && !args.ClearMarkers())
            {
                // retrieve all of the necessary selection marker data
                // from the TerminalCore layer under one lock to improve performance
                const auto markerData{ _core.SelectionInfo() };

                // lambda helper function that can be used to display a selection marker
                // - targetEnd: if true, target the "end" selection marker. Otherwise, target "start".
                auto displayMarker = [&](bool targetEnd) {
                    const auto flipMarker{ targetEnd ? markerData.EndAtRightBoundary : markerData.StartAtLeftBoundary };
                    const auto& marker{ targetEnd ? SelectionEndMarker() : SelectionStartMarker() };

                    // Ensure the marker is oriented properly
                    // (i.e. if start is at the beginning of the buffer, it should be flipped)
                    auto transform{ marker.RenderTransform().as<Windows::UI::Xaml::Media::ScaleTransform>() };
                    transform.ScaleX(std::abs(transform.ScaleX()) * (flipMarker ? -1.0 : 1.0));
                    marker.RenderTransform(transform);

                    // Compute the location of the top left corner of the cell in DIPS
                    auto terminalPos{ targetEnd ? markerData.EndPos : markerData.StartPos };
                    if (flipMarker)
                    {
                        // When we flip the marker, a negative scaling makes us be one cell-width to the left.
                        // Add one to the viewport pos' x-coord to fix that.
                        terminalPos.X += 1;
                    }
                    const til::point locationInDIPs{ _toPosInDips(terminalPos) };

                    // Move the marker to the top left corner of the cell
                    SelectionCanvas().SetLeft(marker,
                                              (locationInDIPs.x - SwapChainPanel().ActualOffset().x));
                    SelectionCanvas().SetTop(marker,
                                             (locationInDIPs.y - SwapChainPanel().ActualOffset().y));
                    marker.Visibility(Visibility::Visible);
                };

                // show/update selection markers
                // figure out which endpoint to move, get it and the relevant icon (hide the other icon)
                const auto movingEnd{ WI_IsFlagSet(markerData.Endpoint, SelectionEndpointTarget::End) };
                const auto selectionAnchor{ movingEnd ? markerData.EndPos : markerData.StartPos };
                const auto& marker{ movingEnd ? SelectionEndMarker() : SelectionStartMarker() };
                const auto& otherMarker{ movingEnd ? SelectionStartMarker() : SelectionEndMarker() };
                if (selectionAnchor.Y < 0 || selectionAnchor.Y >= _core.ViewHeight())
                {
                    // if the endpoint is outside of the viewport,
                    // just hide the markers
                    marker.Visibility(Visibility::Collapsed);
                    otherMarker.Visibility(Visibility::Collapsed);
                    co_return;
                }
                else if (WI_AreAllFlagsSet(markerData.Endpoint, SelectionEndpointTarget::Start | SelectionEndpointTarget::End))
                {
                    // display both markers
                    displayMarker(true);
                    displayMarker(false);
                }
                else
                {
                    // display one marker,
                    // but hide the other
                    displayMarker(movingEnd);
                    otherMarker.Visibility(Visibility::Collapsed);
                }
            }
            else
            {
                // hide selection markers
                SelectionStartMarker().Visibility(Visibility::Collapsed);
                SelectionEndMarker().Visibility(Visibility::Collapsed);
            }
        }
    }

    til::point TermControl::_toPosInDips(const Core::Point terminalCellPos)
    {
        const til::point terminalPos{ terminalCellPos };
        const til::size marginsInDips{ til::math::rounding, GetPadding().Left, GetPadding().Top };
        const til::size fontSize{ til::math::rounding, _core.FontSize() };
        const til::point posInPixels{ terminalPos * fontSize };
        const auto scale{ SwapChainPanel().CompositionScaleX() };
        const til::point posInDIPs{ til::math::flooring, posInPixels.x / scale, posInPixels.y / scale };
        return posInDIPs + marginsInDips;
    }

    void TermControl::_coreFontSizeChanged(const int fontWidth,
                                           const int fontHeight,
                                           const bool isInitialChange)
    {
        // scale the selection markers to be the size of a cell
        auto scaleMarker = [fontWidth, fontHeight, dpiScale{ SwapChainPanel().CompositionScaleX() }](const Windows::UI::Xaml::Shapes::Path& shape) {
            // The selection markers were designed to be 5x14 in size,
            // so use those dimensions below for the scaling
            const auto scaleX = fontWidth / 5.0 / dpiScale;
            const auto scaleY = fontHeight / 14.0 / dpiScale;

            Windows::UI::Xaml::Media::ScaleTransform transform;
            transform.ScaleX(scaleX);
            transform.ScaleY(scaleY);
            shape.RenderTransform(transform);

            // now hide the shape
            shape.Visibility(Visibility::Collapsed);
        };
        scaleMarker(SelectionStartMarker());
        scaleMarker(SelectionEndMarker());

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

    Control::MouseButtonState TermControl::GetPressedMouseButtons(const winrt::Windows::UI::Input::PointerPoint point)
    {
        Control::MouseButtonState state{};
        WI_SetFlagIf(state, Control::MouseButtonState::IsLeftButtonDown, point.Properties().IsLeftButtonPressed());
        WI_SetFlagIf(state, Control::MouseButtonState::IsMiddleButtonDown, point.Properties().IsMiddleButtonPressed());
        WI_SetFlagIf(state, Control::MouseButtonState::IsRightButtonDown, point.Properties().IsRightButtonPressed());
        return state;
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

    hstring TermControl::ReadEntireBuffer() const
    {
        return _core.ReadEntireBuffer();
    }

    Core::Scheme TermControl::ColorScheme() const noexcept
    {
        return _core.ColorScheme();
    }

    void TermControl::ColorScheme(const Core::Scheme& scheme) const noexcept
    {
        _core.ColorScheme(scheme);
    }

    void TermControl::AdjustOpacity(const double opacity, const bool relative)
    {
        _core.AdjustOpacity(opacity, relative);
    }

    // - You'd think this should just be "Opacity", but UIElement already
    //   defines an "Opacity", which we're actually not setting at all. We're
    //   not overriding or changing _that_ value. Callers that want the opacity
    //   set by the settings should call this instead.
    double TermControl::BackgroundOpacity() const
    {
        return _core.Opacity();
    }

    // Method Description:
    // - Called when the core raises a FoundMatch event. That's done in response
    //   to us starting a search query with ControlCore::Search.
    // - The args will tell us if there were or were not any results for that
    //   particular search. We'll use that to control what to announce to
    //   Narrator. When we have more elaborate search information to report, we
    //   may want to report that here. (see GH #3920)
    // Arguments:
    // - args: contains information about the results that were or were not found.
    // Return Value:
    // - <none>
    void TermControl::_coreFoundMatch(const IInspectable& /*sender*/, const Control::FoundResultsArgs& args)
    {
        if (auto automationPeer{ Automation::Peers::FrameworkElementAutomationPeer::FromElement(*this) })
        {
            automationPeer.RaiseNotificationEvent(
                Automation::Peers::AutomationNotificationKind::ActionCompleted,
                Automation::Peers::AutomationNotificationProcessing::ImportantMostRecent,
                args.FoundMatch() ? RS_(L"SearchBox_MatchesAvailable") : RS_(L"SearchBox_NoMatches"), // what to announce if results were found
                L"SearchBoxResultAnnouncement" /* unique name for this group of notifications */);
        }
    }

    void TermControl::OwningHwnd(uint64_t owner)
    {
        _core.OwningHwnd(owner);
    }

    uint64_t TermControl::OwningHwnd()
    {
        return _core.OwningHwnd();
    }

    void TermControl::AddMark(const Control::ScrollMark& mark)
    {
        _core.AddMark(mark);
    }
    void TermControl::ClearMark() { _core.ClearMark(); }
    void TermControl::ClearAllMarks() { _core.ClearAllMarks(); }
    void TermControl::ScrollToMark(const Control::ScrollToMarkDirection& direction) { _core.ScrollToMark(direction); }

    Windows::Foundation::Collections::IVector<Control::ScrollMark> TermControl::ScrollMarks() const
    {
        return _core.ScrollMarks();
    }

    void TermControl::ColorSelection(Control::SelectionColor fg, Control::SelectionColor bg, uint32_t matchMode)
    {
        _core.ColorSelection(fg, bg, matchMode);
    }
}
