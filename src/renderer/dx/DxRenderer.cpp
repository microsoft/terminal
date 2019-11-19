// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "DxRenderer.hpp"
#include "CustomTextLayout.h"

#include "../../interactivity/win32/CustomWindowMessages.h"
#include "../../types/inc/Viewport.hpp"
#include "../../inc/unicode.hpp"
#include "../../inc/DefaultSettings.h"
#include <VersionHelpers.h>

#pragma hdrstop

static constexpr float POINTS_PER_INCH = 72.0f;
static constexpr std::wstring_view FALLBACK_FONT_FACE = L"Consolas";
static constexpr std::wstring_view FALLBACK_LOCALE = L"en-us";

using namespace Microsoft::Console::Render;
using namespace Microsoft::Console::Types;

// Routine Description:
// - Constructs a DirectX-based renderer for console text
//   which primarily uses DirectWrite on a Direct2D surface
#pragma warning(suppress : 26455)
// TODO GH 2683: The default constructor should not throw.
DxEngine::DxEngine() :
    RenderEngineBase(),
    _isInvalidUsed{ false },
    _invalidRect{ 0 },
    _invalidScroll{ 0 },
    _presentParams{ 0 },
    _presentReady{ false },
    _presentScroll{ 0 },
    _presentDirty{ 0 },
    _presentOffset{ 0 },
    _isEnabled{ false },
    _isPainting{ false },
    _displaySizePixels{ 0 },
    _foregroundColor{ 0 },
    _backgroundColor{ 0 },
    _selectionBackground{ DEFAULT_FOREGROUND },
    _glyphCell{ 0 },
    _haveDeviceResources{ false },
    _hwndTarget{ static_cast<HWND>(INVALID_HANDLE_VALUE) },
    _sizeTarget{ 0 },
    _dpi{ USER_DEFAULT_SCREEN_DPI },
    _scale{ 1.0f },
    _chainMode{ SwapChainMode::ForComposition },
    _customRenderer{ ::Microsoft::WRL::Make<CustomTextRenderer>() }
{
    THROW_IF_FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&_d2dFactory)));

    THROW_IF_FAILED(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(_dwriteFactory),
        reinterpret_cast<IUnknown**>(_dwriteFactory.GetAddressOf())));
}

// Routine Description:
// - Destroys an instance of the DirectX rendering engine
DxEngine::~DxEngine()
{
    _ReleaseDeviceResources();
}

// Routine Description:
// - Sets this engine to enabled allowing painting and presentation to occur
// Arguments:
// - <none>
// Return Value:
// - Generally S_OK, but might return a DirectX or memory error if
//   resources need to be created or adjusted when enabling to prepare for draw
//   Can give invalid state if you enable an enabled class.
[[nodiscard]] HRESULT DxEngine::Enable() noexcept
{
    return _EnableDisplayAccess(true);
}

// Routine Description:
// - Sets this engine to disabled to prevent painting and presentation from occuring
// Arguments:
// - <none>
// Return Value:
// - Should be OK. We might close/free resources, but that shouldn't error.
//   Can give invalid state if you disable a disabled class.
[[nodiscard]] HRESULT DxEngine::Disable() noexcept
{
    return _EnableDisplayAccess(false);
}

// Routine Description:
// - Helper to enable/disable painting/display access/presentation in a unified
//   manner between enable/disable functions.
// Arguments:
// - outputEnabled - true to enable, false to disable
// Return Value:
// - Generally OK. Can return invalid state if you set to the state that is already
//   active (enabling enabled, disabling disabled).
[[nodiscard]] HRESULT DxEngine::_EnableDisplayAccess(const bool outputEnabled) noexcept
{
    // Invalid state if we're setting it to the same as what we already have.
    RETURN_HR_IF(E_NOT_VALID_STATE, outputEnabled == _isEnabled);

    _isEnabled = outputEnabled;
    if (!_isEnabled)
    {
        _ReleaseDeviceResources();
    }

    return S_OK;
}

// Routine Description;
// - Creates device-specific resources required for drawing
//   which generally means those that are represented on the GPU and can
//   vary based on the monitor, display adapter, etc.
// - These may need to be recreated during the course of painting a frame
//   should something about that hardware pipeline change.
// - Will free device resources that already existed as first operation.
// Arguments:
// - createSwapChain - If true, we create the entire rendering pipeline
//                   - If false, we just set up the adapter.
// Return Value:
// - Could be any DirectX/D3D/D2D/DXGI/DWrite error or memory issue.
[[nodiscard]] HRESULT DxEngine::_CreateDeviceResources(const bool createSwapChain) noexcept
{
    if (_haveDeviceResources)
    {
        _ReleaseDeviceResources();
    }

    auto freeOnFail = wil::scope_exit([&]() noexcept { _ReleaseDeviceResources(); });

    RETURN_IF_FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&_dxgiFactory2)));

    const DWORD DeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT |
                              // clang-format off
// This causes problems for folks who do not have the whole DirectX SDK installed
// when they try to run the rest of the project in debug mode.
// As such, I'm leaving this flag here for people doing DX-specific work to toggle it
// only when they need it and shutting it off otherwise.
// Find out more about the debug layer here:
// https://docs.microsoft.com/en-us/windows/desktop/direct3d11/overviews-direct3d-11-devices-layers
// You can find out how to install it here:
// https://docs.microsoft.com/en-us/windows/uwp/gaming/use-the-directx-runtime-and-visual-studio-graphics-diagnostic-features
                              // clang-format on
                              // D3D11_CREATE_DEVICE_DEBUG |
                              D3D11_CREATE_DEVICE_SINGLETHREADED;

    const std::array<D3D_FEATURE_LEVEL, 5> FeatureLevels{ D3D_FEATURE_LEVEL_11_1,
                                                          D3D_FEATURE_LEVEL_11_0,
                                                          D3D_FEATURE_LEVEL_10_1,
                                                          D3D_FEATURE_LEVEL_10_0,
                                                          D3D_FEATURE_LEVEL_9_1 };

    // Trying hardware first for maximum performance, then trying WARP (software) renderer second
    // in case we're running inside a downlevel VM where hardware passthrough isn't enabled like
    // for Windows 7 in a VM.
    const auto hardwareResult = D3D11CreateDevice(nullptr,
                                                  D3D_DRIVER_TYPE_HARDWARE,
                                                  nullptr,
                                                  DeviceFlags,
                                                  FeatureLevels.data(),
                                                  gsl::narrow_cast<UINT>(FeatureLevels.size()),
                                                  D3D11_SDK_VERSION,
                                                  &_d3dDevice,
                                                  nullptr,
                                                  &_d3dDeviceContext);

    if (FAILED(hardwareResult))
    {
        RETURN_IF_FAILED(D3D11CreateDevice(nullptr,
                                           D3D_DRIVER_TYPE_WARP,
                                           nullptr,
                                           DeviceFlags,
                                           FeatureLevels.data(),
                                           gsl::narrow_cast<UINT>(FeatureLevels.size()),
                                           D3D11_SDK_VERSION,
                                           &_d3dDevice,
                                           nullptr,
                                           &_d3dDeviceContext));
    }

    _displaySizePixels = _GetClientSize();

    if (createSwapChain)
    {
        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc = { 0 };
        SwapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        SwapChainDesc.BufferCount = 2;
        SwapChainDesc.SampleDesc.Count = 1;
        SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        SwapChainDesc.Scaling = DXGI_SCALING_NONE;

        try
        {
            switch (_chainMode)
            {
            case SwapChainMode::ForHwnd:
            {
                // use the HWND's dimensions for the swap chain dimensions.
                RECT rect = { 0 };
                RETURN_IF_WIN32_BOOL_FALSE(GetClientRect(_hwndTarget, &rect));

                SwapChainDesc.Width = rect.right - rect.left;
                SwapChainDesc.Height = rect.bottom - rect.top;

                // We can't do alpha for HWNDs. Set to ignore. It will fail otherwise.
                SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
                const auto createSwapChainResult = _dxgiFactory2->CreateSwapChainForHwnd(_d3dDevice.Get(),
                                                                                         _hwndTarget,
                                                                                         &SwapChainDesc,
                                                                                         nullptr,
                                                                                         nullptr,
                                                                                         &_dxgiSwapChain);
                if (FAILED(createSwapChainResult))
                {
                    SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
                    RETURN_IF_FAILED(_dxgiFactory2->CreateSwapChainForHwnd(_d3dDevice.Get(),
                                                                           _hwndTarget,
                                                                           &SwapChainDesc,
                                                                           nullptr,
                                                                           nullptr,
                                                                           &_dxgiSwapChain));
                }

                break;
            }
            case SwapChainMode::ForComposition:
            {
                // Use the given target size for compositions.
                SwapChainDesc.Width = _displaySizePixels.cx;
                SwapChainDesc.Height = _displaySizePixels.cy;

                // We're doing advanced composition pretty much for the purpose of pretty alpha, so turn it on.
                SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
                // It's 100% required to use scaling mode stretch for composition. There is no other choice.
                SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;

                RETURN_IF_FAILED(_dxgiFactory2->CreateSwapChainForComposition(_d3dDevice.Get(),
                                                                              &SwapChainDesc,
                                                                              nullptr,
                                                                              &_dxgiSwapChain));
                break;
            }
            default:
                THROW_HR(E_NOTIMPL);
            }
        }
        CATCH_RETURN();

        // With a new swap chain, mark the entire thing as invalid.
        RETURN_IF_FAILED(InvalidateAll());

        RETURN_IF_FAILED(_PrepareRenderTarget());
    }

    _haveDeviceResources = true;
    if (_isPainting)
    {
        // TODO: MSFT: 21169176 - remove this or restore the "try a few times to render" code... I think
        _d2dRenderTarget->BeginDraw();
    }

    freeOnFail.release(); // don't need to release if we made it to the bottom and everything was good.

    // Notify that swap chain changed.

    if (_pfn)
    {
        try
        {
            _pfn();
        }
        CATCH_LOG(); // A failure in the notification function isn't a failure to prepare, so just log it and go on.
    }

    return S_OK;
}

