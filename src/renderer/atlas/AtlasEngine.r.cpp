// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

#include "BackendD2D.h"
#include "BackendD3D.h"

// #### NOTE ####
// If you see any code in here that contains "_api." you might be seeing a race condition.
// The AtlasEngine::Present() method is called on a background thread without any locks,
// while any of the API methods (like AtlasEngine::Invalidate) might be called concurrently.
// The usage of the _r field is safe as its members are in practice
// only ever written to by the caller of Present() (the "Renderer" class).
// The _api fields on the other hand are concurrently written to by others.

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

#pragma region IRenderEngine

// Present() is called without the console buffer lock being held.
// --> Put as much in here as possible.
[[nodiscard]] HRESULT AtlasEngine::Present() noexcept
try
{
    if (!_p.dxgi.adapter || !_p.dxgi.factory->IsCurrent())
    {
        _recreateAdapter();
    }

    if (!_b)
    {
        _recreateBackend();
    }

    if (_p.swapChain.generation != _p.s.generation())
    {
        _handleSwapChainUpdate();
    }

    _b->Render(_p);
    _present();
    return S_OK;
}
catch (const wil::ResultException& exception)
{
    const auto hr = exception.GetErrorCode();

    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        _p.dxgi = {};
        return E_PENDING;
    }

    if (_p.warningCallback)
    {
        try
        {
            _p.warningCallback(hr);
        }
        CATCH_LOG()
    }

    _b.reset();
    return hr;
}
CATCH_RETURN()

[[nodiscard]] bool AtlasEngine::RequiresContinuousRedraw() noexcept
{
    return ATLAS_DEBUG_CONTINUOUS_REDRAW || (_b && _b->RequiresContinuousRedraw());
}

void AtlasEngine::WaitUntilCanRender() noexcept
{
    if constexpr (ATLAS_DEBUG_RENDER_DELAY)
    {
        Sleep(ATLAS_DEBUG_RENDER_DELAY);
    }
    _waitUntilCanRender();
}

#pragma endregion

void AtlasEngine::_recreateAdapter()
{
#ifndef NDEBUG
    if (IsDebuggerPresent())
    {
        // DXGIGetDebugInterface1 returns E_NOINTERFACE on systems without the Windows SDK installed.
        if (wil::com_ptr<IDXGIInfoQueue> infoQueue; SUCCEEDED_LOG(DXGIGetDebugInterface1(0, IID_PPV_ARGS(infoQueue.addressof()))))
        {
            // I didn't want to link with dxguid.lib just for getting DXGI_DEBUG_ALL. This GUID is publicly documented.
            static constexpr GUID dxgiDebugAll{ 0xe48ae283, 0xda80, 0x490b, { 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x8 } };
            for (const auto severity : { DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO })
            {
                LOG_IF_FAILED(infoQueue->SetBreakOnSeverity(dxgiDebugAll, severity, true));
            }
        }
    }
#endif

#ifndef NDEBUG
    static constexpr UINT flags = DXGI_CREATE_FACTORY_DEBUG;
#else
    static constexpr UINT flags = 0;
#endif

// IID_PPV_ARGS doesn't work here for some reason.
#pragma warning(suppress : 26496) // The variable 'hr' does not change after construction, mark it as const (con.4).
    auto hr = CreateDXGIFactory2(flags, __uuidof(_p.dxgi.factory), _p.dxgi.factory.put_void());

#ifndef NDEBUG
    // This might be due to missing the "Graphics debugger and GPU profiler for
    // DirectX" tools. Just as a sanity check, try again without
    // `DXGI_CREATE_FACTORY_DEBUG`
    if (FAILED(hr))
    {
        hr = CreateDXGIFactory2(0, __uuidof(_p.dxgi.factory), _p.dxgi.factory.put_void());
    }
#endif
    THROW_IF_FAILED(hr);

    wil::com_ptr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 desc{};

    {
        const auto useSoftwareRendering = _p.s->target->useSoftwareRendering;
        UINT index = 0;

        do
        {
            THROW_IF_FAILED(_p.dxgi.factory->EnumAdapters1(index++, adapter.put()));
            THROW_IF_FAILED(adapter->GetDesc1(&desc));

            // If useSoftwareRendering is false we exit during the first iteration. Using the default adapter (index 0)
            // is the right thing to do under most circumstances, unless you _really_ want to get your hands dirty.
            // The alternative is to track the window rectangle in respect to all IDXGIOutputs and select the right
            // IDXGIAdapter, while also considering the "graphics preference" override in the windows settings app, etc.
            //
            // If useSoftwareRendering is true we search until we find the first WARP adapter (usually the last adapter).
        } while (useSoftwareRendering && WI_IsFlagClear(desc.Flags, DXGI_ADAPTER_FLAG_SOFTWARE));
    }

    if (memcmp(&_p.dxgi.adapterLuid, &desc.AdapterLuid, sizeof(LUID)) != 0)
    {
        _p.dxgi.adapter = std::move(adapter);
        _p.dxgi.adapterLuid = desc.AdapterLuid;
        _p.dxgi.adapterFlags = desc.Flags;
        _b.reset();
    }
}

