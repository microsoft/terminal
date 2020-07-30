#include "pch.h"
#include <argb.h>
#include <DefaultSettings.h>
#include "HostClass.h"
#include "HostClass.g.cpp"

// #include <wrl.h>
extern std::mutex m;
extern std::condition_variable cv;
extern bool dtored;

using namespace ::Microsoft::Console::Types;
using namespace ::Microsoft::Terminal::Core;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Input;
using namespace winrt::Windows::UI::Xaml::Automation::Peers;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::System;
using namespace winrt::Microsoft::Terminal::Settings;
// using namespace winrt::Windows::ApplicationModel::DataTransfer;

namespace winrt::ScratchWinRTServer::implementation
{
    HostClass::HostClass(const winrt::guid& g) :
        _id{ g },
        _desiredFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8 },
        _actualFont{ DEFAULT_FONT_FACE, 0, DEFAULT_FONT_WEIGHT, { 0, DEFAULT_FONT_SIZE }, CP_UTF8, false }
    {
        _terminal = std::make_unique<::Microsoft::Terminal::Core::Terminal>();
        _settings = winrt::Microsoft::Terminal::Settings::TerminalSettings();

        auto fontSize = _settings.FontSize();
        const auto newSize = std::max<short>(gsl::narrow_cast<short>(fontSize), 1);
        const auto fontFace = _settings.FontFace();
        const auto fontWeight = _settings.FontWeight();
        _actualFont = { fontFace, 0, fontWeight.Weight, { 0, newSize }, CP_UTF8, false };
        _desiredFont = { _actualFont };
    }

    HostClass::~HostClass()
    {
        printf("~HostClass()\n");
        std::unique_lock<std::mutex> lk(m);
        dtored = true;
        cv.notify_one();
    }

    winrt::guid HostClass::Id()
    {
        return _id;
    }

    void HostClass::Attach(Windows::UI::Xaml::Controls::SwapChainPanel panel)
    {
        printf("Hey dummy, we're not using Attach() anymore\n");
    }

    void HostClass::BeginRendering()
    {
        _InitializeTerminal();
    }

    void HostClass::RenderEngineSwapChainChanged()
    {
        // This event is only registered during terminal initialization,
        // so we don't need to check _initializedTerminal.
        // We also don't lock for things that come back from the renderer.
        auto chainHandle = _renderEngine->GetSwapChainHandle();
        auto weakThis{ get_weak() };

        // co_await winrt::resume_foreground(Dispatcher());

        if (auto control{ weakThis.get() })
        {
            _AttachDxgiSwapChainToXaml(chainHandle);
        }
    }

    void HostClass::_AttachDxgiSwapChainToXaml(HANDLE swapChainHandle)
    {
        // NOPE DONT DO THIS
        // auto nativePanel = _panel.as<ISwapChainPanelNative2>();
        // nativePanel->SetSwapChainHandle(swapChainHandle);
    }

    void HostClass::ThisIsInsane(uint64_t swapchainHandle)
    {
        HANDLE foo = (HANDLE)swapchainHandle;
        _hSwapchain.reset(foo);
        // _hSwapchain.put((HANDLE)swapchainHandle);
        // _hSwapchain = (HANDLE)swapchainHandle;
    }

    bool HostClass::_InitializeTerminal()
    {
        { // scope for terminalLock
            auto terminalLock = _terminal->LockForWriting();

            if (_initializedTerminal)
            {
                return false;
            }

            // const auto actualWidth = _panel.ActualWidth();
            // const auto actualHeight = _panel.ActualHeight();
            // const auto windowWidth = actualWidth * _panel.CompositionScaleX(); // Width() and Height() are NaN?
            // const auto windowHeight = actualHeight * _panel.CompositionScaleY();

            const auto actualWidth = 400; //_panel.ActualWidth();
            const auto actualHeight = 150; //_panel.ActualHeight();
            const auto windowWidth = 400.0; // actualWidth * _panel.CompositionScaleX(); // Width() and Height() are NaN?
            const auto windowHeight = 150.0; // actualHeight * _panel.CompositionScaleY();

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

            // _renderer->SetRendererEnteredErrorStateCallback([weakThis = get_weak()]() {
            //     if (auto strongThis{ weakThis.get() })
            //     {
            //         strongThis->_RendererEnteredErrorState();
            //     }
            // });

            THROW_IF_FAILED(localPointerToThread->Initialize(_renderer.get()));

            // Set up the DX Engine
            auto dxEngine = std::make_unique<::Microsoft::Console::Render::DxEngine>();
            dxEngine->_swapChainHandle.swap(_hSwapchain);
            _renderer->AddRenderEngine(dxEngine.get());

            // Initialize our font with the renderer
            // We don't have to care about DPI. We'll get a change message immediately if it's not 96
            // and react accordingly.
            // _UpdateFont(true); // <-- TODO
            _renderer->TriggerFontChange(96, _desiredFont, _actualFont); // "UpdateFont"

            const COORD windowSize{ static_cast<short>(windowWidth), static_cast<short>(windowHeight) };

            // Fist set up the dx engine with the window size in pixels.
            // Then, using the font, get the number of characters that can fit.
            // Resize our terminal connection to match that size, and initialize the terminal with that size.
            const auto viewInPixels = Viewport::FromDimensions({ 0, 0 }, windowSize);
            THROW_IF_FAILED(dxEngine->SetWindowSize({ viewInPixels.Width(), viewInPixels.Height() }));

            // Update DxEngine's SelectionBackground
            // dxEngine->SetSelectionBackground(_settings.SelectionBackground());

            const auto vp = dxEngine->GetViewportInCharacters(viewInPixels);
            const auto width = vp.Width();
            const auto height = vp.Height();
            // _connection.Resize(height, width); // <-- TODO

            // Override the default width and height to match the size of the swapChainPanel
            // _settings.InitialCols(width); // <-- TODO
            // _settings.InitialRows(height); // <-- TODO
            _settings.DefaultBackground(til::color{ 255, 0, 255, 255 }); //rgba
            _settings.DefaultForeground(til::color{ 0, 0, 0, 255 }); //rgba
            _terminal->CreateFromSettings(_settings, renderTarget); // <-- TODO

            dxEngine->SetRetroTerminalEffects(false);
            dxEngine->SetForceFullRepaintRendering(false);
            dxEngine->SetSoftwareRendering(false);

            // // Update DxEngine's AntialiasingMode
            // switch (_settings.AntialiasingMode())
            // {
            // case TextAntialiasingMode::Cleartype:
            //     dxEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
            //     break;
            // case TextAntialiasingMode::Aliased:
            //     dxEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_ALIASED);
            //     break;
            // case TextAntialiasingMode::Grayscale:
            // default:
            //     dxEngine->SetAntialiasingMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            //     break;
            // }

            // // GH#5098: Inform the engine of the opacity of the default text background.
            // if (_settings.UseAcrylic())
            // {
            //     dxEngine->SetDefaultTextBackgroundOpacity(::base::saturated_cast<float>(_settings.TintOpacity()));
            // }

            THROW_IF_FAILED(dxEngine->Enable());
            _renderEngine = std::move(dxEngine);

            // _AttachDxgiSwapChainToXaml(_renderEngine->GetSwapChainHandle());

            // Tell the DX Engine to notify us when the swap chain changes.
            // We do this after we initially set the swapchain so as to avoid unnecessary callbacks (and locking problems)
            _renderEngine->SetCallback(std::bind(&HostClass::RenderEngineSwapChainChanged, this));

            // auto bottom = _terminal->GetViewport().BottomExclusive();
            // auto bufferHeight = bottom;

            // ScrollBar().Maximum(bufferHeight - bufferHeight);
            // ScrollBar().Minimum(0);
            // ScrollBar().Value(0);
            // ScrollBar().ViewportSize(bufferHeight);

            localPointerToThread->EnablePainting();

            // // Set up blinking cursor
            // int blinkTime = GetCaretBlinkTime();
            // if (blinkTime != INFINITE)
            // {
            //     // Create a timer
            //     DispatcherTimer cursorTimer;
            //     cursorTimer.Interval(std::chrono::milliseconds(blinkTime));
            //     cursorTimer.Tick({ get_weak(), &TermControl::_CursorTimerTick });
            //     cursorTimer.Start();
            //     _cursorTimer.emplace(std::move(cursorTimer));
            // }
            // else
            // {
            //     // The user has disabled cursor blinking
            //     _cursorTimer = std::nullopt;
            // }

            // // import value from WinUser (convert from milli-seconds to micro-seconds)
            // _multiClickTimer = GetDoubleClickTime() * 1000;

            // // Focus the control here. If we do it during control initialization, then
            // //      focus won't actually get passed to us. I believe this is because
            // //      we're not technically a part of the UI tree yet, so focusing us
            // //      becomes a no-op.
            // this->Focus(FocusState::Programmatic);

            _initializedTerminal = true;
        } // scope for TerminalLock

        // Start the connection outside of lock, because it could
        // start writing output immediately.
        // _connection.Start(); // <-- TODO

        // Likewise, run the event handlers outside of lock (they could
        // be reentrant)
        // _InitializedHandlers(*this, nullptr);

        _terminal->Write(L"Hello world from another process");
        return true;
    }

}