[[nodiscard]] HRESULT DxEngine::_PrepareRenderTarget() noexcept
{
    try
    {
        RETURN_IF_FAILED(_dxgiSwapChain->GetBuffer(0, IID_PPV_ARGS(&_dxgiSurface)));

        const D2D1_RENDER_TARGET_PROPERTIES props =
            D2D1::RenderTargetProperties(
                D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED),
                0.0f,
                0.0f);

        RETURN_IF_FAILED(_d2dFactory->CreateDxgiSurfaceRenderTarget(_dxgiSurface.Get(),
                                                                    &props,
                                                                    &_d2dRenderTarget));

        _d2dRenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        RETURN_IF_FAILED(_d2dRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::DarkRed),
                                                                 &_d2dBrushBackground));

        RETURN_IF_FAILED(_d2dRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),
                                                                 &_d2dBrushForeground));

        const D2D1_STROKE_STYLE_PROPERTIES strokeStyleProperties{
            D2D1_CAP_STYLE_SQUARE, // startCap
            D2D1_CAP_STYLE_SQUARE, // endCap
            D2D1_CAP_STYLE_SQUARE, // dashCap
            D2D1_LINE_JOIN_MITER, // lineJoin
            0.f, // miterLimit
            D2D1_DASH_STYLE_SOLID, // dashStyle
            0.f, // dashOffset
        };
        RETURN_IF_FAILED(_d2dFactory->CreateStrokeStyle(&strokeStyleProperties, nullptr, 0, &_strokeStyle));

        // If in composition mode, apply scaling factor matrix
        if (_chainMode == SwapChainMode::ForComposition)
        {
            const auto fdpi = static_cast<float>(_dpi);
            _d2dRenderTarget->SetDpi(fdpi, fdpi);

            DXGI_MATRIX_3X2_F inverseScale = { 0 };
            inverseScale._11 = 1.0f / _scale;
            inverseScale._22 = inverseScale._11;

            ::Microsoft::WRL::ComPtr<IDXGISwapChain2> sc2;
            RETURN_IF_FAILED(_dxgiSwapChain.As(&sc2));

            RETURN_IF_FAILED(sc2->SetMatrixTransform(&inverseScale));
        }
        return S_OK;
    }
    CATCH_RETURN();
}

// Routine Description:
// - Releases device-specific resources (typically held on the GPU)
// Arguments:
// - <none>
// Return Value:
// - <none>
void DxEngine::_ReleaseDeviceResources() noexcept
{
    try
    {
        _haveDeviceResources = false;
        _d2dBrushForeground.Reset();
        _d2dBrushBackground.Reset();

        if (nullptr != _d2dRenderTarget.Get() && _isPainting)
        {
            _d2dRenderTarget->EndDraw();
        }

        _d2dRenderTarget.Reset();

        _dxgiSurface.Reset();
        _dxgiSwapChain.Reset();

        if (nullptr != _d3dDeviceContext.Get())
        {
            // To ensure the swap chain goes away we must unbind any views from the
            // D3D pipeline
            _d3dDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);
        }
        _d3dDeviceContext.Reset();

        _d3dDevice.Reset();

        _dxgiFactory2.Reset();
    }
    CATCH_LOG();
}

// Routine Description:
// - Helper to create a DirectWrite text layout object
//   out of a string.
// Arguments:
// - string - The text to attempt to layout
// - stringLength - Length of string above in characters
// - ppTextLayout - Location to receive new layout object
// Return Value:
// - S_OK if layout created successfully, otherwise a DirectWrite error
[[nodiscard]] HRESULT DxEngine::_CreateTextLayout(
    _In_reads_(stringLength) PCWCHAR string,
    _In_ size_t stringLength,
    _Out_ IDWriteTextLayout** ppTextLayout) noexcept
{
    return _dwriteFactory->CreateTextLayout(string,
                                            gsl::narrow<UINT32>(stringLength),
                                            _dwriteTextFormat.Get(),
                                            gsl::narrow<float>(_displaySizePixels.cx),
                                            _glyphCell.cy != 0 ? _glyphCell.cy : gsl::narrow<float>(_displaySizePixels.cy),
                                            ppTextLayout);
}

// Routine Description:
// - Sets the target window handle for our display pipeline
// - We will take over the surface of this window for drawing
// Arguments:
// - hwnd - Window handle
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::SetHwnd(const HWND hwnd) noexcept
{
    _hwndTarget = hwnd;
    _chainMode = SwapChainMode::ForHwnd;
    return S_OK;
}

[[nodiscard]] HRESULT DxEngine::SetWindowSize(const SIZE Pixels) noexcept
{
    _sizeTarget = Pixels;

    RETURN_IF_FAILED(InvalidateAll());

    return S_OK;
}

void DxEngine::SetCallback(std::function<void()> pfn)
{
    _pfn = pfn;
}

Microsoft::WRL::ComPtr<IDXGISwapChain1> DxEngine::GetSwapChain()
{
    if (_dxgiSwapChain.Get() == nullptr)
    {
        THROW_IF_FAILED(_CreateDeviceResources(true));
    }

    return _dxgiSwapChain;
}

// Routine Description:
// - Invalidates a rectangle described in characters
// Arguments:
// - psrRegion - Character rectangle
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::Invalidate(const SMALL_RECT* const psrRegion) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, psrRegion);

    _InvalidOr(*psrRegion);
    return S_OK;
}