void AtlasEngine::_recreateBackend()
{
    // D3D11 defers the destruction of objects and only one swap chain can be associated with a
    // HWND, IWindow, or composition surface at a time. --> Destroy it while we still have the old device.
    _destroySwapChain();

    auto d2dMode = ATLAS_DEBUG_FORCE_D2D_MODE;
    auto deviceFlags =
        D3D11_CREATE_DEVICE_SINGLETHREADED
#ifndef NDEBUG
        | D3D11_CREATE_DEVICE_DEBUG
#endif
        // This flag prevents the driver from creating a large thread pool for things like shader computations
        // that would be advantageous for games. For us this has only a minimal performance benefit,
        // but comes with a large memory usage overhead. At the time of writing the Nvidia
        // driver launches $cpu_thread_count more worker threads without this flag.
        | D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS
        // Direct2D support.
        | D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    if (WI_IsFlagSet(_p.dxgi.adapterFlags, DXGI_ADAPTER_FLAG_SOFTWARE))
    {
        WI_ClearFlag(deviceFlags, D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
        d2dMode = true;
    }

    wil::com_ptr<ID3D11Device> device0;
    wil::com_ptr<ID3D11DeviceContext> deviceContext0;
    D3D_FEATURE_LEVEL featureLevel{};

    static constexpr std::array featureLevels{
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1,
    };

#pragma warning(suppress : 26496) // The variable 'hr' does not change after construction, mark it as const (con.4).
    auto hr = D3D11CreateDevice(
        /* pAdapter */ _p.dxgi.adapter.get(),
        /* DriverType */ D3D_DRIVER_TYPE_UNKNOWN,
        /* Software */ nullptr,
        /* Flags */ deviceFlags,
        /* pFeatureLevels */ featureLevels.data(),
        /* FeatureLevels */ gsl::narrow_cast<UINT>(featureLevels.size()),
        /* SDKVersion */ D3D11_SDK_VERSION,
        /* ppDevice */ device0.put(),
        /* pFeatureLevel */ &featureLevel,
        /* ppImmediateContext */ deviceContext0.put());

#ifndef NDEBUG
    if (hr == DXGI_ERROR_SDK_COMPONENT_MISSING)
    {
        // This might happen if you don't have "Graphics debugger and GPU
        // profiler for DirectX" installed in VS. We shouln't just explode if
        // you don't though - instead, disable debugging and try again.
        WI_ClearFlag(deviceFlags, D3D11_CREATE_DEVICE_DEBUG);

        hr = D3D11CreateDevice(
            /* pAdapter */ _p.dxgi.adapter.get(),
            /* DriverType */ D3D_DRIVER_TYPE_UNKNOWN,
            /* Software */ nullptr,
            /* Flags */ deviceFlags,
            /* pFeatureLevels */ featureLevels.data(),
            /* FeatureLevels */ gsl::narrow_cast<UINT>(featureLevels.size()),
            /* SDKVersion */ D3D11_SDK_VERSION,
            /* ppDevice */ device0.put(),
            /* pFeatureLevel */ &featureLevel,
            /* ppImmediateContext */ deviceContext0.put());
    }
#endif
    THROW_IF_FAILED(hr);

    auto device = device0.query<ID3D11Device2>();
    auto deviceContext = deviceContext0.query<ID3D11DeviceContext2>();

#ifndef NDEBUG
    if (IsDebuggerPresent())
    {
        if (const auto d3dInfoQueue = device.try_query<ID3D11InfoQueue>())
        {
            for (const auto severity : { D3D11_MESSAGE_SEVERITY_CORRUPTION, D3D11_MESSAGE_SEVERITY_ERROR, D3D11_MESSAGE_SEVERITY_WARNING, D3D11_MESSAGE_SEVERITY_INFO })
            {
                LOG_IF_FAILED(d3dInfoQueue->SetBreakOnSeverity(severity, true));
            }
        }
    }
#endif

    if (featureLevel < D3D_FEATURE_LEVEL_10_0)
    {
        d2dMode = true;
    }
    else if (featureLevel < D3D_FEATURE_LEVEL_11_0)
    {
        D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS options{};
        // I'm assuming if `CheckFeatureSupport` fails, it'll leave `options` untouched which will result in `d2dMode |= true`.
        std::ignore = device->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &options, sizeof(options));
        d2dMode |= !options.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x;
    }

    _p.device = std::move(device);
    _p.deviceContext = std::move(deviceContext);

    if (d2dMode)
    {
        _b = std::make_unique<BackendD2D>();
    }
    else
    {
        _b = std::make_unique<BackendD3D>(_p);
    }

    // !!! NOTE !!!
    // Normally the viewport is indirectly marked as dirty by `AtlasEngine::_handleSettingsUpdate()` whenever
    // the settings change, but the `!_p.dxgi.factory->IsCurrent()` check is not part of the settings change
    // flow and so we have to manually recreate how AtlasEngine.cpp marks viewports as dirty here.
    // This ensures that the backends redraw their entire viewports whenever a new swap chain is created.
    _p.MarkAllAsDirty();
}

