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
    if (!_p.dirtyRectInPx)
    {
        return S_OK;
    }

    if (_p.dxgiFactory && !_p.dxgiFactory->IsCurrent())
    {
        _p.dxgiFactory.reset();
        _b.reset();
    }

    if (!_b)
    {
        _recreateBackend();
    }

    _b->Render(_p);
    return S_OK;
}
catch (const wil::ResultException& exception)
{
    const auto hr = exception.GetErrorCode();
    const auto isExpected = hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET || hr == D2DERR_RECREATE_TARGET;

    if (!isExpected && _p.warningCallback)
    {
        try
        {
            _p.warningCallback(hr);
        }
        CATCH_LOG()
    }

    _p.dxgiFactory.reset();
    _b.reset();
    return isExpected ? E_PENDING : hr;
}
CATCH_RETURN()

[[nodiscard]] bool AtlasEngine::RequiresContinuousRedraw() noexcept
{
    return ATLAS_DEBUG_CONTINUOUS_REDRAW || (_b && _b->RequiresContinuousRedraw());
}

void AtlasEngine::WaitUntilCanRender() noexcept
{
    if (_b)
    {
        _b->WaitUntilCanRender();
    }
    if constexpr (ATLAS_DEBUG_RENDER_DELAY)
    {
        Sleep(ATLAS_DEBUG_RENDER_DELAY);
    }
}

#pragma endregion

void AtlasEngine::_recreateBackend()
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

    // Tell the OS that we're resilient to graphics device removal. Docs say:
    // > This function should be called once per process and before any device creation.
    if (const auto module = GetModuleHandleW(L"dxgi.dll"))
    {
        if (const auto func = GetProcAddressByFunctionDeclaration(module, DXGIDeclareAdapterRemovalSupport))
        {
            LOG_IF_FAILED(func());
        }
    }

#ifndef NDEBUG
    static constexpr UINT flags = DXGI_CREATE_FACTORY_DEBUG;
#else
    static constexpr UINT flags = 0;
#endif

    // IID_PPV_ARGS doesn't work here for some reason.
    THROW_IF_FAILED(CreateDXGIFactory2(flags, __uuidof(_p.dxgiFactory), _p.dxgiFactory.put_void()));

    auto d2dMode = ATLAS_DEBUG_FORCE_D2D_MODE;
    auto deviceFlags = D3D11_CREATE_DEVICE_SINGLETHREADED
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

    wil::com_ptr<IDXGIAdapter1> dxgiAdapter;
    {
        const auto useSoftwareRendering = _p.s->target->useSoftwareRendering;
        DXGI_ADAPTER_DESC1 desc{};
        UINT index = 0;

        do
        {
            THROW_IF_FAILED(_p.dxgiFactory->EnumAdapters1(index++, dxgiAdapter.put()));
            THROW_IF_FAILED(dxgiAdapter->GetDesc1(&desc));

            // If useSoftwareRendering is false we exit during the first iteration. Using the default adapter (index 0)
            // is the right thing to do under most circumstances, unless you _really_ want to get your hands dirty.
            // The alternative is to track the window rectangle in respect to all IDXGIOutputs and select the right
            // IDXGIAdapter, while also considering the "graphics preference" override in the windows settings app, etc.
            //
            // If useSoftwareRendering is true we search until we find the first WARP adapter (usually the last adapter).
        } while (useSoftwareRendering && WI_IsFlagClear(desc.Flags, DXGI_ADAPTER_FLAG_SOFTWARE));

        if (WI_IsFlagSet(desc.Flags, DXGI_ADAPTER_FLAG_SOFTWARE))
        {
            WI_ClearFlag(deviceFlags, D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS);
            d2dMode = true;
        }
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

    THROW_IF_FAILED(D3D11CreateDevice(
        /* pAdapter */ dxgiAdapter.get(),
        /* DriverType */ D3D_DRIVER_TYPE_UNKNOWN,
        /* Software */ nullptr,
        /* Flags */ deviceFlags,
        /* pFeatureLevels */ featureLevels.data(),
        /* FeatureLevels */ gsl::narrow_cast<UINT>(featureLevels.size()),
        /* SDKVersion */ D3D11_SDK_VERSION,
        /* ppDevice */ device0.put(),
        /* pFeatureLevel */ &featureLevel,
        /* ppImmediateContext */ deviceContext0.put()));

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

    if (d2dMode)
    {
        _b = std::make_unique<BackendD2D>(std::move(device), std::move(deviceContext));
    }
    else
    {
        _b = std::make_unique<BackendD3D>(std::move(device), std::move(deviceContext));
    }
}