// Routine Description:
// - Invalidates one specific character coordinate
// Arguments:
// - pcoordCursor - single point in the character cell grid
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::InvalidateCursor(const COORD* const pcoordCursor) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pcoordCursor);

    const SMALL_RECT sr = Microsoft::Console::Types::Viewport::FromCoord(*pcoordCursor).ToInclusive();
    return Invalidate(&sr);
}

// Routine Description:
// - Invalidates a rectangle describing a pixel area on the display
// Arguments:
// - prcDirtyClient - pixel rectangle
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::InvalidateSystem(const RECT* const prcDirtyClient) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, prcDirtyClient);

    _InvalidOr(*prcDirtyClient);

    return S_OK;
}

// Routine Description:
// - Invalidates a series of character rectangles
// Arguments:
// - rectangles - One or more rectangles describing character positions on the grid
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept
{
    for (const auto& rect : rectangles)
    {
        RETURN_IF_FAILED(Invalidate(&rect));
    }
    return S_OK;
}

// Routine Description:
// - Scrolls the existing dirty region (if it exists) and
//   invalidates the area that is uncovered in the window.
// Arguments:
// - pcoordDelta - The number of characters to move and uncover.
//               - -Y is up, Y is down, -X is left, X is right.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::InvalidateScroll(const COORD* const pcoordDelta) noexcept
{
    if (pcoordDelta->X != 0 || pcoordDelta->Y != 0)
    {
        try
        {
            POINT delta = { 0 };
            delta.x = pcoordDelta->X * _glyphCell.cx;
            delta.y = pcoordDelta->Y * _glyphCell.cy;

            _InvalidOffset(delta);

            _invalidScroll.cx += delta.x;
            _invalidScroll.cy += delta.y;

            // Add the revealed portion of the screen from the scroll to the invalid area.
            const RECT display = _GetDisplayRect();
            RECT reveal = display;

            // X delta first
            OffsetRect(&reveal, delta.x, 0);
            IntersectRect(&reveal, &reveal, &display);
            SubtractRect(&reveal, &display, &reveal);

            if (!IsRectEmpty(&reveal))
            {
                _InvalidOr(reveal);
            }

            // Y delta second (subtract rect won't work if you move both)
            reveal = display;
            OffsetRect(&reveal, 0, delta.y);
            IntersectRect(&reveal, &reveal, &display);
            SubtractRect(&reveal, &display, &reveal);

            if (!IsRectEmpty(&reveal))
            {
                _InvalidOr(reveal);
            }
        }
        CATCH_RETURN();
    }

    return S_OK;
}

// Routine Description:
// - Invalidates the entire window area
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::InvalidateAll() noexcept
{
    const RECT screen = _GetDisplayRect();
    _InvalidOr(screen);

    return S_OK;
}

// Routine Description:
// - This currently has no effect in this renderer.
// Arguments:
// - pForcePaint - Always filled with false
// Return Value:
// - S_FALSE because we don't use this.
[[nodiscard]] HRESULT DxEngine::InvalidateCircling(_Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);

    *pForcePaint = false;
    return S_FALSE;
}

// Routine Description:
// - Gets the area in pixels of the surface we are targeting
// Arguments:
// - <none>
// Return Value:
// - X by Y area in pixels of the surface
[[nodiscard]] SIZE DxEngine::_GetClientSize() const noexcept
{
    switch (_chainMode)
    {
    case SwapChainMode::ForHwnd:
    {
        RECT clientRect = { 0 };
        LOG_IF_WIN32_BOOL_FALSE(GetClientRect(_hwndTarget, &clientRect));

        SIZE clientSize = { 0 };
        clientSize.cx = clientRect.right - clientRect.left;
        clientSize.cy = clientRect.bottom - clientRect.top;

        return clientSize;
    }
    case SwapChainMode::ForComposition:
    {
        SIZE size = _sizeTarget;
        size.cx = static_cast<LONG>(size.cx * _scale);
        size.cy = static_cast<LONG>(size.cy * _scale);
        return size;
    }
    default:
        FAIL_FAST_HR(E_NOTIMPL);
    }
}

// Routine Description:
// - Helper to multiply all parameters of a rectangle by the font size
//   to convert from characters to pixels.
// Arguments:
// - cellsToPixels - rectangle to update
// - fontSize - scaling factors
// Return Value:
// - <none> - Updates reference
void _ScaleByFont(RECT& cellsToPixels, SIZE fontSize) noexcept
{
    cellsToPixels.left *= fontSize.cx;
    cellsToPixels.right *= fontSize.cx;
    cellsToPixels.top *= fontSize.cy;
    cellsToPixels.bottom *= fontSize.cy;
}

// Routine Description:
// - Retrieves a rectangle representation of the pixel size of the
//   surface we are drawing on
// Arguments:
// - <none>
// Return Value;
// - Origin-placed rectangle representing the pixel size of the surface
[[nodiscard]] RECT DxEngine::_GetDisplayRect() const noexcept
{
    return { 0, 0, _displaySizePixels.cx, _displaySizePixels.cy };
}

// Routine Description:
// - Helper to shift the existing dirty rectangle by a pixel offset
//   and crop it to still be within the bounds of the display surface
// Arguments:
// - delta - Adjustment distance in pixels
//         - -Y is up, Y is down, -X is left, X is right.
// Return Value:
// - <none>
void DxEngine::_InvalidOffset(POINT delta)
{
    if (_isInvalidUsed)
    {
        // Copy the existing invalid rect
        RECT invalidNew = _invalidRect;

        // Offset it to the new position
        THROW_IF_WIN32_BOOL_FALSE(OffsetRect(&invalidNew, delta.x, delta.y));

        // Get the rect representing the display
        const RECT rectScreen = _GetDisplayRect();

        // Ensure that the new invalid rectangle is still on the display
        IntersectRect(&invalidNew, &invalidNew, &rectScreen);

        _invalidRect = invalidNew;
    }
}

// Routine description:
// - Adds the given character rectangle to the total dirty region
// - Will scale internally to pixels based on the current font.
// Arguments:
// - sr - character rectangle
// Return Value:
// - <none>
void DxEngine::_InvalidOr(SMALL_RECT sr) noexcept
{
    RECT region;
    region.left = sr.Left;
    region.top = sr.Top;
    region.right = sr.Right;
    region.bottom = sr.Bottom;
    _ScaleByFont(region, _glyphCell);

    region.right += _glyphCell.cx;
    region.bottom += _glyphCell.cy;

    _InvalidOr(region);
}

// Routine Description:
// - Adds the given pixel rectangle to the total dirty region
// Arguments:
// - rc - Dirty pixel rectangle
// Return Value:
// - <none>
void DxEngine::_InvalidOr(RECT rc) noexcept
{
    if (_isInvalidUsed)
    {
        UnionRect(&_invalidRect, &_invalidRect, &rc);

        const RECT rcScreen = _GetDisplayRect();
        IntersectRect(&_invalidRect, &_invalidRect, &rcScreen);
    }
    else
    {
        _invalidRect = rc;
        _isInvalidUsed = true;
    }
}

// Routine Description:
// - This is unused by this renderer.
// Arguments:
// - pForcePaint - always filled with false.
// Return Value:
// - S_FALSE because this is unused.
[[nodiscard]] HRESULT DxEngine::PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);

    *pForcePaint = false;
    return S_FALSE;
}