void AtlasEngine::_handleSwapChainUpdate()
{
    if (_p.swapChain.targetGeneration != _p.s->target.generation())
    {
        _createSwapChain();
    }
    else if (_p.swapChain.targetSize != _p.s->targetSize)
    {
        _resizeBuffers();
    }

    if (_p.swapChain.fontGeneration != _p.s->font.generation())
    {
        _updateMatrixTransform();
    }

    _p.swapChain.generation = _p.s.generation();
}

static constexpr DXGI_SWAP_CHAIN_FLAG swapChainFlags = ATLAS_DEBUG_DISABLE_FRAME_LATENCY_WAITABLE_OBJECT ? DXGI_SWAP_CHAIN_FLAG{} : DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

void AtlasEngine::_createSwapChain()
{
    _destroySwapChain();

    DXGI_SWAP_CHAIN_DESC1 desc{
        .Width = _p.s->targetSize.x,
        .Height = _p.s->targetSize.y,
        .Format = DXGI_FORMAT_B8G8R8A8_UNORM,
        .SampleDesc = { .Count = 1 },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        // Sometimes up to 2 buffers are locked, for instance during screen capture or when moving the window.
        // 3 buffers seems to guarantee a stable framerate at display frequency at all times.
        .BufferCount = 3,
        .Scaling = DXGI_SCALING_NONE,
        // DXGI_SWAP_EFFECT_FLIP_DISCARD is a mode that was created at a time were display drivers
        // lacked support for Multiplane Overlays (MPO) and were copying buffers was expensive.
        // This allowed DWM to quickly draw overlays (like gamebars) on top of rendered content.
        // With faster GPU memory in general and with support for MPO in particular this isn't
        // really an advantage anymore. Instead DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL allows for a
        // more "intelligent" composition and display updates to occur like Panel Self Refresh
        // (PSR) which requires dirty rectangles (Present1 API) to work correctly.
        .SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL,
        // If our background is opaque we can enable "independent" flips by setting DXGI_ALPHA_MODE_IGNORE.
        // As our swap chain won't have to compose with DWM anymore it reduces the display latency dramatically.
        .AlphaMode = _p.s->target->enableTransparentBackground ? DXGI_ALPHA_MODE_PREMULTIPLIED : DXGI_ALPHA_MODE_IGNORE,
        .Flags = swapChainFlags,
    };

    wil::com_ptr<IDXGISwapChain1> swapChain1;
    wil::unique_handle handle;

    if (_p.s->target->hwnd)
    {
        desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        THROW_IF_FAILED(_p.dxgi.factory->CreateSwapChainForHwnd(_p.device.get(), _p.s->target->hwnd, &desc, nullptr, nullptr, swapChain1.addressof()));
    }
    else
    {
        const auto module = GetModuleHandleW(L"dcomp.dll");
        const auto DCompositionCreateSurfaceHandle = GetProcAddressByFunctionDeclaration(module, DCompositionCreateSurfaceHandle);
        THROW_LAST_ERROR_IF(!DCompositionCreateSurfaceHandle);

        // As per: https://docs.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-dcompositioncreatesurfacehandle
        static constexpr DWORD COMPOSITIONSURFACE_ALL_ACCESS = 0x0003L;
        THROW_IF_FAILED(DCompositionCreateSurfaceHandle(COMPOSITIONSURFACE_ALL_ACCESS, nullptr, handle.addressof()));
        THROW_IF_FAILED(_p.dxgi.factory.query<IDXGIFactoryMedia>()->CreateSwapChainForCompositionSurfaceHandle(_p.device.get(), handle.get(), &desc, nullptr, swapChain1.addressof()));
    }

    _p.swapChain.swapChain = swapChain1.query<IDXGISwapChain2>();
    _p.swapChain.handle = std::move(handle);
    _p.swapChain.frameLatencyWaitableObject.reset(_p.swapChain.swapChain->GetFrameLatencyWaitableObject());
    _p.swapChain.targetGeneration = _p.s->target.generation();
    _p.swapChain.targetSize = _p.s->targetSize;
    _p.swapChain.waitForPresentation = true;

    WaitUntilCanRender();

    if (_p.swapChainChangedCallback)
    {
        try
        {
            _p.swapChainChangedCallback(_p.swapChain.handle.get());
        }
        CATCH_LOG()
    }
}

