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

DEFINE_ENUM_FLAG_OPERATORS(winrt::Microsoft::Terminal::Control::CopyFormat);

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

    TermControl::TermControl(IControlSettings settings, TerminalConnection::ITerminalConnection connection) :
        _connection{ connection },
        _initializedTerminal{ false },
        _settings{ settings },
        _closing{ false },
        _isInternalScrollBarUpdate{ false },
        _autoScrollVelocity{ 0 },
        _autoScrollingPointerPoint{ std::nullopt },
        _autoScrollTimer{},
        _lastAutoScrollUpdateTime{ std::nullopt },
        _desiredFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8 },
        _actualFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false },
        _touchAnchor{ std::nullopt },
        _cursorTimer{},
        _blinkTimer{},
        _lastMouseClickTimestamp{},
        _lastMouseClickPos{},
        _lastMouseClickPosNoSelection{},
        _selectionNeedsToBeCopied{ false },
        _searchBox{ nullptr }
    {
        _EnsureStaticInitialization();
        InitializeComponent();

        _terminal = std::make_unique<::Microsoft::Terminal::Core::Terminal>();

        // GH#8969: pre-seed working directory to prevent potential races
        _terminal->SetWorkingDirectory(_settings.StartingDirectory());

        auto pfnWarningBell = std::bind(&TermControl::_TerminalWarningBell, this);
        _terminal->SetWarningBellCallback(pfnWarningBell);

        auto pfnTitleChanged = std::bind(&TermControl::_TerminalTitleChanged, this, std::placeholders::_1);
        _terminal->SetTitleChangedCallback(pfnTitleChanged);

        auto pfnTabColorChanged = std::bind(&TermControl::_TerminalTabColorChanged, this, std::placeholders::_1);
        _terminal->SetTabColorChangedCallback(pfnTabColorChanged);

        auto pfnBackgroundColorChanged = std::bind(&TermControl::_BackgroundColorChanged, this, std::placeholders::_1);
        _terminal->SetBackgroundCallback(pfnBackgroundColorChanged);

        auto pfnScrollPositionChanged = std::bind(&TermControl::_TerminalScrollPositionChanged, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        _terminal->SetScrollPositionChangedCallback(pfnScrollPositionChanged);

        auto pfnTerminalCursorPositionChanged = std::bind(&TermControl::_TerminalCursorPositionChanged, this);
        _terminal->SetCursorPositionChangedCallback(pfnTerminalCursorPositionChanged);

        auto pfnCopyToClipboard = std::bind(&TermControl::_CopyToClipboard, this, std::placeholders::_1);
        _terminal->SetCopyToClipboardCallback(pfnCopyToClipboard);

        _terminal->TaskbarProgressChangedCallback([&]() { TermControl::TaskbarProgressChanged(); });

        // This event is explicitly revoked in the destructor: does not need weak_ref
        auto onReceiveOutputFn = [this](const hstring str) {
            _terminal->Write(str);
            _updatePatternLocations->Run();
        };
        _connectionOutputEventToken = _connection.TerminalOutput(onReceiveOutputFn);

        _terminal->SetWriteInputCallback([this](std::wstring& wstr) {
            _SendInputToConnection(wstr);
        });

        _terminal->UpdateSettings(settings);

        // Subscribe to the connection's disconnected event and call our connection closed handlers.
        _connectionStateChangedRevoker = _connection.StateChanged(winrt::auto_revoke, [this](auto&& /*s*/, auto&& /*v*/) {
            _ConnectionStateChangedHandlers(*this, nullptr);
        });

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
                    control->UpdatePatternLocations();
                }
            },
            UpdatePatternLocationsInterval,
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
                    scrollBar.LargeChange(std::max(update.newViewportSize - 1, 0.)); // scroll one "screenful" at a time when the scroll bar is clicked

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
                if (_terminal->IsSelectionActive())
                {
                    // Currently we populate the search box only if a single line is selected.
                    // Empirically, multi-line selection works as well on sample scenarios,
                    // but since code paths differ, extra work is required to ensure correctness.
                    const auto bufferData = _terminal->RetrieveSelectedTextFromBuffer(true);
                    if (bufferData.text.size() == 1)
                    {
                        const auto selectedLine{ til::at(bufferData.text, 0) };
                        _searchBox->PopulateTextbox(selectedLine.data());
                    }
                }

                _searchBox->SetFocusOnTextbox();
            }
        }
    }

    void TermControl::SearchMatch(const bool goForward)
    {
        if (!_searchBox)
        {
            CreateSearchBoxControl();
        }
        else
        {
            _Search(_searchBox->TextBox().Text(), goForward, false);
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
        if (text.size() == 0 || _closing)
        {
            return;
        }

        const Search::Direction direction = goForward ?
                                                Search::Direction::Forward :
                                                Search::Direction::Backward;

        const Search::Sensitivity sensitivity = caseSensitive ?
                                                    Search::Sensitivity::CaseSensitive :
                                                    Search::Sensitivity::CaseInsensitive;

        Search search(*GetUiaData(), text.c_str(), direction, sensitivity);
        auto lock = _terminal->LockForWriting();
        if (search.FindNext())
        {
            _terminal->SetBlockSelection(false);
            search.Select();
            _renderer->TriggerSelection();
        }
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

        // Take the lock before calling the helper functions to update the settings and appearance
        auto lock = _terminal->LockForWriting();

        _UpdateSettingsFromUIThreadUnderLock(_settings);

        auto appearance = _settings.try_as<IControlAppearance>();
        if (!_focused && _UnfocusedAppearance)
        {
            appearance = _UnfocusedAppearance;
        }
        _UpdateAppearanceFromUIThreadUnderLock(appearance);
    }

    // Method Description:
    // - Dispatches a call to the UI thread and updates the appearance
    // Arguments:
    // - newAppearance: the new appearance to set
    winrt::fire_and_forget TermControl::UpdateAppearance(const IControlAppearance newAppearance)
    {
        // Dispatch a call to the UI thread
        co_await winrt::resume_foreground(Dispatcher());

        // Take the lock before calling the helper function to update the appearance
        auto lock = _terminal->LockForWriting();
        _UpdateAppearanceFromUIThreadUnderLock(newAppearance);
    }

    // Method Description:
    // - Writes the given sequence as input to the active terminal connection,
    // Arguments:
    // - wstr: the string of characters to write to the terminal connection.
    // Return Value:
    // - <none>
    void TermControl::SendInput(const winrt::hstring& wstr)
    {
        _SendInputToConnection(wstr);
    }

    void TermControl::ToggleShaderEffects()
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
    // - Updates the settings of the current terminal.
    // - This method is separate from UpdateSettings because there is an apparent optimizer
    //   issue that causes one of our hstring -> wstring_view conversions to result in garbage,
    //   but only from a coroutine context. See GH#8723.
    // - INVARIANT: This method must be called from the UI thread.
    // - INVARIANT: This method can only be called if the caller has the writing lock on the terminal.
    // Arguments:
    // - newSettings: the new settings to set
    void TermControl::_UpdateSettingsFromUIThreadUnderLock(IControlSettings newSettings)
    {
        if (_closing)
        {
            return;
        }

        // Update our control settings
        _ApplyUISettings(_settings);

        // Update the terminal core with its new Core settings
        _terminal->UpdateSettings(_settings);

        if (!_initializedTerminal)
        {
            // If we haven't initialized, there's no point in continuing.
            // Initialization will handle the renderer settings.
            return;
        }

        // Update DxEngine settings under the lock
        _renderEngine->SetForceFullRepaintRendering(_settings.ForceFullRepaintRendering());
        _renderEngine->SetSoftwareRendering(_settings.SoftwareRendering());

        switch (_settings.AntialiasingMode())
        {
        case TextAntialiasingMode::Cleartype:
            _renderEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
            break;
        case TextAntialiasingMode::Aliased:
            _renderEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);
            break;
        case TextAntialiasingMode::Grayscale:
        default:
            _renderEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            break;
        }

        // Refresh our font with the renderer
        const auto actualFontOldSize = _actualFont.GetSize();
        _UpdateFont();
        const auto actualFontNewSize = _actualFont.GetSize();
        if (actualFontNewSize != actualFontOldSize)
        {
            _RefreshSizeUnderLock();
        }
    }

    // Method Description:
    // - Updates the appearance
    // - INVARIANT: This method must be called from the UI thread.
    // - INVARIANT: This method can only be called if the caller has the writing lock on the terminal.
    // Arguments:
    // - newAppearance: the new appearance to set
    void TermControl::_UpdateAppearanceFromUIThreadUnderLock(IControlAppearance newAppearance)
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
        _BackgroundColorChanged(bg);

        // Set TSF Foreground
        Media::SolidColorBrush foregroundBrush{};
        foregroundBrush.Color(static_cast<til::color>(newAppearance.DefaultForeground()));
        TSFInputControl().Foreground(foregroundBrush);

        // Update the terminal core with its new Core settings
        _terminal->UpdateAppearance(newAppearance);

        // Update DxEngine settings under the lock
        _renderEngine->SetSelectionBackground(til::color{ newAppearance.SelectionBackground() });
        _renderEngine->SetRetroTerminalEffect(newAppearance.RetroTerminalEffect());
        _renderEngine->SetPixelShaderPath(newAppearance.PixelShaderPath());
        _renderer->TriggerRedrawAll();
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

        // Apply padding as swapChainPanel's margin
        auto newMargin = _ParseThicknessFromPadding(newSettings.Padding());
        SwapChainPanel().Margin(newMargin);

        // Initialize our font information.
        const auto fontFace = newSettings.FontFace();
        const short fontHeight = gsl::narrow_cast<short>(newSettings.FontSize());
        const auto fontWeight = newSettings.FontWeight();
        // The font width doesn't terribly matter, we'll only be using the
        //      height to look it up
        // The other params here also largely don't matter.
        //      The family is only used to determine if the font is truetype or
        //      not, but DX doesn't use that info at all.
        //      The Codepage is additionally not actually used by the DX engine at all.
        _actualFont = { fontFace, 0, fontWeight.Weight, { 0, fontHeight }, CP_UTF8, false };
        _desiredFont = { _actualFont };

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

        _UpdateSystemParameterSettings();
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
            if (_renderEngine)
            {
                _renderEngine->SetDefaultTextBackgroundOpacity(::base::saturated_cast<float>(_settings.TintOpacity()));
            }
        }
        else
        {
            Media::SolidColorBrush solidColor{};
            RootGrid().Background(solidColor);

            // GH#5098: Inform the engine of the new opacity of the default text background.
            if (_renderEngine)
            {
                _renderEngine->SetDefaultTextBackgroundOpacity(1.0f);
            }
        }
    }

    // Method Description:
    // - Style the background of the control with the provided background color
    // Arguments:
    // - color: The background color to use as a uint32 (aka DWORD COLORREF)
    // Return Value:
    // - <none>
    winrt::fire_and_forget TermControl::_BackgroundColorChanged(const til::color color)
    {
        til::color newBgColor{ color };

        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            if (auto acrylic = RootGrid().Background().try_as<Media::AcrylicBrush>())
            {
                acrylic.FallbackColor(newBgColor);
                acrylic.TintColor(newBgColor);
            }
            else if (auto solidColor = RootGrid().Background().try_as<Media::SolidColorBrush>())
            {
                solidColor.Color(newBgColor);
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
            auto autoPeer = winrt::make_self<winrt::Microsoft::Terminal::Control::implementation::TermControlAutomationPeer>(this);

            _uiaEngine = std::make_unique<::Microsoft::Console::Render::UiaEngine>(autoPeer.get());
            _renderer->AddRenderEngine(_uiaEngine.get());
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
        return _terminal.get();
    }

    const FontInfo TermControl::GetActualFont() const
    {
        return _actualFont;
    }

    const Windows::UI::Xaml::Thickness TermControl::GetPadding()
    {
        return SwapChainPanel().Margin();
    }

    TerminalConnection::ConnectionState TermControl::ConnectionState() const
    {
        return _connection.State();
    }

    winrt::fire_and_forget TermControl::RenderEngineSwapChainChanged()
    {
        // This event is only registered during terminal initialization,
        // so we don't need to check _initializedTerminal.
        // We also don't lock for things that come back from the renderer.
        auto chain = _renderEngine->GetSwapChain();
        auto weakThis{ get_weak() };

        co_await winrt::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            _AttachDxgiSwapChainToXaml(chain.Get());
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
    winrt::fire_and_forget TermControl::_RendererWarning(const HRESULT hr)
    {
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

    void TermControl::_AttachDxgiSwapChainToXaml(IDXGISwapChain1* swapChain)
    {
        auto nativePanel = SwapChainPanel().as<ISwapChainPanelNative>();
        nativePanel->SetSwapChain(swapChain);
    }

    bool TermControl::_InitializeTerminal()
    {
        { // scope for terminalLock
            auto terminalLock = _terminal->LockForWriting();

            if (_initializedTerminal)
            {
                return false;
            }

            const auto actualWidth = SwapChainPanel().ActualWidth();
            const auto actualHeight = SwapChainPanel().ActualHeight();

            const auto windowWidth = actualWidth * SwapChainPanel().CompositionScaleX(); // Width() and Height() are NaN?
            const auto windowHeight = actualHeight * SwapChainPanel().CompositionScaleY();

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
                    strongThis->_RendererEnteredErrorState();
                }
            });

            THROW_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

            // Set up the DX Engine
            auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
            _renderer->AddRenderEngine(dxEngine.get());

            // Initialize our font with the renderer
            // We don't have to care about DPI. We'll get a change message immediately if it's not 96
            // and react accordingly.
            _UpdateFont(true);

            const COORD windowSize{ static_cast<short>(windowWidth), static_cast<short>(windowHeight) };

            // Fist set up the dx engine with the window size in pixels.
            // Then, using the font, get the number of characters that can fit.
            // Resize our terminal connection to match that size, and initialize the terminal with that size.
            const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
            LOG_IF_FAILED(dxEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

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
            dxEngine->SetWarningCallback(std::bind(&TermControl::_RendererWarning, this, std::placeholders::_1));

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

            _AttachDxgiSwapChainToXaml(_renderEngine->GetSwapChain().Get());

            // Tell the DX Engine to notify us when the swap chain changes.
            // We do this after we initially set the swapchain so as to avoid unnecessary callbacks (and locking problems)
            _renderEngine->SetCallback(std::bind(&TermControl::RenderEngineSwapChainChanged, this));

            auto bottom = _terminal->GetViewport().BottomExclusive();
            auto bufferHeight = bottom;

            ScrollBar().Maximum(bufferHeight - bufferHeight);
            ScrollBar().Minimum(0);
            ScrollBar().Value(0);
            ScrollBar().ViewportSize(bufferHeight);
            ScrollBar().LargeChange(std::max<SHORT>(bufferHeight - 1, 0)); // scroll one "screenful" at a time when the scroll bar is clicked

            localPointerToThread->EnablePainting();

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

            // import value from WinUser (convert from milli-seconds to micro-seconds)
            _multiClickTimer = GetDoubleClickTime() * 1000;

            // Focus the control here. If we do it during control initialization, then
            //      focus won't actually get passed to us. I believe this is because
            //      we're not technically a part of the UI tree yet, so focusing us
            //      becomes a no-op.
            this->Focus(FocusState::Programmatic);

            // Now that the renderer is set up, update the appearance for initialization
            _UpdateAppearanceFromUIThreadUnderLock(_settings);

            _initializedTerminal = true;
        } // scope for TerminalLock

        // Start the connection outside of lock, because it could
        // start writing output immediately.
        _connection.Start();

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
        const bool handled = _terminal->SendCharEvent(ch, scanCode, modifiers);
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
        if (_isReadOnly)
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
        if (_isReadOnly)
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
        if (_terminal->IsSelectionActive() &&
            !KeyEvent::IsModifierKey(vkey) &&
            vkey != VK_SNAPSHOT &&
            keyDown)
        {
            const CoreWindow window = CoreWindow::GetForCurrentThread();
            const auto leftWinKeyState = window.GetKeyState(VirtualKey::LeftWindows);
            const auto rightWinKeyState = window.GetKeyState(VirtualKey::RightWindows);
            const auto isLeftWinKeyDown = WI_IsFlagSet(leftWinKeyState, CoreVirtualKeyStates::Down);
            const auto isRightWinKeyDown = WI_IsFlagSet(rightWinKeyState, CoreVirtualKeyStates::Down);

            // GH#8791 - don't dismiss selection if Windows key was also pressed as a key-combination.
            if (!isLeftWinKeyDown && !isRightWinKeyDown)
            {
                _terminal->ClearSelection();
                _renderer->TriggerSelection();
            }

            if (vkey == VK_ESCAPE)
            {
                return true;
            }
        }

        if (vkey == VK_ESCAPE ||
            vkey == VK_RETURN)
        {
            TSFInputControl().ClearBuffer();
        }

        // If the terminal translated the key, mark the event as handled.
        // This will prevent the system from trying to get the character out
        // of it and sending us a CharacterReceived event.
        const auto handled = vkey ? _terminal->SendKeyEvent(vkey, scanCode, modifiers, keyDown) : true;

        if (_cursorTimer.has_value())
        {
            // Manually show the cursor when a key is pressed. Restarting
            // the timer prevents flickering.
            _terminal->SetCursorOn(true);
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
    // - Send this particular mouse event to the terminal.
    //   See Terminal::SendMouseEvent for more information.
    // Arguments:
    // - point: the PointerPoint object representing a mouse event from our XAML input handler
    bool TermControl::_TrySendMouseEvent(Windows::UI::Input::PointerPoint const& point)
    {
        const auto props = point.Properties();

        // Get the terminal position relative to the viewport
        const auto terminalPosition = _GetTerminalPosition(point.Position());

        // Which mouse button changed state (and how)
        unsigned int uiButton{};
        switch (props.PointerUpdateKind())
        {
        case PointerUpdateKind::LeftButtonPressed:
            uiButton = WM_LBUTTONDOWN;
            break;
        case PointerUpdateKind::LeftButtonReleased:
            uiButton = WM_LBUTTONUP;
            break;
        case PointerUpdateKind::MiddleButtonPressed:
            uiButton = WM_MBUTTONDOWN;
            break;
        case PointerUpdateKind::MiddleButtonReleased:
            uiButton = WM_MBUTTONUP;
            break;
        case PointerUpdateKind::RightButtonPressed:
            uiButton = WM_RBUTTONDOWN;
            break;
        case PointerUpdateKind::RightButtonReleased:
            uiButton = WM_RBUTTONUP;
            break;
        default:
            uiButton = WM_MOUSEMOVE;
        }

        // Mouse wheel data
        const short sWheelDelta = ::base::saturated_cast<short>(props.MouseWheelDelta());
        if (sWheelDelta != 0 && !props.IsHorizontalMouseWheel())
        {
            // if we have a mouse wheel delta and it wasn't a horizontal wheel motion
            uiButton = WM_MOUSEWHEEL;
        }

        const auto modifiers = _GetPressedModifierKeys();
        const TerminalInput::MouseButtonState state{ props.IsLeftButtonPressed(), props.IsMiddleButtonPressed(), props.IsRightButtonPressed() };
        return _terminal->SendMouseEvent(terminalPosition, uiButton, modifiers, sWheelDelta, state);
    }

    // Method Description:
    // - Checks if we can send vt mouse input.
    // Arguments:
    // - point: the PointerPoint object representing a mouse event from our XAML input handler
    bool TermControl::_CanSendVTMouseInput()
    {
        if (!_terminal)
        {
            return false;
        }
        // If the user is holding down Shift, suppress mouse events
        // TODO GH#4875: disable/customize this functionality
        const auto modifiers = _GetPressedModifierKeys();
        if (modifiers.IsShiftPressed())
        {
            return false;
        }
        return _terminal->IsTrackingMouseInput();
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

        // We also TryShow in GotFocusHandler, but this call is specifically
        // for the case where the Terminal is in focus but the user closed the
        // on-screen keyboard. This lets the user simply tap on the terminal
        // again to bring it up.
        InputPane::GetForCurrentView().TryShow();

        if (!_focused)
        {
            Focus(FocusState::Pointer);
        }

        if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Mouse || ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Pen)
        {
            const auto modifiers = static_cast<uint32_t>(args.KeyModifiers());
            // static_cast to a uint32_t because we can't use the WI_IsFlagSet
            // macro directly with a VirtualKeyModifiers
            const auto altEnabled = WI_IsFlagSet(modifiers, static_cast<uint32_t>(VirtualKeyModifiers::Menu));
            const auto shiftEnabled = WI_IsFlagSet(modifiers, static_cast<uint32_t>(VirtualKeyModifiers::Shift));
            const auto ctrlEnabled = WI_IsFlagSet(modifiers, static_cast<uint32_t>(VirtualKeyModifiers::Control));

            auto lock = _terminal->LockForWriting();
            const auto cursorPosition = point.Position();
            const auto terminalPosition = _GetTerminalPosition(cursorPosition);

            // GH#9396: we prioritize hyper-link over VT mouse events
            if (point.Properties().IsLeftButtonPressed() && ctrlEnabled && !_terminal->GetHyperlinkAtPosition(terminalPosition).empty())
            {
                // Handle hyper-link only on the first click to prevent multiple activations
                const auto clickCount = _NumberOfClicks(cursorPosition, point.Timestamp());
                if (clickCount == 1)
                {
                    _HyperlinkHandler(_terminal->GetHyperlinkAtPosition(terminalPosition));
                }
            }
            else if (_CanSendVTMouseInput())
            {
                _TrySendMouseEvent(point);
            }
            else if (point.Properties().IsLeftButtonPressed())
            {
                // Update the selection appropriately
                // handle ALT key
                _terminal->SetBlockSelection(altEnabled);

                const auto clickCount = _NumberOfClicks(cursorPosition, point.Timestamp());

                // This formula enables the number of clicks to cycle properly between single-, double-, and triple-click.
                // To increase the number of acceptable click states, simply increment MAX_CLICK_COUNT and add another if-statement
                const unsigned int MAX_CLICK_COUNT = 3;
                const auto multiClickMapper = clickCount > MAX_CLICK_COUNT ? ((clickCount + MAX_CLICK_COUNT - 1) % MAX_CLICK_COUNT) + 1 : clickCount;

                ::Terminal::SelectionExpansionMode mode = ::Terminal::SelectionExpansionMode::Cell;
                if (multiClickMapper == 1)
                {
                    mode = ::Terminal::SelectionExpansionMode::Cell;
                }
                else if (multiClickMapper == 2)
                {
                    mode = ::Terminal::SelectionExpansionMode::Word;
                }
                else if (multiClickMapper == 3)
                {
                    mode = ::Terminal::SelectionExpansionMode::Line;
                }

                // Capture the position of the first click
                if (mode == ::Terminal::SelectionExpansionMode::Cell)
                {
                    _singleClickTouchdownPos = cursorPosition;
                    if (!_terminal->IsSelectionActive())
                    {
                        _lastMouseClickPosNoSelection = cursorPosition;
                    }
                }

                // We reset the active selection if one of the conditions apply:
                // - shift is not held
                // - GH#9384: the position is the same as of the first click starting the selection
                // (we need to reset selection on double-click or triple-click, so it captures the word or the line,
                // rather than extending the selection)
                if (_terminal->IsSelectionActive() && (!shiftEnabled || _lastMouseClickPosNoSelection == cursorPosition))
                {
                    // Reset the selection
                    _terminal->ClearSelection();
                    _selectionNeedsToBeCopied = false; // there's no selection, so there's nothing to update
                }

                if (shiftEnabled && _terminal->IsSelectionActive())
                {
                    // If shift is pressed and there is a selection we extend it using the selection mode
                    // (expand the "end"selection point)
                    _terminal->SetSelectionEnd(terminalPosition, mode);
                    _selectionNeedsToBeCopied = true;
                }
                else if (mode != ::Terminal::SelectionExpansionMode::Cell || shiftEnabled)
                {
                    // If we are handling a double / triple-click or shift+single click
                    // we establish selection using the selected mode
                    // (expand both "start" and "end" selection points)
                    _terminal->MultiClickSelection(terminalPosition, mode);
                    _selectionNeedsToBeCopied = true;
                }

                if (_terminal->IsSelectionActive())
                {
                    // GH#9787: if selection is active we don't want to track the touchdown position
                    // so that dragging the mouse will extend the selection rather than starting the new one
                    _singleClickTouchdownPos = std::nullopt;
                }

                _renderer->TriggerSelection();
            }
            else if (point.Properties().IsRightButtonPressed())
            {
                if (_settings.CopyOnSelect() || !_terminal->IsSelectionActive())
                {
                    // CopyOnSelect right click always pastes
                    PasteTextFromClipboard();
                }
                else
                {
                    CopySelectionToClipboard(shiftEnabled, nullptr);
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
        const auto terminalPosition = _GetTerminalPosition(cursorPosition);

        if (!_focused && _settings.FocusFollowMouse())
        {
            _FocusFollowMouseRequestedHandlers(*this, nullptr);
        }

        if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Mouse || ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Pen)
        {
            // Short-circuit isReadOnly check to avoid warning dialog
            if (_focused && !_isReadOnly && _CanSendVTMouseInput())
            {
                _TrySendMouseEvent(point);
            }
            else if (_focused && point.Properties().IsLeftButtonPressed())
            {
                auto lock = _terminal->LockForWriting();

                if (_singleClickTouchdownPos)
                {
                    // Figure out if the user's moved a quarter of a cell's smaller axis away from the clickdown point
                    auto& touchdownPoint{ *_singleClickTouchdownPos };
                    auto distance{ std::sqrtf(std::powf(cursorPosition.X - touchdownPoint.X, 2) + std::powf(cursorPosition.Y - touchdownPoint.Y, 2)) };
                    const til::size fontSize{ _actualFont.GetSize() };

                    const auto fontSizeInDips = fontSize.scale(til::math::rounding, 1.0f / _renderEngine->GetScaling());
                    if (distance >= (std::min(fontSizeInDips.width(), fontSizeInDips.height()) / 4.f))
                    {
                        _terminal->SetSelectionAnchor(_GetTerminalPosition(touchdownPoint));
                        // stop tracking the touchdown point
                        _singleClickTouchdownPos = std::nullopt;
                    }
                }

                _SetEndSelectionPointAtCursor(cursorPosition);

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

            _UpdateHoveredCell(terminalPosition);
        }
        else if (_focused && ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Touch && _touchAnchor)
        {
            const auto contactRect = point.Properties().ContactRect();
            winrt::Windows::Foundation::Point newTouchPoint{ contactRect.X, contactRect.Y };
            const auto anchor = _touchAnchor.value();

            // Our _actualFont's size is in pixels, convert to DIPs, which the
            // rest of the Points here are in.
            const til::size fontSize{ _actualFont.GetSize() };
            const auto fontSizeInDips = fontSize.scale(til::math::rounding, 1.0f / _renderEngine->GetScaling());

            // Get the difference between the point we've dragged to and the start of the touch.
            const float dy = newTouchPoint.Y - anchor.Y;

            // Start viewport scroll after we've moved more than a half row of text
            if (std::abs(dy) > (fontSizeInDips.height<float>() / 2.0f))
            {
                // Multiply by -1, because moving the touch point down will
                // create a positive delta, but we want the viewport to move up,
                // so we'll need a negative scroll amount (and the inverse for
                // panning down)
                const float numRows = -1.0f * (dy / fontSizeInDips.height<float>());

                const auto currentOffset = ::base::ClampedNumeric<double>(ScrollBar().Value());
                const auto newValue = numRows + currentOffset;

                ScrollBar().Value(newValue);

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

        _ReleasePointerCapture(sender, args);

        if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Mouse || ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Pen)
        {
            // Short-circuit isReadOnly check to avoid warning dialog
            if (!_isReadOnly && _CanSendVTMouseInput())
            {
                _TrySendMouseEvent(point);
                args.Handled(true);
                return;
            }

            // Only a left click release when copy on select is active should perform a copy.
            // Right clicks and middle clicks should not need to do anything when released.
            if (_settings.CopyOnSelect() && point.Properties().PointerUpdateKind() == Windows::UI::Input::PointerUpdateKind::LeftButtonReleased && _selectionNeedsToBeCopied)
            {
                CopySelectionToClipboard(false, nullptr);
            }
        }
        else if (ptr.PointerDeviceType() == Windows::Devices::Input::PointerDeviceType::Touch)
        {
            _touchAnchor = std::nullopt;
        }

        _singleClickTouchdownPos = std::nullopt;

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
        const auto props = point.Properties();
        const TerminalInput::MouseButtonState state{ props.IsLeftButtonPressed(), props.IsMiddleButtonPressed(), props.IsRightButtonPressed() };
        auto result = _DoMouseWheel(point.Position(),
                                    ControlKeyStates{ args.KeyModifiers() },
                                    point.Properties().MouseWheelDelta(),
                                    state);
        if (result)
        {
            args.Handled(true);
        }
    }

    // Method Description:
    // - Actually handle a scrolling event, whether from a mouse wheel or a

    //   touchpad scroll. Depending upon what modifier keys are pressed,
    //   different actions will take place.
    //   * Attempts to first dispatch the mouse scroll as a VT event
    //   * If Ctrl+Shift are pressed, then attempts to change our opacity
    //   * If just Ctrl is pressed, we'll attempt to "zoom" by changing our font size
    //   * Otherwise, just scrolls the content of the viewport
    // Arguments:
    // - point: the location of the mouse during this event
    // - modifiers: The modifiers pressed during this event, in the form of a VirtualKeyModifiers
    // - delta: the mouse wheel delta that triggered this event.
    bool TermControl::_DoMouseWheel(const Windows::Foundation::Point point,
                                    const ControlKeyStates modifiers,
                                    const int32_t delta,
                                    const TerminalInput::MouseButtonState state)
    {
        // Short-circuit isReadOnly check to avoid warning dialog
        if (!_isReadOnly && _CanSendVTMouseInput())
        {
            // Most mouse event handlers call
            //      _TrySendMouseEvent(point);
            // here with a PointerPoint. However, as of #979, we don't have a
            // PointerPoint to work with. So, we're just going to do a
            // mousewheel event manually
            return _terminal->SendMouseEvent(_GetTerminalPosition(point),
                                             WM_MOUSEWHEEL,
                                             _GetPressedModifierKeys(),
                                             ::base::saturated_cast<short>(delta),
                                             state);
        }

        const auto ctrlPressed = modifiers.IsCtrlPressed();
        const auto shiftPressed = modifiers.IsShiftPressed();

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
            _MouseScrollHandler(delta, point, state.isLeftButtonDown);
        }
        return false;
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
        TerminalInput::MouseButtonState state{ leftButtonDown, midButtonDown, rightButtonDown };
        return _DoMouseWheel(location, modifiers, delta, state);
    }

    // Method Description:
    // - Tell TerminalCore to update its knowledge about the locations of visible regex patterns
    // - We should call this (through the throttled function) when something causes the visible
    //   region to change, such as when new text enters the buffer or the viewport is scrolled
    void TermControl::UpdatePatternLocations()
    {
        _terminal->UpdatePatterns();
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
                auto acrylicBrush = RootGrid().Background().as<Media::AcrylicBrush>();
                _settings.TintOpacity(acrylicBrush.TintOpacity() + effectiveDelta);
                acrylicBrush.TintOpacity(_settings.TintOpacity());

                if (acrylicBrush.TintOpacity() == 1.0)
                {
                    _settings.UseAcrylic(false);
                    _InitializeBackgroundBrush();
                    const auto bg = _settings.DefaultBackground();
                    _BackgroundColorChanged(bg);
                }
                else
                {
                    // GH#5098: Inform the engine of the new opacity of the default text background.
                    if (_renderEngine)
                    {
                        _renderEngine->SetDefaultTextBackgroundOpacity(::base::saturated_cast<float>(_settings.TintOpacity()));
                    }
                }
            }
            CATCH_LOG();
        }
        else if (mouseDelta < 0)
        {
            _settings.UseAcrylic(true);

            //Setting initial opacity set to 1 to ensure smooth transition to acrylic during mouse scroll
            _settings.TintOpacity(1.0);
            _InitializeBackgroundBrush();
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
        AdjustFontSize(fontDelta);
    }

    // Method Description:
    // - Reset the font size of the terminal to its default size.
    // Arguments:
    // - none
    void TermControl::ResetFontSize()
    {
        _SetFontSize(_settings.FontSize());
    }

    // Method Description:
    // - Adjust the font size of the terminal control.
    // Arguments:
    // - fontSizeDelta: The amount to increase or decrease the font size by.
    void TermControl::AdjustFontSize(int fontSizeDelta)
    {
        const auto newSize = _desiredFont.GetEngineSize().Y + fontSizeDelta;
        _SetFontSize(newSize);
    }

    // Method Description:
    // - Scroll the visible viewport in response to a mouse wheel event.
    // Arguments:
    // - mouseDelta: the mouse wheel delta that triggered this event.
    // - point: the location of the mouse during this event
    // - isLeftButtonPressed: true iff the left mouse button was pressed during this event.
    void TermControl::_MouseScrollHandler(const double mouseDelta,
                                          const Windows::Foundation::Point point,
                                          const bool isLeftButtonPressed)
    {
        const auto currentOffset = ScrollBar().Value();

        // negative = down, positive = up
        // However, for us, the signs are flipped.
        // With one of the precision mice, one click is always a multiple of 120 (WHEEL_DELTA),
        // but the "smooth scrolling" mode results in non-int values
        const auto rowDelta = mouseDelta / (-1.0 * WHEEL_DELTA);

        // WHEEL_PAGESCROLL is a Win32 constant that represents the "scroll one page
        // at a time" setting. If we ignore it, we will scroll a truly absurd number
        // of rows.
        const auto rowsToScroll{ _rowsToScroll == WHEEL_PAGESCROLL ? GetViewHeight() : _rowsToScroll };
        double newValue = (rowsToScroll * rowDelta) + (currentOffset);

        // The scroll bar's ValueChanged handler will actually move the viewport
        //      for us.
        ScrollBar().Value(newValue);

        if (_terminal->IsSelectionActive() && isLeftButtonPressed)
        {
            // Have to take the lock or we could change the endpoints out from under the renderer actively rendering.
            auto lock = _terminal->LockForWriting();

            // If user is mouse selecting and scrolls, they then point at new character.
            //      Make sure selection reflects that immediately.
            _SetEndSelectionPointAtCursor(point);
        }
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

        // Clear the regex pattern tree so the renderer does not try to render them while scrolling
        {
            // We're taking the lock here instead of in ClearPatternTree because ClearPatternTree is
            // sometimes called from an already-locked context. Here, we are sure we are not
            // already under lock (since it is not an internal scroll bar update)
            // TODO GH#9617: refine locking around pattern tree
            auto lock = _terminal->LockForWriting();
            _terminal->ClearPatternTree();
        }

        const auto newValue = static_cast<int>(args.NewValue());

        // This is a scroll event that wasn't initiated by the terminal
        //      itself - it was initiated by the mouse wheel, or the scrollbar.
        _terminal->UserScrollViewport(newValue);

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
        if (!_autoScrollingPointerPoint.has_value() || _autoScrollingPointerPoint.value().PointerId() == pointerPoint.PointerId())
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
        if (_autoScrollingPointerPoint.has_value() && pointerId == _autoScrollingPointerPoint.value().PointerId())
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
    // - Called continuously to gradually scroll viewport when user is
    //      mouse selecting outside it (to 'follow' the cursor).
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
                    // Have to take the lock because the renderer will not draw correctly if you move its endpoints while it is generating a frame.
                    auto lock = _terminal->LockForWriting();

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
            _terminal->SetCursorOn(true);
            _cursorTimer.value().Start();
        }

        if (_blinkTimer.has_value())
        {
            _blinkTimer.value().Start();
        }

        _UpdateSystemParameterSettings();

        // Only update the appearance here if an unfocused config exists -
        // if an unfocused config does not exist then we never would have switched
        // appearances anyway so there's no need to switch back upon gaining focus
        if (_UnfocusedAppearance)
        {
            UpdateAppearance(_settings);
        }
    }

    // Method Description
    // - Updates internal params based on system parameters
    void TermControl::_UpdateSystemParameterSettings() noexcept
    {
        if (!SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &_rowsToScroll, 0))
        {
            LOG_LAST_ERROR();
            // If SystemParametersInfoW fails, which it shouldn't, fall back to
            // Windows' default value.
            _rowsToScroll = 3;
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
            _terminal->SetCursorOn(false);
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
    // - Writes the given sequence as input to the active terminal connection.
    // - This method has been overloaded to allow zero-copy winrt::param::hstring optimizations.
    // Arguments:
    // - wstr: the string of characters to write to the terminal connection.
    // Return Value:
    // - <none>
    void TermControl::_SendInputToConnection(const winrt::hstring& wstr)
    {
        if (_isReadOnly)
        {
            _RaiseReadOnlyWarning();
        }
        else
        {
            _connection.WriteInput(wstr);
        }
    }

    void TermControl::_SendInputToConnection(std::wstring_view wstr)
    {
        if (_isReadOnly)
        {
            _RaiseReadOnlyWarning();
        }
        else
        {
            _connection.WriteInput(wstr);
        }
    }

    // Method Description:
    // - Pre-process text pasted (presumably from the clipboard)
    //   before sending it over the terminal's connection.
    void TermControl::_SendPastedTextToConnection(const std::wstring& wstr)
    {
        _terminal->WritePastedText(wstr);
        _terminal->ClearSelection();
        _terminal->TrySnapOnInput();
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
    void TermControl::_UpdateFont(const bool initialUpdate)
    {
        const int newDpi = static_cast<int>(static_cast<double>(USER_DEFAULT_SCREEN_DPI) * SwapChainPanel().CompositionScaleX());

        // TODO: MSFT:20895307 If the font doesn't exist, this doesn't
        //      actually fail. We need a way to gracefully fallback.
        _renderer->TriggerFontChange(newDpi, _desiredFont, _actualFont);

        // If the actual font went through the last-chance fallback routines...
        if (_actualFont.GetFallback())
        {
            // Then warn the user that we picked something because we couldn't find their font.

            // Format message with user's choice of font and the font that was chosen instead.
            const winrt::hstring message{ fmt::format(std::wstring_view{ RS_(L"NoticeFontNotFound") }, _desiredFont.GetFaceName(), _actualFont.GetFaceName()) };

            // Capture what we need to resume later.
            [strongThis = get_strong(), message]() -> winrt::fire_and_forget {
                // Take these out of the lambda and store them locally
                // because the coroutine will lose them into space
                // by the time it resumes.
                const auto msg = message;
                const auto strong = strongThis;

                // Pop the rest of this function to the tail of the UI thread
                // Just in case someone was holding a lock when they called us and
                // the handlers decide to do something that take another lock
                // (like ShellExecute pumping our messaging thread...GH#7994)
                co_await strong->Dispatcher();

                auto noticeArgs = winrt::make<NoticeEventArgs>(NoticeLevel::Warning, std::move(msg));
                strong->_RaiseNoticeHandlers(*strong, std::move(noticeArgs));
            }();
        }

        const auto actualNewSize = _actualFont.GetSize();
        _fontSizeChangedHandlers(actualNewSize.X, actualNewSize.Y, initialUpdate);
    }

    // Method Description:
    // - Set the font size of the terminal control.
    // Arguments:
    // - fontSize: The size of the font.
    void TermControl::_SetFontSize(int fontSize)
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
            _UpdateFont();

            // Resize the terminal's BUFFER to match the new font size. This does
            // NOT change the size of the window, because that can lead to more
            // problems (like what happens when you change the font size while the
            // window is maximized?)
            _RefreshSizeUnderLock();
        }
        CATCH_LOG();
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

        auto lock = _terminal->LockForWriting();

        const auto newSize = e.NewSize();
        const auto currentScaleX = SwapChainPanel().CompositionScaleX();
        const auto currentEngineScale = _renderEngine->GetScaling();
        auto foundationSize = newSize;

        // A strange thing can happen here. If you have two tabs open, and drag
        // across a DPI boundary, then switch to the other tab, that tab will
        // receive two events: First, a SizeChanged, then a ScaleChanged. In the
        // SizeChanged event handler, the SwapChainPanel's CompositionScale will
        // _already_ be the new scaling, but the engine won't have that value
        // yet. If we scale by the CompositionScale here, we'll end up in a
        // weird torn state. I'm not totally sure why.
        //
        // Fortunately we will be getting that following ScaleChanged event, and
        // we'll end up resizing again, so we don't terribly need to worry about
        // this.
        foundationSize.Width *= currentEngineScale;
        foundationSize.Height *= currentEngineScale;

        _DoResizeUnderLock(foundationSize.Width, foundationSize.Height);
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
        if (_renderEngine)
        {
            const auto scaleX = sender.CompositionScaleX();
            const auto scaleY = sender.CompositionScaleY();
            const auto dpi = (float)(scaleX * USER_DEFAULT_SCREEN_DPI);
            const auto currentEngineScale = _renderEngine->GetScaling();

            // If we're getting a notification to change to the DPI we already
            // have, then we're probably just beginning the DPI change. Since
            // we'll get _another_ event with the real DPI, do nothing here for
            // now. We'll also skip the next resize in _SwapChainSizeChanged.
            const bool dpiWasUnchanged = currentEngineScale == scaleX;
            if (dpiWasUnchanged)
            {
                return;
            }

            const auto actualFontOldSize = _actualFont.GetSize();

            auto lock = _terminal->LockForWriting();

            _renderer->TriggerFontChange(::base::saturated_cast<int>(dpi), _desiredFont, _actualFont);

            const auto actualFontNewSize = _actualFont.GetSize();
            if (actualFontNewSize != actualFontOldSize)
            {
                _RefreshSizeUnderLock();
            }
        }
    }

    // Method Description:
    // - Toggle the cursor on and off when called by the cursor blink timer.
    // Arguments:
    // - sender: not used
    // - e: not used
    void TermControl::_CursorTimerTick(Windows::Foundation::IInspectable const& /* sender */,
                                       Windows::Foundation::IInspectable const& /* e */)
    {
        if ((_closing) || (!_terminal->IsCursorBlinkingAllowed() && _terminal->IsCursorVisible()))
        {
            return;
        }
        _terminal->SetCursorOn(!_terminal->IsCursorOn());
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
            auto& renderTarget = *_renderer;
            auto& blinkingState = _terminal->GetBlinkingState();
            blinkingState.ToggleBlinkingRendition(renderTarget);
        }
    }

    // Method Description:
    // - Sets selection's end position to match supplied cursor position, e.g. while mouse dragging.
    // Arguments:
    // - cursorPosition: in pixels, relative to the origin of the control
    void TermControl::_SetEndSelectionPointAtCursor(Windows::Foundation::Point const& cursorPosition)
    {
        if (!_terminal->IsSelectionActive())
        {
            return;
        }

        auto terminalPosition = _GetTerminalPosition(cursorPosition);

        const short lastVisibleRow = std::max<short>(_terminal->GetViewport().Height() - 1, 0);
        const short lastVisibleCol = std::max<short>(_terminal->GetViewport().Width() - 1, 0);

        terminalPosition.Y = std::clamp<short>(terminalPosition.Y, 0, lastVisibleRow);
        terminalPosition.X = std::clamp<short>(terminalPosition.X, 0, lastVisibleCol);

        // save location (for rendering) + render
        _terminal->SetSelectionEnd(terminalPosition);
        _renderer->TriggerSelection();
        _selectionNeedsToBeCopied = true;
    }

    // Method Description:
    // - Perform a resize for the current size of the swapchainpanel. If the
    //   font size changed, we'll need to resize the buffer to fit the existing
    //   swapchain size. This helper will call _DoResizeUnderLock with the current size
    //   of the swapchain, accounting for scaling due to DPI.
    // - Note that a DPI change will also trigger a font size change, and will call into here.
    // - The write lock should be held when calling this method, we might be changing the buffer size in _DoResizeUnderLock.
    // Arguments:
    // - <none>
    // Return Value:
    // - <none>
    void TermControl::_RefreshSizeUnderLock()
    {
        const auto currentScaleX = SwapChainPanel().CompositionScaleX();
        const auto currentScaleY = SwapChainPanel().CompositionScaleY();
        const auto actualWidth = SwapChainPanel().ActualWidth();
        const auto actualHeight = SwapChainPanel().ActualHeight();

        const auto widthInPixels = actualWidth * currentScaleX;
        const auto heightInPixels = actualHeight * currentScaleY;

        _DoResizeUnderLock(widthInPixels, heightInPixels);
    }

    // Method Description:
    // - Process a resize event that was initiated by the user. This can either
    //   be due to the user resizing the window (causing the swapchain to
    //   resize) or due to the DPI changing (causing us to need to resize the
    //   buffer to match)
    // Arguments:
    // - newWidth: the new width of the swapchain, in pixels.
    // - newHeight: the new height of the swapchain, in pixels.
    void TermControl::_DoResizeUnderLock(const double newWidth, const double newHeight)
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

    void TermControl::_TerminalWarningBell()
    {
        _WarningBellHandlers(*this, nullptr);
    }

    void TermControl::_TerminalTitleChanged(const std::wstring_view& wstr)
    {
        _TitleChangedHandlers(*this, winrt::make<TitleChangedEventArgs>(winrt::hstring{ wstr }));
    }
    void TermControl::_TerminalTabColorChanged(const std::optional<til::color> /*color*/)
    {
        _TabColorChangedHandlers(*this, nullptr);
    }

    void TermControl::_CopyToClipboard(const std::wstring_view& wstr)
    {
        auto copyArgs = winrt::make_self<CopyToClipboardEventArgs>(winrt::hstring(wstr));
        _CopyToClipboardHandlers(*this, *copyArgs);
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
    void TermControl::_TerminalScrollPositionChanged(const int viewTop,
                                                     const int viewHeight,
                                                     const int bufferSize)
    {
        // Since this callback fires from non-UI thread, we might be already
        // closed/closing.
        if (_closing.load())
        {
            return;
        }

        // Clear the regex pattern tree so the renderer does not try to render them while scrolling
        // We're **NOT** taking the lock here unlike _ScrollbarChangeHandler because
        // we are already under lock (since this usually happens as a result of writing).
        // TODO GH#9617: refine locking around pattern tree
        _terminal->ClearPatternTree();

        _scrollPositionChangedHandlers(viewTop, viewHeight, bufferSize);

        ScrollBarUpdate update;
        const auto hiddenContent = bufferSize - viewHeight;
        update.newMaximum = hiddenContent;
        update.newMinimum = 0;
        update.newViewportSize = viewHeight;
        update.newValue = viewTop;

        _updateScrollBar->Run(update);
        _updatePatternLocations->Run();
    }

    // Method Description:
    // - Tells TSFInputControl to redraw the Canvas/TextBlock so it'll update
    //   to be where the current cursor position is.
    // Arguments:
    // - N/A
    void TermControl::_TerminalCursorPositionChanged()
    {
        _tsfTryRedrawCanvas->Run();
    }

    hstring TermControl::Title()
    {
        hstring hstr{ _terminal->GetConsoleTitle() };
        return hstr;
    }

    hstring TermControl::GetProfileName() const
    {
        return _settings.ProfileName();
    }

    hstring TermControl::WorkingDirectory() const
    {
        hstring hstr{ _terminal->GetWorkingDirectory() };
        return hstr;
    }

    bool TermControl::BracketedPasteEnabled() const noexcept
    {
        return _terminal->IsXtermBracketedPasteModeEnabled();
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

        // no selection --> nothing to copy
        if (!_terminal->IsSelectionActive())
        {
            return false;
        }

        // Mark the current selection as copied
        _selectionNeedsToBeCopied = false;

        // extract text from buffer
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
        auto copyArgs = winrt::make_self<CopyToClipboardEventArgs>(winrt::hstring(textData),
                                                                   winrt::to_hstring(htmlData),
                                                                   winrt::to_hstring(rtfData),
                                                                   formats);
        _CopyToClipboardHandlers(*this, *copyArgs);
        return true;
    }

    // Method Description:
    // - Initiate a paste operation.
    void TermControl::PasteTextFromClipboard()
    {
        // attach TermControl::_SendInputToConnection() as the clipboardDataHandler.
        // This is called when the clipboard data is loaded.
        auto clipboardDataHandler = std::bind(&TermControl::_SendPastedTextToConnection, this, std::placeholders::_1);
        auto pasteArgs = winrt::make_self<PasteFromClipboardEventArgs>(clipboardDataHandler);

        // send paste event up to TermApp
        _PasteFromClipboardHandlers(*this, *pasteArgs);
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
    winrt::fire_and_forget TermControl::_AsyncCloseConnection()
    {
        if (auto localConnection{ std::exchange(_connection, nullptr) })
        {
            // Close the connection on the background thread.
            co_await winrt::resume_background();
            localConnection.Close();
            // connection is destroyed.
        }
    }

    void TermControl::Close()
    {
        if (!_closing.exchange(true))
        {
            _RestorePointerCursorHandlers(*this, nullptr);

            // Stop accepting new output and state changes before we disconnect everything.
            _connection.TerminalOutput(_connectionOutputEventToken);
            _connectionStateChangedRevoker.revoke();

            TSFInputControl().Close(); // Disconnect the TSF input control so it doesn't receive EditContext events.
            _autoScrollTimer.Stop();

            // GH#1996 - Close the connection asynchronously on a background
            // thread.
            // Since TermControl::Close is only ever triggered by the UI, we
            // don't really care to wait for the connection to be completely
            // closed. We can just do it whenever.
            _AsyncCloseConnection();

            {
                // GH#8734:
                // We lock the terminal here to make sure it isn't still being
                // used in the connection thread before we destroy the renderer.
                // However, we must unlock it again prior to triggering the
                // teardown, to avoid the render thread being deadlocked. The
                // renderer may be waiting to acquire the terminal lock, while
                // we're waiting for the renderer to finish.
                auto lock = _terminal->LockForWriting();
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

            // we don't destroy _terminal here; it now has the same lifetime as the
            // control.
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
        const auto fontSize = _actualFont.GetSize();
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
            const auto fontSize = _actualFont.GetSize();
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
        const auto fontSize = _actualFont.GetSize();
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

        constexpr std::array<KeyModifier, 5> modifiers{ {
            { VirtualKey::RightMenu, ControlKeyStates::RightAltPressed },
            { VirtualKey::LeftMenu, ControlKeyStates::LeftAltPressed },
            { VirtualKey::RightControl, ControlKeyStates::RightCtrlPressed },
            { VirtualKey::LeftControl, ControlKeyStates::LeftCtrlPressed },
            { VirtualKey::Shift, ControlKeyStates::ShiftPressed },
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
        // cursorPosition is DIPs, relative to SwapChainPanel origin
        const til::point cursorPosInDIPs{ til::math::rounding, cursorPosition };
        const til::size marginsInDips{ til::math::rounding, GetPadding().Left, GetPadding().Top };

        // This point is the location of the cursor within the actual grid of characters, in DIPs
        const til::point relativeToMarginInDIPs = cursorPosInDIPs - marginsInDips;

        // Convert it to pixels
        const til::point relativeToMarginInPixels{ relativeToMarginInDIPs * SwapChainPanel().CompositionScaleX() };

        // Get the size of the font, which is in pixels
        const til::size fontSize{ _actualFont.GetSize() };

        // Convert the location in pixels to characters within the current viewport.
        return til::point{ relativeToMarginInPixels / fontSize };
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

        _connection.WriteInput(text);
    }

    // Method Description:
    // - CurrentCursorPosition handler for the TSFInputControl that
    //   handles returning current cursor position.
    // Arguments:
    // - eventArgs: event for storing the current cursor position
    // Return Value:
    // - <none>
    void TermControl::_CurrentCursorPositionHandler(const IInspectable& /*sender*/, const CursorPositionEventArgs& eventArgs)
    {
        auto lock = _terminal->LockForReading();
        if (!_initializedTerminal)
        {
            // fake it
            eventArgs.CurrentPosition({ 0, 0 });
            return;
        }

        const til::point cursorPos = _terminal->GetCursorPosition();
        Windows::Foundation::Point p = { ::base::ClampedNumeric<float>(cursorPos.x()), ::base::ClampedNumeric<float>(cursorPos.y()) };
        eventArgs.CurrentPosition(p);
    }

    // Method Description:
    // - FontInfo handler for the TSFInputControl that
    //   handles returning current font information
    // Arguments:
    // - eventArgs: event for storing the current font information
    // Return Value:
    // - <none>
    void TermControl::_FontInfoHandler(const IInspectable& /*sender*/, const FontInfoEventArgs& eventArgs)
    {
        eventArgs.FontSize(CharacterDimensions());
        eventArgs.FontFace(_actualFont.GetFaceName());
        ::winrt::Windows::UI::Text::FontWeight weight;
        weight.Weight = static_cast<uint16_t>(_actualFont.GetWeight());
        eventArgs.FontWeight(weight);
    }

    // Method Description:
    // - Returns the number of clicks that occurred (double and triple click support).
    // Every call to this function registers a click.
    // Arguments:
    // - clickPos: the (x,y) position of a given cursor (i.e.: mouse cursor).
    //    NOTE: origin (0,0) is top-left.
    // - clickTime: the timestamp that the click occurred
    // Return Value:
    // - if the click is in the same position as the last click and within the timeout, the number of clicks within that time window
    // - otherwise, 1
    const unsigned int TermControl::_NumberOfClicks(winrt::Windows::Foundation::Point clickPos, Timestamp clickTime)
    {
        // if click occurred at a different location or past the multiClickTimer...
        Timestamp delta;
        THROW_IF_FAILED(UInt64Sub(clickTime, _lastMouseClickTimestamp, &delta));
        if (clickPos != _lastMouseClickPos || delta > _multiClickTimer)
        {
            _multiClickCounter = 1;
        }
        else
        {
            _multiClickCounter++;
        }

        _lastMouseClickTimestamp = clickTime;
        _lastMouseClickPos = clickPos;
        return _multiClickCounter;
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
    winrt::fire_and_forget TermControl::_DragDropHandler(Windows::Foundation::IInspectable const& /*sender*/,
                                                         DragEventArgs const e)
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
                _SendPastedTextToConnection(std::wstring{ link.AbsoluteUri() });
            }
            CATCH_LOG();
        }
        else if (e.DataView().Contains(StandardDataFormats::WebLink()))
        {
            try
            {
                Windows::Foundation::Uri link{ co_await e.DataView().GetWebLinkAsync() };
                _SendPastedTextToConnection(std::wstring{ link.AbsoluteUri() });
            }
            CATCH_LOG();
        }
        else if (e.DataView().Contains(StandardDataFormats::Text()))
        {
            try
            {
                std::wstring text{ co_await e.DataView().GetTextAsync() };
                _SendPastedTextToConnection(text);
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

                    auto lock = _terminal->LockForWriting();

                    if (containsSpaces)
                    {
                        fullPath.insert(0, L"\"");
                        fullPath += L"\"";
                    }

                    allPaths += fullPath;
                }
                _SendInputToConnection(allPaths);
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
    winrt::fire_and_forget TermControl::_HyperlinkHandler(const std::wstring_view uri)
    {
        // Save things we need to resume later.
        winrt::hstring heldUri{ uri };
        auto strongThis{ get_strong() };

        // Pop the rest of this function to the tail of the UI thread
        // Just in case someone was holding a lock when they called us and
        // the handlers decide to do something that take another lock
        // (like ShellExecute pumping our messaging thread...GH#7994)
        co_await Dispatcher();

        auto hyperlinkArgs = winrt::make_self<OpenHyperlinkEventArgs>(heldUri);
        _OpenHyperlinkHandlers(*strongThis, *hyperlinkArgs);
    }

    // Method Description:
    // - Produces the error dialog that notifies the user that rendering cannot proceed.
    winrt::fire_and_forget TermControl::_RendererEnteredErrorState()
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
        _renderer->ResetErrorStateAndResume();
    }

    IControlSettings TermControl::Settings() const
    {
        return _settings;
    }

    Windows::Foundation::IReference<winrt::Windows::UI::Color> TermControl::TabColor() noexcept
    {
        auto coreColor = _terminal->GetTabColor();
        return coreColor.has_value() ? Windows::Foundation::IReference<winrt::Windows::UI::Color>(til::color{ coreColor.value() }) : nullptr;
    }

    // Method Description:
    // - Sends an event (which will be caught by TerminalPage and forwarded to AppHost after)
    //   to set the progress indicator on the taskbar
    winrt::fire_and_forget TermControl::TaskbarProgressChanged()
    {
        co_await resume_foreground(Dispatcher(), CoreDispatcherPriority::High);
        _SetTaskbarProgressHandlers(*this, nullptr);
    }

    // Method Description:
    // - Gets the internal taskbar state value
    // Return Value:
    // - The taskbar state of this control
    const size_t TermControl::TaskbarState() const noexcept
    {
        return _terminal->GetTaskbarState();
    }

    // Method Description:
    // - Gets the internal taskbar progress value
    // Return Value:
    // - The taskbar progress of this control
    const size_t TermControl::TaskbarProgress() const noexcept
    {
        return _terminal->GetTaskbarProgress();
    }

    // Method Description:
    // - Checks whether the control is in a read-only mode (in this mode node input is sent to connection).
    // Return Value:
    // - True if the mode is read-only
    bool TermControl::ReadOnly() const noexcept
    {
        return _isReadOnly;
    }

    // Method Description:
    // - Toggles the read-only flag, raises event describing the value change
    void TermControl::ToggleReadOnly()
    {
        _isReadOnly = !_isReadOnly;
        _ReadOnlyChangedHandlers(*this, winrt::box_value(_isReadOnly));
    }

    winrt::fire_and_forget TermControl::_RaiseReadOnlyWarning()
    {
        auto weakThis{ get_weak() };
        co_await winrt::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            auto noticeArgs = winrt::make<NoticeEventArgs>(NoticeLevel::Info, RS_(L"TermControlReadOnly"));
            control->_RaiseNoticeHandlers(*control, std::move(noticeArgs));
        }
    }

    // Method description:
    // - Updates last hovered cell, renders / removes rendering of hyper-link if required
    // Arguments:
    // - terminalPosition: The terminal position of the pointer
    void TermControl::_UpdateHoveredCell(const std::optional<COORD>& terminalPosition)
    {
        if (terminalPosition == _lastHoveredCell)
        {
            return;
        }

        _lastHoveredCell = terminalPosition;

        uint16_t newId{ 0u };
        // we can't use auto here because we're pre-declaring newInterval.
        decltype(_terminal->GetHyperlinkIntervalFromPosition(COORD{})) newInterval{ std::nullopt };
        if (terminalPosition.has_value())
        {
            auto lock = _terminal->LockForReading(); // Lock for the duration of our reads.

            const auto uri = _terminal->GetHyperlinkAtPosition(*terminalPosition);
            if (!uri.empty())
            {
                // Update the tooltip with the URI
                HoveredUri().Text(uri);

                // Set the border thickness so it covers the entire cell
                const auto charSizeInPixels = CharacterDimensions();
                const auto htInDips = charSizeInPixels.Height / SwapChainPanel().CompositionScaleY();
                const auto wtInDips = charSizeInPixels.Width / SwapChainPanel().CompositionScaleX();
                const Thickness newThickness{ wtInDips, htInDips, 0, 0 };
                HyperlinkTooltipBorder().BorderThickness(newThickness);

                // Compute the location of the top left corner of the cell in DIPS
                const til::size marginsInDips{ til::math::rounding, GetPadding().Left, GetPadding().Top };
                const til::point startPos{ terminalPosition->X, terminalPosition->Y };
                const til::size fontSize{ _actualFont.GetSize() };
                const til::point posInPixels{ startPos * fontSize };
                const til::point posInDIPs{ posInPixels / SwapChainPanel().CompositionScaleX() };
                const til::point locationInDIPs{ posInDIPs + marginsInDips };

                // Move the border to the top left corner of the cell
                OverlayCanvas().SetLeft(HyperlinkTooltipBorder(), (locationInDIPs.x() - SwapChainPanel().ActualOffset().x));
                OverlayCanvas().SetTop(HyperlinkTooltipBorder(), (locationInDIPs.y() - SwapChainPanel().ActualOffset().y));
            }

            newId = _terminal->GetHyperlinkIdAtPosition(*terminalPosition);
            newInterval = _terminal->GetHyperlinkIntervalFromPosition(*terminalPosition);
        }

        // If the hyperlink ID changed or the interval changed, trigger a redraw all
        // (so this will happen both when we move onto a link and when we move off a link)
        if (newId != _lastHoveredId || (newInterval != _lastHoveredInterval))
        {
            auto lock = _terminal->LockForWriting();
            _lastHoveredId = newId;
            _lastHoveredInterval = newInterval;
            _renderEngine->UpdateHyperlinkHoveredId(newId);
            _renderer->UpdateLastHoveredInterval(newInterval);
            _renderer->TriggerRedrawAll();
        }
    }

    // Method Description:
    // - Handle a mouse exited event, specifically clearing last hovered cell
    // and removing selection from hyper link if exists
    // Arguments:
    // - sender: not used
    // - args: event data
    void TermControl::_PointerExitedHandler(Windows::Foundation::IInspectable const& /*sender*/, Windows::UI::Xaml::Input::PointerRoutedEventArgs const& /*e*/)
    {
        _UpdateHoveredCell(std::nullopt);
    }

    // -------------------------------- WinRT Events ---------------------------------
    // Winrt events need a method for adding a callback to the event and removing the callback.
    // These macros will define them both for you.
    DEFINE_EVENT(TermControl, FontSizeChanged, _fontSizeChangedHandlers, Control::FontSizeChangedEventArgs);
    DEFINE_EVENT(TermControl, ScrollPositionChanged, _scrollPositionChangedHandlers, Control::ScrollPositionChangedEventArgs);
}