// Routine description:
// - Prepares the surfaces for painting and begins a drawing batch
// Arguments:
// - <none>
// Return Value:
// - Any DirectX error, a memory error, etc.
[[nodiscard]] HRESULT DxEngine::StartPaint() noexcept
{
    FAIL_FAST_IF_FAILED(InvalidateAll());
    RETURN_HR_IF(E_NOT_VALID_STATE, _isPainting); // invalid to start a paint while painting.

    if (_isEnabled)
    {
        try
        {
            const auto clientSize = _GetClientSize();
            if (!_haveDeviceResources)
            {
                RETURN_IF_FAILED(_CreateDeviceResources(true));
            }
            else if (_displaySizePixels.cy != clientSize.cy ||
                     _displaySizePixels.cx != clientSize.cx)
            {
                // OK, we're going to play a dangerous game here for the sake of optimizing resize
                // First, set up a complete clear of all device resources if something goes terribly wrong.
                auto resetDeviceResourcesOnFailure = wil::scope_exit([&]() noexcept {
                    _ReleaseDeviceResources();
                });

                // Now let go of a few of the device resources that get in the way of resizing buffers in the swap chain
                _dxgiSurface.Reset();
                _d2dRenderTarget.Reset();

                // Change the buffer size and recreate the render target (and surface)
                RETURN_IF_FAILED(_dxgiSwapChain->ResizeBuffers(2, clientSize.cx, clientSize.cy, DXGI_FORMAT_B8G8R8A8_UNORM, 0));
                RETURN_IF_FAILED(_PrepareRenderTarget());

                // OK we made it past the parts that can cause errors. We can release our failure handler.
                resetDeviceResourcesOnFailure.release();

                // And persist the new size.
                _displaySizePixels = clientSize;
            }

            _d2dRenderTarget->BeginDraw();
            _isPainting = true;
        }
        CATCH_RETURN();
    }

    return S_OK;
}

// Routine Description:
// - Ends batch drawing and captures any state necessary for presentation
// Arguments:
// - <none>
// Return Value:
// - Any DirectX error, a memory error, etc.
[[nodiscard]] HRESULT DxEngine::EndPaint() noexcept
{
    RETURN_HR_IF(E_INVALIDARG, !_isPainting); // invalid to end paint when we're not painting

    HRESULT hr = S_OK;

    if (_haveDeviceResources)
    {
        _isPainting = false;

        hr = _d2dRenderTarget->EndDraw();

        if (SUCCEEDED(hr))
        {
            if (_invalidScroll.cy != 0 || _invalidScroll.cx != 0)
            {
                _presentDirty = _invalidRect;

                const RECT display = _GetDisplayRect();
                SubtractRect(&_presentScroll, &display, &_presentDirty);
                _presentOffset.x = _invalidScroll.cx;
                _presentOffset.y = _invalidScroll.cy;

                _presentParams.DirtyRectsCount = 1;
                _presentParams.pDirtyRects = &_presentDirty;

                _presentParams.pScrollOffset = &_presentOffset;
                _presentParams.pScrollRect = &_presentScroll;

                if (IsRectEmpty(&_presentScroll))
                {
                    _presentParams.pScrollRect = nullptr;
                    _presentParams.pScrollOffset = nullptr;
                }
            }

            _presentReady = true;
        }
        else
        {
            _presentReady = false;
            _ReleaseDeviceResources();
        }
    }

    _invalidRect = { 0 };
    _isInvalidUsed = false;

    _invalidScroll = { 0 };

    return hr;
}

// Routine Description:
// - Copies the front surface of the swap chain (the one being displayed)
//   to the back surface of the swap chain (the one we draw on next)
//   so we can draw on top of what's already there.
// Arguments:
// - <none>
// Return Value:
// - Any DirectX error, a memory error, etc.
[[nodiscard]] HRESULT DxEngine::_CopyFrontToBack() noexcept
{
    try
    {
        Microsoft::WRL::ComPtr<ID3D11Resource> backBuffer;
        Microsoft::WRL::ComPtr<ID3D11Resource> frontBuffer;

        RETURN_IF_FAILED(_dxgiSwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer)));
        RETURN_IF_FAILED(_dxgiSwapChain->GetBuffer(1, IID_PPV_ARGS(&frontBuffer)));

        _d3dDeviceContext->CopyResource(backBuffer.Get(), frontBuffer.Get());
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Takes queued drawing information and presents it to the screen.
// - This is separated out so it can be done outside the lock as it's expensive.
// Arguments:
// - <none>
// Return Value:
// - S_OK on success, E_PENDING to indicate a retry or a relevant DirectX error
[[nodiscard]] HRESULT DxEngine::Present() noexcept
{
    if (_presentReady)
    {
        try
        {
            HRESULT hr = S_OK;

            hr = _dxgiSwapChain->Present(1, 0);
            /*hr = _dxgiSwapChain->Present1(1, 0, &_presentParams);*/

            if (FAILED(hr))
            {
                // These two error codes are indicated for destroy-and-recreate
                if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
                {
                    // We don't need to end painting here, as the renderer has done it for us.
                    _ReleaseDeviceResources();
                    FAIL_FAST_IF_FAILED(InvalidateAll());
                    return E_PENDING; // Indicate a retry to the renderer.
                }

                FAIL_FAST_HR(hr);
            }

            RETURN_IF_FAILED(_CopyFrontToBack());
            _presentReady = false;

            _presentDirty = { 0 };
            _presentOffset = { 0 };
            _presentScroll = { 0 };
            _presentParams = { 0 };
        }
        CATCH_RETURN();
    }

    return S_OK;
}

// Routine Description:
// - This is currently unused.
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::ScrollFrame() noexcept
{
    return S_OK;
}

// Routine Description:
// - This paints in the back most layer of the frame with the background color.
// Arguments:
// - <none>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::PaintBackground() noexcept
{
    switch (_chainMode)
    {
    case SwapChainMode::ForHwnd:
        _d2dRenderTarget->FillRectangle(D2D1::RectF(static_cast<float>(_invalidRect.left),
                                                    static_cast<float>(_invalidRect.top),
                                                    static_cast<float>(_invalidRect.right),
                                                    static_cast<float>(_invalidRect.bottom)),
                                        _d2dBrushBackground.Get());
        break;
    case SwapChainMode::ForComposition:
        D2D1_COLOR_F nothing = { 0 };

        _d2dRenderTarget->Clear(nothing);
        break;
    }

    return S_OK;
}