void AtlasEngine::_destroySwapChain()
{
    if (_p.swapChain.swapChain)
    {
        // D3D11 defers the destruction of objects and only one swap chain can be associated with a
        // HWND, IWindow, or composition surface at a time. --> Force the destruction of all objects.
        _p.swapChain = {};
        if (_b)
        {
            _b->ReleaseResources();
        }
        if (_p.deviceContext)
        {
            _p.deviceContext->ClearState();
            _p.deviceContext->Flush();
        }
    }
}

void AtlasEngine::_resizeBuffers()
{
    _b->ReleaseResources();
    _p.deviceContext->ClearState();

    THROW_IF_FAILED(_p.swapChain.swapChain->ResizeBuffers(0, _p.s->targetSize.x, _p.s->targetSize.y, DXGI_FORMAT_UNKNOWN, swapChainFlags));
    _p.swapChain.targetSize = _p.s->targetSize;
}

void AtlasEngine::_updateMatrixTransform()
{
    if (!_p.s->target->hwnd)
    {
        // XAML's SwapChainPanel combines the worst of both worlds and always applies a transform
        // to the swap chain to make it match the display scale. This undoes the damage.
        const DXGI_MATRIX_3X2_F matrix{
            ._11 = static_cast<f32>(USER_DEFAULT_SCREEN_DPI) / static_cast<f32>(_p.s->font->dpi),
            ._22 = static_cast<f32>(USER_DEFAULT_SCREEN_DPI) / static_cast<f32>(_p.s->font->dpi),
        };
        THROW_IF_FAILED(_p.swapChain.swapChain->SetMatrixTransform(&matrix));
    }
    _p.swapChain.fontGeneration = _p.s->font.generation();
}