// Routine Description:
// - Places one line of text onto the screen at the given position
// Arguments:
// - clusters - Iterable collection of cluster information (text and columns it should consume)
// - coord - Character coordinate position in the cell grid
// - fTrimLeft - Whether or not to trim off the left half of a double wide character
// Return Value:
// - S_OK or relevant DirectX error
[[nodiscard]] HRESULT DxEngine::PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                                COORD const coord,
                                                const bool /*trimLeft*/) noexcept
{
    try
    {
        // Calculate positioning of our origin.
        D2D1_POINT_2F origin;
        origin.x = static_cast<float>(coord.X * _glyphCell.cx);
        origin.y = static_cast<float>(coord.Y * _glyphCell.cy);

        // Create the text layout
        CustomTextLayout layout(_dwriteFactory.Get(),
                                _dwriteTextAnalyzer.Get(),
                                _dwriteTextFormat.Get(),
                                _dwriteFontFace.Get(),
                                clusters,
                                _glyphCell.cx);

        // Get the baseline for this font as that's where we draw from
        DWRITE_LINE_SPACING spacing;
        RETURN_IF_FAILED(_dwriteTextFormat->GetLineSpacing(&spacing.method, &spacing.height, &spacing.baseline));

        // Assemble the drawing context information
        DrawingContext context(_d2dRenderTarget.Get(),
                               _d2dBrushForeground.Get(),
                               _d2dBrushBackground.Get(),
                               _dwriteFactory.Get(),
                               spacing,
                               D2D1::SizeF(gsl::narrow<FLOAT>(_glyphCell.cx), gsl::narrow<FLOAT>(_glyphCell.cy)),
                               D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

        // Layout then render the text
        RETURN_IF_FAILED(layout.Draw(&context, _customRenderer.Get(), origin.x, origin.y));
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Paints lines around cells (draws in pieces of the grid)
// Arguments:
// - lines - Which grid lines (top, left, bottom, right) to draw
// - color - The color to use for drawing the lines
// - cchLine - Length of the line to draw in character cells
// - coordTarget - The X,Y character position in the grid where we should start drawing
//               - We will draw rightward (+X) from here
// Return Value:
// - S_OK or relevant DirectX error
[[nodiscard]] HRESULT DxEngine::PaintBufferGridLines(GridLines const lines,
                                                     COLORREF const color,
                                                     size_t const cchLine,
                                                     COORD const coordTarget) noexcept
{
    const auto existingColor = _d2dBrushForeground->GetColor();
    const auto restoreBrushOnExit = wil::scope_exit([&]() noexcept { _d2dBrushForeground->SetColor(existingColor); });

    _d2dBrushForeground->SetColor(_ColorFFromColorRef(color));

    const auto font = _GetFontSize();
    D2D_POINT_2F target;
    target.x = static_cast<float>(coordTarget.X) * font.X;
    target.y = static_cast<float>(coordTarget.Y) * font.Y;

    D2D_POINT_2F start = { 0 };
    D2D_POINT_2F end = { 0 };

    for (size_t i = 0; i < cchLine; i++)
    {
        // 0.5 pixel offset for crisp lines
        start = { target.x + 0.5f, target.y + 0.5f };

        if (lines & GridLines::Top)
        {
            end = start;
            end.x += font.X;

            _d2dRenderTarget->DrawLine(start, end, _d2dBrushForeground.Get(), 1.0f, _strokeStyle.Get());
        }

        if (lines & GridLines::Left)
        {
            end = start;
            end.y += font.Y;

            _d2dRenderTarget->DrawLine(start, end, _d2dBrushForeground.Get(), 1.0f, _strokeStyle.Get());
        }

        // NOTE: Watch out for inclusive/exclusive rectangles here.
        // We have to remove 1 from the font size for the bottom and right lines to ensure that the
        // starting point remains within the clipping rectangle.
        // For example, if we're drawing a letter at 0,0 and the font size is 8x16....
        // The bottom left corner inclusive is at 0,15 which is Y (0) + Font Height (16) - 1 = 15.
        // The top right corner inclusive is at 7,0 which is X (0) + Font Height (8) - 1 = 7.

        // 0.5 pixel offset for crisp lines; -0.5 on the Y to fit _in_ the cell, not outside it.
        start = { target.x + 0.5f, target.y + font.Y - 0.5f };

        if (lines & GridLines::Bottom)
        {
            end = start;
            end.x += font.X - 1.f;

            _d2dRenderTarget->DrawLine(start, end, _d2dBrushForeground.Get(), 1.0f, _strokeStyle.Get());
        }

        start = { target.x + font.X - 0.5f, target.y + 0.5f };

        if (lines & GridLines::Right)
        {
            end = start;
            end.y += font.Y - 1.f;

            _d2dRenderTarget->DrawLine(start, end, _d2dBrushForeground.Get(), 1.0f, _strokeStyle.Get());
        }

        // Move to the next character in this run.
        target.x += font.X;
    }

    return S_OK;
}

// Routine Description:
// - Paints an overlay highlight on a portion of the frame to represent selected text
// Arguments:
//  - rect - Rectangle to invert or highlight to make the selection area
// Return Value:
// - S_OK or relevant DirectX error.
[[nodiscard]] HRESULT DxEngine::PaintSelection(const SMALL_RECT rect) noexcept
{
    const auto existingColor = _d2dBrushForeground->GetColor();

    _d2dBrushForeground->SetColor(_selectionBackground);
    const auto resetColorOnExit = wil::scope_exit([&]() noexcept { _d2dBrushForeground->SetColor(existingColor); });

    RECT pixels;
    pixels.left = rect.Left * _glyphCell.cx;
    pixels.top = rect.Top * _glyphCell.cy;
    pixels.right = rect.Right * _glyphCell.cx;
    pixels.bottom = rect.Bottom * _glyphCell.cy;

    D2D1_RECT_F draw = { 0 };
    draw.left = static_cast<float>(pixels.left);
    draw.top = static_cast<float>(pixels.top);
    draw.right = static_cast<float>(pixels.right);
    draw.bottom = static_cast<float>(pixels.bottom);

    _d2dRenderTarget->FillRectangle(draw, _d2dBrushForeground.Get());

    return S_OK;
}

// Helper to choose which Direct2D method to use when drawing the cursor rectangle
enum class CursorPaintType
{
    Fill,
    Outline
};

// Routine Description:
// - Draws a block at the given position to represent the cursor
// - May be a styled cursor at the character cell location that is less than a full block
// Arguments:
// - options - Packed options relevant to how to draw the cursor
// Return Value:
// - S_OK or relevant DirectX error.
[[nodiscard]] HRESULT DxEngine::PaintCursor(const IRenderEngine::CursorOptions& options) noexcept
{
    // if the cursor is off, do nothing - it should not be visible.
    if (!options.isOn)
    {
        return S_FALSE;
    }
    // Create rectangular block representing where the cursor can fill.
    D2D1_RECT_F rect = { 0 };
    rect.left = static_cast<float>(options.coordCursor.X * _glyphCell.cx);
    rect.top = static_cast<float>(options.coordCursor.Y * _glyphCell.cy);
    rect.right = static_cast<float>(rect.left + _glyphCell.cx);
    rect.bottom = static_cast<float>(rect.top + _glyphCell.cy);

    // If we're double-width, make it one extra glyph wider
    if (options.fIsDoubleWidth)
    {
        rect.right += _glyphCell.cx;
    }

    CursorPaintType paintType = CursorPaintType::Fill;

    switch (options.cursorType)
    {
    case CursorType::Legacy:
    {
        // Enforce min/max cursor height
        ULONG ulHeight = std::clamp(options.ulCursorHeightPercent, s_ulMinCursorHeightPercent, s_ulMaxCursorHeightPercent);

        ulHeight = gsl::narrow<ULONG>((_glyphCell.cy * ulHeight) / 100);
        rect.top = rect.bottom - ulHeight;
        break;
    }
    case CursorType::VerticalBar:
    {
        // It can't be wider than one cell or we'll have problems in invalidation, so restrict here.
        // It's either the left + the proposed width from the ease of access setting, or
        // it's the right edge of the block cursor as a maximum.
        rect.right = std::min(rect.right, rect.left + options.cursorPixelWidth);
        break;
    }
    case CursorType::Underscore:
    {
        rect.top = rect.bottom - 1;
        break;
    }
    case CursorType::EmptyBox:
    {
        paintType = CursorPaintType::Outline;
        break;
    }
    case CursorType::FullBox:
    {
        break;
    }
    default:
        return E_NOTIMPL;
    }

    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> brush = _d2dBrushForeground;

    if (options.fUseColor)
    {
        // Make sure to make the cursor opaque
        RETURN_IF_FAILED(_d2dRenderTarget->CreateSolidColorBrush(_ColorFFromColorRef(OPACITY_OPAQUE | options.cursorColor), &brush));
    }

    switch (paintType)
    {
    case CursorPaintType::Fill:
    {
        _d2dRenderTarget->FillRectangle(rect, brush.Get());
        break;
    }
    case CursorPaintType::Outline:
    {
        // DrawRectangle in straddles physical pixels in an attempt to draw a line
        // between them. To avoid this, bump the rectangle around by half the stroke width.
        rect.top += 0.5f;
        rect.left += 0.5f;
        rect.bottom -= 0.5f;
        rect.right -= 0.5f;

        _d2dRenderTarget->DrawRectangle(rect, brush.Get());
        break;
    }
    default:
        return E_NOTIMPL;
    }

    return S_OK;
}

// Routine Description:
// - Updates the default brush colors used for drawing
// Arguments:
// - colorForeground - Foreground brush color
// - colorBackground - Background brush color
// - legacyColorAttribute - <unused>
// - extendedAttrs - <unused>
// - isSettingDefaultBrushes - Lets us know that these are the default brushes to paint the swapchain background or selection
// Return Value:
// - S_OK or relevant DirectX error.
[[nodiscard]] HRESULT DxEngine::UpdateDrawingBrushes(COLORREF const colorForeground,
                                                     COLORREF const colorBackground,
                                                     const WORD /*legacyColorAttribute*/,
                                                     const ExtendedAttributes /*extendedAttrs*/,
                                                     bool const isSettingDefaultBrushes) noexcept
{
    _foregroundColor = _ColorFFromColorRef(colorForeground);
    _backgroundColor = _ColorFFromColorRef(colorBackground);

    _d2dBrushForeground->SetColor(_foregroundColor);
    _d2dBrushBackground->SetColor(_backgroundColor);

    // If this flag is set, then we need to update the default brushes too and the swap chain background.
    if (isSettingDefaultBrushes)
    {
        _defaultForegroundColor = _foregroundColor;
        _defaultBackgroundColor = _backgroundColor;

        // If we have a swap chain, set the background color there too so the area
        // outside the chain on a resize can be filled in with an appropriate color value.
        /*if (_dxgiSwapChain)
        {
            const auto dxgiColor = s_RgbaFromColorF(_defaultBackgroundColor);
            RETURN_IF_FAILED(_dxgiSwapChain->SetBackgroundColor(&dxgiColor));
        }*/
    }

    return S_OK;
}

// Routine Description:
// - Updates the font used for drawing
// Arguments:
// - pfiFontInfoDesired - Information specifying the font that is requested
// - fiFontInfo - Filled with the nearest font actually chosen for drawing
// Return Value:
// - S_OK or relevant DirectX error
[[nodiscard]] HRESULT DxEngine::UpdateFont(const FontInfoDesired& pfiFontInfoDesired, FontInfo& fiFontInfo) noexcept
{
    RETURN_IF_FAILED(_GetProposedFont(pfiFontInfoDesired,
                                      fiFontInfo,
                                      _dpi,
                                      _dwriteTextFormat,
                                      _dwriteTextAnalyzer,
                                      _dwriteFontFace));

    try
    {
        const auto size = fiFontInfo.GetSize();

        _glyphCell.cx = size.X;
        _glyphCell.cy = size.Y;
    }
    CATCH_RETURN();

    return S_OK;
}

[[nodiscard]] Viewport DxEngine::GetViewportInCharacters(const Viewport& viewInPixels) noexcept
{
    const short widthInChars = gsl::narrow_cast<short>(viewInPixels.Width() / _glyphCell.cx);
    const short heightInChars = gsl::narrow_cast<short>(viewInPixels.Height() / _glyphCell.cy);

    return Viewport::FromDimensions(viewInPixels.Origin(), { widthInChars, heightInChars });
}

// Routine Description:
// - Sets the DPI in this renderer
// Arguments:
// - iDpi - DPI
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::UpdateDpi(int const iDpi) noexcept
{
    _dpi = iDpi;

    // The scale factor may be necessary for composition contexts, so save it once here.
    _scale = _dpi / static_cast<float>(USER_DEFAULT_SCREEN_DPI);

    RETURN_IF_FAILED(InvalidateAll());

    return S_OK;
}

// Method Description:
// - Get the current scale factor of this renderer. The actual DPI the renderer
//   is USER_DEFAULT_SCREEN_DPI * GetScaling()
// Arguments:
// - <none>
// Return Value:
// - the scaling multiplier of this render engine
float DxEngine::GetScaling() const noexcept
{
    return _scale;
}

// Method Description:
// - This method will update our internal reference for how big the viewport is.
//      Does nothing for DX.
// Arguments:
// - srNewViewport - The bounds of the new viewport.
// Return Value:
// - HRESULT S_OK
[[nodiscard]] HRESULT DxEngine::UpdateViewport(const SMALL_RECT /*srNewViewport*/) noexcept
{
    return S_OK;
}

// Routine Description:
// - Currently unused by this renderer
// Arguments:
// - pfiFontInfoDesired - <unused>
// - pfiFontInfo - <unused>
// - iDpi - <unused>
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::GetProposedFont(const FontInfoDesired& pfiFontInfoDesired,
                                                FontInfo& pfiFontInfo,
                                                int const iDpi) noexcept
{
    Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
    Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1> analyzer;
    Microsoft::WRL::ComPtr<IDWriteFontFace1> face;

    return _GetProposedFont(pfiFontInfoDesired,
                            pfiFontInfo,
                            iDpi,
                            format,
                            analyzer,
                            face);
}

// Routine Description:
// - Gets the area that we currently believe is dirty within the character cell grid
// Arguments:
// - <none>
// Return Value:
// - Rectangle describing dirty area in characters.
[[nodiscard]] SMALL_RECT DxEngine::GetDirtyRectInChars() noexcept
{
    SMALL_RECT r;
    r.Top = gsl::narrow<SHORT>(floor(_invalidRect.top / _glyphCell.cy));
    r.Left = gsl::narrow<SHORT>(floor(_invalidRect.left / _glyphCell.cx));
    r.Bottom = gsl::narrow<SHORT>(floor(_invalidRect.bottom / _glyphCell.cy));
    r.Right = gsl::narrow<SHORT>(floor(_invalidRect.right / _glyphCell.cx));

    // Exclusive to inclusive
    r.Bottom--;
    r.Right--;

    return r;
}

// Routine Description:
// - Gets COORD packed with shorts of each glyph (character) cell's
//   height and width.
// Arguments:
// - <none>
// Return Value:
// - Nearest integer short x and y values for each cell.
[[nodiscard]] COORD DxEngine::_GetFontSize() const noexcept
{
    return { gsl::narrow<SHORT>(_glyphCell.cx), gsl::narrow<SHORT>(_glyphCell.cy) };
}

// Routine Description:
// - Gets the current font size
// Arguments:
// - pFontSize - Filled with the font size.
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::GetFontSize(_Out_ COORD* const pFontSize) noexcept
{
    *pFontSize = _GetFontSize();
    return S_OK;
}

// Routine Description:
// - Currently unused by this renderer.
// Arguments:
// - glyph - The glyph run to process for column width.
// - pResult - True if it should take two columns. False if it should take one.
// Return Value:
// - S_OK or relevant DirectWrite error.
[[nodiscard]] HRESULT DxEngine::IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pResult);

    try
    {
        const Cluster cluster(glyph, 0); // columns don't matter, we're doing analysis not layout.

        // Create the text layout
        CustomTextLayout layout(_dwriteFactory.Get(),
                                _dwriteTextAnalyzer.Get(),
                                _dwriteTextFormat.Get(),
                                _dwriteFontFace.Get(),
                                { &cluster, 1 },
                                _glyphCell.cx);

        UINT32 columns = 0;
        RETURN_IF_FAILED(layout.GetColumns(&columns));

        *pResult = columns != 1;
    }
    CATCH_RETURN();

    return S_OK;
}

// Method Description:
// - Updates the window's title string.
// Arguments:
// - newTitle: the new string to use for the title of the window
// Return Value:
// - S_OK
[[nodiscard]] HRESULT DxEngine::_DoUpdateTitle(_In_ const std::wstring& /*newTitle*/) noexcept
{
    return PostMessageW(_hwndTarget, CM_UPDATE_TITLE, 0, 0) ? S_OK : E_FAIL;
}

// Routine Description:
// - Attempts to locate the font given, but then begins falling back if we cannot find it.
// - We'll try to fall back to Consolas with the given weight/stretch/style first,
//   then try Consolas again with normal weight/stretch/style,
//   and if nothing works, then we'll throw an error.
// Arguments:
// - familyName - The font name we should be looking for
// - weight - The weight (bold, light, etc.)
// - stretch - The stretch of the font is the spacing between each letter
// - style - Normal, italic, etc.
// Return Value:
// - Smart pointer holding interface reference for queryable font data.
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxEngine::_ResolveFontFaceWithFallback(std::wstring& familyName,
                                                                                              DWRITE_FONT_WEIGHT& weight,
                                                                                              DWRITE_FONT_STRETCH& stretch,
                                                                                              DWRITE_FONT_STYLE& style,
                                                                                              std::wstring& localeName) const
{
    auto face = _FindFontFace(familyName, weight, stretch, style, localeName);

    if (!face)
    {
        familyName = FALLBACK_FONT_FACE;
        face = _FindFontFace(familyName, weight, stretch, style, localeName);
    }

    if (!face)
    {
        familyName = FALLBACK_FONT_FACE;
        weight = DWRITE_FONT_WEIGHT_NORMAL;
        stretch = DWRITE_FONT_STRETCH_NORMAL;
        style = DWRITE_FONT_STYLE_NORMAL;
        face = _FindFontFace(familyName, weight, stretch, style, localeName);
    }

    THROW_IF_NULL_ALLOC(face);

    return face;
}

// Routine Description:
// - Locates a suitable font face from the given information
// Arguments:
// - familyName - The font name we should be looking for
// - weight - The weight (bold, light, etc.)
// - stretch - The stretch of the font is the spacing between each letter
// - style - Normal, italic, etc.
// Return Value:
// - Smart pointer holding interface reference for queryable font data.
[[nodiscard]] Microsoft::WRL::ComPtr<IDWriteFontFace1> DxEngine::_FindFontFace(std::wstring& familyName,
                                                                               DWRITE_FONT_WEIGHT& weight,
                                                                               DWRITE_FONT_STRETCH& stretch,
                                                                               DWRITE_FONT_STYLE& style,
                                                                               std::wstring& localeName) const
{
    Microsoft::WRL::ComPtr<IDWriteFontFace1> fontFace;

    Microsoft::WRL::ComPtr<IDWriteFontCollection> fontCollection;
    THROW_IF_FAILED(_dwriteFactory->GetSystemFontCollection(&fontCollection, false));

    UINT32 familyIndex;
    BOOL familyExists;
    THROW_IF_FAILED(fontCollection->FindFamilyName(familyName.data(), &familyIndex, &familyExists));

    if (familyExists)
    {
        Microsoft::WRL::ComPtr<IDWriteFontFamily> fontFamily;
        THROW_IF_FAILED(fontCollection->GetFontFamily(familyIndex, &fontFamily));

        Microsoft::WRL::ComPtr<IDWriteFont> font;
        THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(weight, stretch, style, &font));

        Microsoft::WRL::ComPtr<IDWriteFontFace> fontFace0;
        THROW_IF_FAILED(font->CreateFontFace(&fontFace0));

        THROW_IF_FAILED(fontFace0.As(&fontFace));

        // Retrieve metrics in case the font we created was different than what was requested.
        weight = font->GetWeight();
        stretch = font->GetStretch();
        style = font->GetStyle();

        // Dig the family name out at the end to return it.
        familyName = _GetFontFamilyName(fontFamily.Get(), localeName);
    }

    return fontFace;
}