void AtlasEngine::_waitUntilCanRender() noexcept
{
    // IDXGISwapChain2::GetFrameLatencyWaitableObject returns an auto-reset event.
    // Once we've waited on the event, waiting on it again will block until the timeout elapses.
    // _waitForPresentation guards against this.
    if constexpr (!ATLAS_DEBUG_DISABLE_FRAME_LATENCY_WAITABLE_OBJECT)
    {
        if (_p.swapChain.waitForPresentation)
        {
            WaitForSingleObjectEx(_p.swapChain.frameLatencyWaitableObject.get(), 100, true);
            _p.swapChain.waitForPresentation = false;
        }
    }
}

void AtlasEngine::_present()
{
    const RECT fullRect{ 0, 0, _p.swapChain.targetSize.x, _p.swapChain.targetSize.y };

    DXGI_PRESENT_PARAMETERS params{};
    RECT scrollRect{};
    POINT scrollOffset{};

    // Since rows might be taller than their cells, they might have drawn outside of the viewport.
    RECT dirtyRect{
        .left = std::max(_p.dirtyRectInPx.left, 0),
        .top = std::max(_p.dirtyRectInPx.top, 0),
        .right = std::min<LONG>(_p.dirtyRectInPx.right, fullRect.right),
        .bottom = std::min<LONG>(_p.dirtyRectInPx.bottom, fullRect.bottom),
    };

    // Present1() dislikes being called with an empty dirty rect.
    if (dirtyRect.left >= dirtyRect.right || dirtyRect.top >= dirtyRect.bottom)
    {
        return;
    }

    if constexpr (!ATLAS_DEBUG_SHOW_DIRTY)
    {
        if (memcmp(&dirtyRect, &fullRect, sizeof(RECT)) != 0)
        {
            params.DirtyRectsCount = 1;
            params.pDirtyRects = &dirtyRect;

            if (_p.scrollOffset)
            {
                const auto offsetInPx = _p.scrollOffset * _p.s->font->cellSize.y;
                const auto width = _p.s->targetSize.x;
                // We don't use targetSize.y here, because "height" refers to the bottom coordinate of the last text row
                // in the buffer. We then add the "offsetInPx" (which is negative when scrolling text upwards) and thus
                // end up with a "bottom" value that is the bottom of the last row of text that we haven't invalidated.
                const auto height = _p.s->viewportCellCount.y * _p.s->font->cellSize.y;
                const auto top = std::max(0, offsetInPx);
                const auto bottom = height + std::min(0, offsetInPx);

                scrollRect = { 0, top, width, bottom };
                scrollOffset = { 0, offsetInPx };

                params.pScrollRect = &scrollRect;
                params.pScrollOffset = &scrollOffset;
            }
        }
    }

    if constexpr (Feature_AtlasEnginePresentFallback::IsEnabled())
    {
        if (FAILED_LOG(_p.swapChain.swapChain->Present1(1, 0, &params)))
        {
            THROW_IF_FAILED(_p.swapChain.swapChain->Present(1, 0));
        }
    }
    else
    {
        THROW_IF_FAILED(_p.swapChain.swapChain->Present1(1, 0, &params));
    }

    _p.swapChain.waitForPresentation = true;
}