// Routine Description:
// - Helper to retrieve the user's locale preference or fallback to the default.
// Arguments:
// - <none>
// Return Value:
// - A locale that can be used on construction of assorted DX objects that want to know one.
[[nodiscard]] std::wstring DxEngine::_GetLocaleName() const
{
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> localeName;

    const auto returnCode = GetUserDefaultLocaleName(localeName.data(), gsl::narrow<int>(localeName.size()));
    if (returnCode)
    {
        return { localeName.data() };
    }
    else
    {
        return { FALLBACK_LOCALE.data(), FALLBACK_LOCALE.size() };
    }
}

// Routine Description:
// - Retrieves the font family name out of the given object in the given locale.
// - If we can't find a valid name for the given locale, we'll fallback and report it back.
// Arguments:
// - fontFamily - DirectWrite font family object
// - localeName - The locale in which the name should be retrieved.
//              - If fallback occurred, this is updated to what we retrieved instead.
// Return Value:
// - Localized string name of the font family
[[nodiscard]] std::wstring DxEngine::_GetFontFamilyName(gsl::not_null<IDWriteFontFamily*> const fontFamily,
                                                        std::wstring& localeName) const
{
    // See: https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritefontcollection
    Microsoft::WRL::ComPtr<IDWriteLocalizedStrings> familyNames;
    THROW_IF_FAILED(fontFamily->GetFamilyNames(&familyNames));

    // First we have to find the right family name for the locale. We're going to bias toward what the caller
    // requested, but fallback if we need to and reply with the locale we ended up choosing.
    UINT32 index = 0;
    BOOL exists = false;

    // This returns S_OK whether or not it finds a locale name. Check exists field instead.
    // If it returns an error, it's a real problem, not an absence of this locale name.
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-findlocalename
    THROW_IF_FAILED(familyNames->FindLocaleName(localeName.data(), &index, &exists));

    // If we tried and it still doesn't exist, try with the fallback locale.
    if (!exists)
    {
        localeName = FALLBACK_LOCALE;
        THROW_IF_FAILED(familyNames->FindLocaleName(localeName.data(), &index, &exists));
    }

    // If it still doesn't exist, we're going to try index 0.
    if (!exists)
    {
        index = 0;

        // Get the locale name out so at least the caller knows what locale this name goes with.
        UINT32 length = 0;
        THROW_IF_FAILED(familyNames->GetLocaleNameLength(index, &length));
        localeName.resize(length);

        // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getlocalenamelength
        // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getlocalename
        // GetLocaleNameLength does not include space for null terminator, but GetLocaleName needs it so add one.
        THROW_IF_FAILED(familyNames->GetLocaleName(index, localeName.data(), length + 1));
    }

    // OK, now that we've decided which family name and the locale that it's in... let's go get it.
    UINT32 length = 0;
    THROW_IF_FAILED(familyNames->GetStringLength(index, &length));

    // Make our output buffer and resize it so it is allocated.
    std::wstring retVal;
    retVal.resize(length);

    // FINALLY, go fetch the string name.
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getstringlength
    // https://docs.microsoft.com/en-us/windows/win32/api/dwrite/nf-dwrite-idwritelocalizedstrings-getstring
    // Once again, GetStringLength is without the null, but GetString needs the null. So add one.
    THROW_IF_FAILED(familyNames->GetString(index, retVal.data(), length + 1));

    // and return it.
    return retVal;
}

// Routine Description:
// - Updates the font used for drawing
// Arguments:
// - desired - Information specifying the font that is requested
// - actual - Filled with the nearest font actually chosen for drawing
// - dpi - The DPI of the screen
// Return Value:
// - S_OK or relevant DirectX error
[[nodiscard]] HRESULT DxEngine::_GetProposedFont(const FontInfoDesired& desired,
                                                 FontInfo& actual,
                                                 const int dpi,
                                                 Microsoft::WRL::ComPtr<IDWriteTextFormat>& textFormat,
                                                 Microsoft::WRL::ComPtr<IDWriteTextAnalyzer1>& textAnalyzer,
                                                 Microsoft::WRL::ComPtr<IDWriteFontFace1>& fontFace) const noexcept
{
    try
    {
        std::wstring fontName(desired.GetFaceName());
        DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
        DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
        DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
        std::wstring localeName = _GetLocaleName();

        const auto face = _ResolveFontFaceWithFallback(fontName, weight, stretch, style, localeName);

        DWRITE_FONT_METRICS1 fontMetrics;
        face->GetMetrics(&fontMetrics);

        const UINT32 spaceCodePoint = UNICODE_SPACE;
        UINT16 spaceGlyphIndex;
        THROW_IF_FAILED(face->GetGlyphIndicesW(&spaceCodePoint, 1, &spaceGlyphIndex));

        INT32 advanceInDesignUnits;
        THROW_IF_FAILED(face->GetDesignGlyphAdvances(1, &spaceGlyphIndex, &advanceInDesignUnits));

        // The math here is actually:
        // Requested Size in Points * DPI scaling factor * Points to Pixels scaling factor.
        // - DPI = dots per inch
        // - PPI = points per inch or "points" as usually seen when choosing a font size
        // - The DPI scaling factor is the current monitor DPI divided by 96, the default DPI.
        // - The Points to Pixels factor is based on the typography definition of 72 points per inch.
        //    As such, converting requires taking the 96 pixel per inch default and dividing by the 72 points per inch
        //    to get a factor of 1 and 1/3.
        // This turns into something like:
        // - 12 ppi font * (96 dpi / 96 dpi) * (96 dpi / 72 points per inch) = 16 pixels tall font for 100% display (96 dpi is 100%)
        // - 12 ppi font * (144 dpi / 96 dpi) * (96 dpi / 72 points per inch) = 24 pixels tall font for 150% display (144 dpi is 150%)
        // - 12 ppi font * (192 dpi / 96 dpi) * (96 dpi / 72 points per inch) = 32 pixels tall font for 200% display (192 dpi is 200%)
        float heightDesired = static_cast<float>(desired.GetEngineSize().Y) * static_cast<float>(USER_DEFAULT_SCREEN_DPI) / POINTS_PER_INCH;

        // The advance is the number of pixels left-to-right (X dimension) for the given font.
        // We're finding a proportional factor here with the design units in "ems", not an actual pixel measurement.

        // For HWND swap chains, we play trickery with the font size. For others, we use inherent scaling.
        // For composition swap chains, we scale by the DPI later during drawing and presentation.
        if (_chainMode == SwapChainMode::ForHwnd)
        {
            heightDesired *= (static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI));
        }

        const float widthAdvance = static_cast<float>(advanceInDesignUnits) / fontMetrics.designUnitsPerEm;

        // Use the real pixel height desired by the "em" factor for the width to get the number of pixels
        // we will need per character in width. This will almost certainly result in fractional X-dimension pixels.
        const float widthApprox = heightDesired * widthAdvance;

        // Since we can't deal with columns of the presentation grid being fractional pixels in width, round to the nearest whole pixel.
        const float widthExact = round(widthApprox);

        // Now reverse the "em" factor from above to turn the exact pixel width into a (probably) fractional
        // height in pixels of each character. It's easier for us to pad out height and align vertically
        // than it is horizontally.
        const auto fontSize = widthExact / widthAdvance;

        // Now figure out the basic properties of the character height which include ascent and descent
        // for this specific font size.
        const float ascent = (fontSize * fontMetrics.ascent) / fontMetrics.designUnitsPerEm;
        const float descent = (fontSize * fontMetrics.descent) / fontMetrics.designUnitsPerEm;

        // We're going to build a line spacing object here to track all of this data in our format.
        DWRITE_LINE_SPACING lineSpacing = {};
        lineSpacing.method = DWRITE_LINE_SPACING_METHOD_UNIFORM;

        // We need to make sure the baseline falls on a round pixel (not a fractional pixel).
        // If the baseline is fractional, the text appears blurry, especially at small scales.
        // Since we also need to make sure the bounding box as a whole is round pixels
        // (because the entire console system maths in full cell units),
        // we're just going to ceiling up the ascent and descent to make a full pixel amount
        // and set the baseline to the full round pixel ascent value.
        //
        // For reference, for the letters "ag":
        // aaaaaa   ggggggg     <===================================
        //      a   g    g            |                            |
        //  aaaaa   ggggg             |<-ascent                    |
        // a    a   g                 |                            |---- height
        // aaaaa a  gggggg      <-------------------baseline       |
        //          g     g           |<-descent                   |
        //          gggggg      <===================================
        //
        const auto fullPixelAscent = ceil(ascent);
        const auto fullPixelDescent = ceil(descent);
        lineSpacing.height = fullPixelAscent + fullPixelDescent;
        lineSpacing.baseline = fullPixelAscent;

        // Create the font with the fractional pixel height size.
        // It should have an integer pixel width by our math above.
        // Then below, apply the line spacing to the format to position the floating point pixel height characters
        // into a cell that has an integer pixel height leaving some padding above/below as necessary to round them out.
        Microsoft::WRL::ComPtr<IDWriteTextFormat> format;
        THROW_IF_FAILED(_dwriteFactory->CreateTextFormat(fontName.data(),
                                                         nullptr,
                                                         weight,
                                                         style,
                                                         stretch,
                                                         fontSize,
                                                         localeName.data(),
                                                         &format));

        THROW_IF_FAILED(format.As(&textFormat));

        Microsoft::WRL::ComPtr<IDWriteTextAnalyzer> analyzer;
        THROW_IF_FAILED(_dwriteFactory->CreateTextAnalyzer(&analyzer));
        THROW_IF_FAILED(analyzer.As(&textAnalyzer));

        fontFace = face;

        THROW_IF_FAILED(textFormat->SetLineSpacing(lineSpacing.method, lineSpacing.height, lineSpacing.baseline));
        THROW_IF_FAILED(textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));
        THROW_IF_FAILED(textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));

        // The scaled size needs to represent the pixel box that each character will fit within for the purposes
        // of hit testing math and other such multiplication/division.
        COORD coordSize = { 0 };
        coordSize.X = gsl::narrow<SHORT>(widthExact);
        coordSize.Y = gsl::narrow<SHORT>(lineSpacing.height);

        // Unscaled is for the purposes of re-communicating this font back to the renderer again later.
        // As such, we need to give the same original size parameter back here without padding
        // or rounding or scaling manipulation.
        const COORD unscaled = desired.GetEngineSize();

        const COORD scaled = coordSize;

        actual.SetFromEngine(fontName,
                             desired.GetFamily(),
                             textFormat->GetFontWeight(),
                             false,
                             scaled,
                             unscaled);
    }
    CATCH_RETURN();

    return S_OK;
}

// Routine Description:
// - Helps convert a GDI COLORREF into a Direct2D ColorF
// Arguments:
// - color - GDI color
// Return Value:
// - D2D color
[[nodiscard]] D2D1_COLOR_F DxEngine::_ColorFFromColorRef(const COLORREF color) noexcept
{
    // Converts BGR color order to RGB.
    const UINT32 rgb = ((color & 0x0000FF) << 16) | (color & 0x00FF00) | ((color & 0xFF0000) >> 16);

    switch (_chainMode)
    {
    case SwapChainMode::ForHwnd:
    {
        return D2D1::ColorF(rgb);
    }
    case SwapChainMode::ForComposition:
    {
        // Get the A value we've snuck into the highest byte
        const BYTE a = ((color >> 24) & 0xFF);
        const float aFloat = a / 255.0f;

        return D2D1::ColorF(rgb, aFloat);
    }
    default:
        FAIL_FAST_HR(E_NOTIMPL);
    }
}

// Routine Description:
// - Updates the selection background color of the DxEngine
// Arguments:
// - color - GDI Color
// Return Value:
// - N/A
void DxEngine::SetSelectionBackground(const COLORREF color) noexcept
{
    _selectionBackground = D2D1::ColorF(GetRValue(color) / 255.0f,
                                        GetGValue(color) / 255.0f,
                                        GetBValue(color) / 255.0f,
                                        0.5f);
}
