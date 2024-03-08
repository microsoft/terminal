// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

#include "Backend.h"
#include "../base/FontCache.h"

// #### NOTE ####
// If you see any code in here that contains "_r." you might be seeing a race condition.
// The AtlasEngine::Present() method is called on a background thread without any locks,
// while any of the API methods (like AtlasEngine::Invalidate) might be called concurrently.
// The usage of _r fields is unsafe as those are accessed and written to by the Present() method.

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26459) // You called an STL function '...' with a raw pointer parameter at position '...' that may be unsafe [...].
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render::Atlas;

// Like gsl::narrow but returns a HRESULT.
#pragma warning(push)
#pragma warning(disable : 26472) // Don't use a static_cast for arithmetic conversions. Use brace initialization, gsl::narrow_cast or gsl::narrow (type.1).
template<typename T, typename U>
constexpr HRESULT api_narrow(U val, T& out) noexcept
{
    out = static_cast<T>(val);
    return static_cast<U>(out) != val || (std::is_signed_v<T> != std::is_signed_v<U> && out < T{} != val < U{}) ? HRESULT_FROM_WIN32(ERROR_ARITHMETIC_OVERFLOW) : S_OK;
}
#pragma warning(pop)

template<typename T, typename U>
constexpr HRESULT vec2_narrow(U x, U y, vec2<T>& out) noexcept
{
    return api_narrow(x, out.x) | api_narrow(y, out.y);
}

#pragma region IRenderEngine

[[nodiscard]] HRESULT AtlasEngine::Invalidate(const til::rect* const psrRegion) noexcept
{
    //assert(psrRegion->top < psrRegion->bottom && psrRegion->top >= 0 && psrRegion->bottom <= _api.cellCount.y);

    // BeginPaint() protects against invalid out of bounds numbers.
    _api.invalidatedRows.start = std::min(_api.invalidatedRows.start, gsl::narrow_cast<u16>(psrRegion->top));
    _api.invalidatedRows.end = std::max(_api.invalidatedRows.end, gsl::narrow_cast<u16>(psrRegion->bottom));
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateCursor(const til::rect* const psrRegion) noexcept
{
    //assert(psrRegion->left <= psrRegion->right && psrRegion->left >= 0 && psrRegion->right <= _api.cellCount.x);
    //assert(psrRegion->top <= psrRegion->bottom && psrRegion->top >= 0 && psrRegion->bottom <= _api.cellCount.y);

    const auto left = gsl::narrow_cast<u16>(psrRegion->left);
    const auto top = gsl::narrow_cast<u16>(psrRegion->top);
    const auto right = gsl::narrow_cast<u16>(psrRegion->right);
    const auto bottom = gsl::narrow_cast<u16>(psrRegion->bottom);

    // BeginPaint() protects against invalid out of bounds numbers.
    _api.invalidatedCursorArea.left = std::min(_api.invalidatedCursorArea.left, left);
    _api.invalidatedCursorArea.top = std::min(_api.invalidatedCursorArea.top, top);
    _api.invalidatedCursorArea.right = std::max(_api.invalidatedCursorArea.right, right);
    _api.invalidatedCursorArea.bottom = std::max(_api.invalidatedCursorArea.bottom, bottom);
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateSystem(const til::rect* const prcDirtyClient) noexcept
{
    const auto top = prcDirtyClient->top / _api.s->font->cellSize.y;
    const auto bottom = prcDirtyClient->bottom / _api.s->font->cellSize.y;

    // BeginPaint() protects against invalid out of bounds numbers.
    til::rect rect;
    rect.top = top;
    rect.bottom = bottom;
    return Invalidate(&rect);
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateSelection(const std::vector<til::rect>& rectangles) noexcept
{
    for (const auto& rect : rectangles)
    {
        // BeginPaint() protects against invalid out of bounds numbers.
        // TODO: rect can contain invalid out of bounds coordinates when the selection is being
        // dragged outside of the viewport (and the window begins scrolling automatically).
        _api.invalidatedRows.start = gsl::narrow_cast<u16>(std::min<int>(_api.invalidatedRows.start, std::max<int>(0, rect.top)));
        _api.invalidatedRows.end = gsl::narrow_cast<u16>(std::max<int>(_api.invalidatedRows.end, std::max<int>(0, rect.bottom)));
    }
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateScroll(const til::point* const pcoordDelta) noexcept
{
    // InvalidateScroll() is a "synchronous" API. Any Invalidate()s after
    // a InvalidateScroll() refer to the new viewport after the scroll.
    // --> We need to shift the current invalidation rectangles as well.

    if (const auto delta = pcoordDelta->x)
    {
        _api.invalidatedCursorArea.left = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedCursorArea.left + delta, u16min, u16max));
        _api.invalidatedCursorArea.right = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedCursorArea.right + delta, u16min, u16max));

        _api.invalidatedRows = invalidatedRowsAll;
    }

    if (const auto delta = pcoordDelta->y)
    {
        _api.scrollOffset = gsl::narrow_cast<i16>(clamp<int>(_api.scrollOffset + delta, i16min, i16max));

        _api.invalidatedCursorArea.top = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedCursorArea.top + delta, u16min, u16max));
        _api.invalidatedCursorArea.bottom = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedCursorArea.bottom + delta, u16min, u16max));

        if (delta < 0)
        {
            _api.invalidatedRows.start = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedRows.start + delta, u16min, u16max));
            _api.invalidatedRows.end = _api.s->viewportCellCount.y;
        }
        else
        {
            _api.invalidatedRows.start = 0;
            _api.invalidatedRows.end = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedRows.end + delta, u16min, u16max));
        }
    }

    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateAll() noexcept
{
    _api.invalidatedRows = invalidatedRowsAll;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateFlush(_In_ const bool /*circled*/, _Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);
    *pForcePaint = false;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateTitle(const std::wstring_view proposedTitle) noexcept
{
    _api.invalidatedTitle = true;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::NotifyNewText(const std::wstring_view newText) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::UpdateFont(const FontInfoDesired& fontInfoDesired, _Out_ FontInfo& fontInfo) noexcept
{
    return UpdateFont(fontInfoDesired, fontInfo, {}, {});
}

[[nodiscard]] HRESULT AtlasEngine::UpdateSoftFont(const std::span<const uint16_t> bitPattern, const til::size cellSize, const size_t centeringHint) noexcept
{
    const auto softFont = _api.s.write()->font.write();
    softFont->softFontPattern.assign(bitPattern.begin(), bitPattern.end());
    softFont->softFontCellSize.width = std::max(0, cellSize.width);
    softFont->softFontCellSize.height = std::max(0, cellSize.height);
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::UpdateDpi(const int dpi) noexcept
{
    u16 newDPI;
    RETURN_IF_FAILED(api_narrow(dpi, newDPI));

    if (_api.s->font->dpi != newDPI)
    {
        _api.s.write()->font.write()->dpi = newDPI;
    }

    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::UpdateViewport(const til::inclusive_rect& srNewViewport) noexcept
try
{
    const u16x2 viewportCellCount{
        gsl::narrow<u16>(std::max(1, srNewViewport.right - srNewViewport.left + 1)),
        gsl::narrow<u16>(std::max(1, srNewViewport.bottom - srNewViewport.top + 1)),
    };
    const u16x2 viewportOffset{
        gsl::narrow<u16>(srNewViewport.left),
        gsl::narrow<u16>(srNewViewport.top),
    };

    if (_api.s->viewportCellCount != viewportCellCount)
    {
        _api.s.write()->viewportCellCount = viewportCellCount;
    }
    if (_api.s->viewportOffset != viewportOffset)
    {
        _api.s.write()->viewportOffset = viewportOffset;
    }

    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::GetProposedFont(const FontInfoDesired& fontInfoDesired, _Out_ FontInfo& fontInfo, const int dpi) noexcept
try
{
    // One day I'm going to implement GDI for AtlasEngine...
    // Until then this code is work in progress.
#if 0
    wil::unique_hfont hfont;

    // This block of code (for GDI fonts) is unfinished.
    if (fontInfoDesired.IsDefaultRasterFont())
    {
        hfont.reset(static_cast<HFONT>(GetStockObject(OEM_FIXED_FONT)));
        RETURN_HR_IF(E_FAIL, !hfont);
    }
    else if (requestedFaceName == DEFAULT_RASTER_FONT_FACENAME)
    {
        // GDI Windows Font Mapping reference:
        // https://msdn.microsoft.com/en-us/library/ms969909.aspx

        LOGFONTW lf;
        lf.lfHeight = -MulDiv(requestedSize.height, dpi, 72);
        lf.lfWidth = 0;
        lf.lfEscapement = 0;
        lf.lfOrientation = 0;
        lf.lfWeight = requestedWeight;
        lf.lfItalic = FALSE;
        lf.lfUnderline = FALSE;
        lf.lfStrikeOut = FALSE;
        lf.lfCharSet = OEM_CHARSET;
        lf.lfOutPrecision = OUT_RASTER_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = PROOF_QUALITY; // disables scaling for rasterized fonts
        lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
        // .size() only includes regular characters, but we also want to copy the trailing \0, so +1 it is.
        memcpy(&lf.lfFaceName[0], &DEFAULT_RASTER_FONT_FACENAME[0], sizeof(DEFAULT_RASTER_FONT_FACENAME));

        hfont.reset(CreateFontIndirectW(&lf));
        RETURN_HR_IF(E_FAIL, !hfont);
    }

    if (hfont)
    {
// wil::unique_any_t's constructor says: "should not be WI_NOEXCEPT (may forward to a throwing constructor)".
// The constructor we use by default doesn't throw.
#pragma warning(suppress : 26447) // The function is declared 'noexcept' but calls function '...' which may throw exceptions (f.6).
        wil::unique_hdc hdc{ CreateCompatibleDC(nullptr) };
        RETURN_HR_IF(E_FAIL, !hdc);

        DeleteObject(SelectObject(hdc.get(), hfont.get()));

        til::size sz;
        RETURN_HR_IF(E_FAIL, !GetTextExtentPoint32W(hdc.get(), L"M", 1, &sz));
        resultingCellSize.width = sz.width;
        resultingCellSize.height = sz.height;
    }
#endif

    _resolveFontMetrics(nullptr, fontInfoDesired, fontInfo);
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::GetDirtyArea(std::span<const til::rect>& area) noexcept
{
    area = std::span{ &_api.dirtyRect, 1 };
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::GetFontSize(_Out_ til::size* pFontSize) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pFontSize);
    pFontSize->width = _api.s->font->cellSize.x;
    pFontSize->height = _api.s->font->cellSize.y;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pResult);

    wil::com_ptr<IDWriteTextFormat> textFormat;
    RETURN_IF_FAILED(_p.dwriteFactory->CreateTextFormat(
        /* fontFamilyName */ _api.s->font->fontName.c_str(),
        /* fontCollection */ _api.s->font->fontCollection.get(),
        /* fontWeight     */ static_cast<DWRITE_FONT_WEIGHT>(_api.s->font->fontWeight),
        /* fontStyle      */ DWRITE_FONT_STYLE_NORMAL,
        /* fontStretch    */ DWRITE_FONT_STRETCH_NORMAL,
        /* fontSize       */ _api.s->font->fontSize,
        /* localeName     */ _p.userLocaleName.c_str(),
        /* textFormat     */ textFormat.addressof()));

    wil::com_ptr<IDWriteTextLayout> textLayout;
    RETURN_IF_FAILED(_p.dwriteFactory->CreateTextLayout(glyph.data(), gsl::narrow_cast<uint32_t>(glyph.size()), textFormat.get(), FLT_MAX, FLT_MAX, textLayout.addressof()));

    DWRITE_TEXT_METRICS metrics{};
    RETURN_IF_FAILED(textLayout->GetMetrics(&metrics));

    const auto minWidth = (_api.s->font->cellSize.x * 1.2f);
    *pResult = metrics.width > minWidth;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::UpdateTitle(const std::wstring_view newTitle) noexcept
{
    return S_OK;
}

#pragma endregion

#pragma region DxRenderer

HRESULT AtlasEngine::Enable() noexcept
{
    return S_OK;
}

[[nodiscard]] std::wstring_view AtlasEngine::GetPixelShaderPath() noexcept
{
    return _api.s->misc->customPixelShaderPath;
}

[[nodiscard]] std::wstring_view AtlasEngine::GetPixelShaderImagePath() noexcept
{
    return _api.s->misc->customPixelShaderImagePath;
}

[[nodiscard]] bool AtlasEngine::GetRetroTerminalEffect() const noexcept
{
    return _api.s->misc->useRetroTerminalEffect;
}

[[nodiscard]] float AtlasEngine::GetScaling() const noexcept
{
    return static_cast<f32>(_api.s->font->dpi) / static_cast<f32>(USER_DEFAULT_SCREEN_DPI);
}

[[nodiscard]] Microsoft::Console::Types::Viewport AtlasEngine::GetViewportInCharacters(const Types::Viewport& viewInPixels) const noexcept
{
    assert(_api.s->font->cellSize.x != 0);
    assert(_api.s->font->cellSize.y != 0);
    return Types::Viewport::FromDimensions(viewInPixels.Origin(), { viewInPixels.Width() / _api.s->font->cellSize.x, viewInPixels.Height() / _api.s->font->cellSize.y });
}

[[nodiscard]] Microsoft::Console::Types::Viewport AtlasEngine::GetViewportInPixels(const Types::Viewport& viewInCharacters) const noexcept
{
    assert(_api.s->font->cellSize.x != 0);
    assert(_api.s->font->cellSize.y != 0);
    return Types::Viewport::FromDimensions(viewInCharacters.Origin(), { viewInCharacters.Width() * _api.s->font->cellSize.x, viewInCharacters.Height() * _api.s->font->cellSize.y });
}

void AtlasEngine::SetAntialiasingMode(const D2D1_TEXT_ANTIALIAS_MODE antialiasingMode) noexcept
{
    const auto mode = static_cast<AntialiasingMode>(antialiasingMode);
    if (_api.antialiasingMode != mode)
    {
        _api.antialiasingMode = mode;
        _resolveTransparencySettings();
    }
}

void AtlasEngine::SetCallback(std::function<void(HANDLE)> pfn) noexcept
{
    _p.swapChainChangedCallback = std::move(pfn);
}

void AtlasEngine::EnableTransparentBackground(const bool isTransparent) noexcept
{
    if (_api.enableTransparentBackground != isTransparent)
    {
        _api.enableTransparentBackground = isTransparent;
        _resolveTransparencySettings();
    }
}

void AtlasEngine::SetForceFullRepaintRendering(bool enable) noexcept
{
}

[[nodiscard]] HRESULT AtlasEngine::SetHwnd(const HWND hwnd) noexcept
{
    if (_api.s->target->hwnd != hwnd)
    {
        _api.s.write()->target.write()->hwnd = hwnd;
    }
    return S_OK;
}

void AtlasEngine::SetPixelShaderPath(std::wstring_view value) noexcept
try
{
    if (_api.s->misc->customPixelShaderPath != value)
    {
        _api.s.write()->misc.write()->customPixelShaderPath = value;
        _resolveTransparencySettings();
    }
}
CATCH_LOG()

void AtlasEngine::SetPixelShaderImagePath(std::wstring_view value) noexcept
try
{
    if (_api.s->misc->customPixelShaderImagePath != value)
    {
        _api.s.write()->misc.write()->customPixelShaderImagePath = value;
        _resolveTransparencySettings();
    }
}
CATCH_LOG()

void AtlasEngine::SetRetroTerminalEffect(bool enable) noexcept
{
    if (_api.s->misc->useRetroTerminalEffect != enable)
    {
        _api.s.write()->misc.write()->useRetroTerminalEffect = enable;
        _resolveTransparencySettings();
    }
}

void AtlasEngine::SetSelectionBackground(const COLORREF color, const float alpha) noexcept
{
    const u32 selectionColor = (color & 0xffffff) | gsl::narrow_cast<u32>(lrintf(alpha * 255.0f)) << 24;
    if (_api.s->misc->selectionColor != selectionColor)
    {
        _api.s.write()->misc.write()->selectionColor = selectionColor;
    }
}

void AtlasEngine::SetSoftwareRendering(bool enable) noexcept
{
    if (_api.s->target->useSoftwareRendering != enable)
    {
        _api.s.write()->target.write()->useSoftwareRendering = enable;
    }
}

void AtlasEngine::SetWarningCallback(std::function<void(HRESULT)> pfn) noexcept
{
    _p.warningCallback = std::move(pfn);
}

[[nodiscard]] HRESULT AtlasEngine::SetWindowSize(const til::size pixels) noexcept
{
    // When Win+D is pressed, `GetClientRect` returns {0,0}.
    // There's probably more situations in which our callers may pass us invalid data.
    if (!pixels)
    {
        return S_OK;
    }

    const u16x2 newSize{
        gsl::narrow_cast<u16>(clamp<til::CoordType>(pixels.width, 1, u16max)),
        gsl::narrow_cast<u16>(clamp<til::CoordType>(pixels.height, 1, u16max)),
    };

    if (_api.s->targetSize != newSize)
    {
        _api.s.write()->targetSize = newSize;
    }

    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::UpdateFont(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept
{
    try
    {
        _updateFont(fontInfoDesired.GetFaceName().c_str(), fontInfoDesired, fontInfo, features, axes);
        return S_OK;
    }
    CATCH_LOG();

    if constexpr (Feature_NearbyFontLoading::IsEnabled())
    {
        try
        {
            // _resolveFontMetrics() checks `_api.s->font->fontCollection` for a pre-existing font collection,
            // before falling back to using the system font collection. This way we can inject our custom one. See GH#9375.
            // Doing it this way is a bit hacky, but it does have the benefit that we can cache a font collection
            // instance across font changes, like when zooming the font size rapidly using the scroll wheel.
            _api.s.write()->font.write()->fontCollection = FontCache::GetCached();
            _updateFont(fontInfoDesired.GetFaceName().c_str(), fontInfoDesired, fontInfo, features, axes);
            return S_OK;
        }
        CATCH_LOG();
    }

    try
    {
        _updateFont(nullptr, fontInfoDesired, fontInfo, features, axes);
        return S_OK;
    }
    CATCH_RETURN();
}

void AtlasEngine::UpdateHyperlinkHoveredId(const uint16_t hoveredId) noexcept
{
    _api.hyperlinkHoveredId = hoveredId;
}

#pragma endregion

void AtlasEngine::_resolveTransparencySettings() noexcept
{
    // An opaque background allows us to use true "independent" flips. See AtlasEngine::_createSwapChain().
    // We can't enable them if custom shaders are specified, because it's unknown, whether they support opaque inputs.
    const bool useAlpha = _api.enableTransparentBackground || !_api.s->misc->customPixelShaderPath.empty();
    // If the user asks for ClearType, but also for a transparent background
    // (which our ClearType shader doesn't simultaneously support)
    // then we need to sneakily force the renderer to grayscale AA.
    const auto antialiasingMode = useAlpha && _api.antialiasingMode == AntialiasingMode::ClearType ? AntialiasingMode::Grayscale : _api.antialiasingMode;

    if (antialiasingMode != _api.s->font->antialiasingMode || useAlpha != _api.s->target->useAlpha)
    {
        const auto s = _api.s.write();
        s->font.write()->antialiasingMode = antialiasingMode;
        s->target.write()->useAlpha = useAlpha;
        _api.backgroundOpaqueMixin = useAlpha ? 0x00000000 : 0xff000000;
    }
}

void AtlasEngine::_updateFont(const wchar_t* faceName, const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes)
{
    std::vector<DWRITE_FONT_FEATURE> fontFeatures;
    if (!features.empty())
    {
        fontFeatures.reserve(features.size() + 3);

        // All of these features are enabled by default by DirectWrite.
        // If you want to (and can) peek into the source of DirectWrite
        // you can look for the "GenericDefaultGsubFeatures" and "GenericDefaultGposFeatures" arrays.
        // Gsub is for GetGlyphs() and Gpos for GetGlyphPlacements().
        //
        // GH#10774: Apparently specifying all of the features is just redundant.
        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES, 1);
        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES, 1);
        fontFeatures.emplace_back(DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES, 1);

        for (const auto& p : features)
        {
            if (p.first.size() == 4)
            {
                const auto s = p.first.data();
                switch (const auto tag = DWRITE_MAKE_FONT_FEATURE_TAG(s[0], s[1], s[2], s[3]))
                {
                case DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES:
                    fontFeatures[0].parameter = p.second;
                    break;
                case DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES:
                    fontFeatures[1].parameter = p.second;
                    break;
                case DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES:
                    fontFeatures[2].parameter = p.second;
                    break;
                default:
                    fontFeatures.emplace_back(tag, p.second);
                    break;
                }
            }
        }
    }

    std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues;
    if (!axes.empty())
    {
        fontAxisValues.reserve(axes.size() + 3);

        // AtlasEngine::_recreateFontDependentResources() relies on these fields to
        // exist in this particular order in order to create appropriate default axes.
        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_WEIGHT, -1.0f);
        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_ITALIC, -1.0f);
        fontAxisValues.emplace_back(DWRITE_FONT_AXIS_TAG_SLANT, -1.0f);

        for (const auto& p : axes)
        {
            if (p.first.size() == 4)
            {
                const auto s = p.first.data();
                switch (const auto tag = DWRITE_MAKE_FONT_AXIS_TAG(s[0], s[1], s[2], s[3]))
                {
                case DWRITE_FONT_AXIS_TAG_WEIGHT:
                    fontAxisValues[0].value = p.second;
                    break;
                case DWRITE_FONT_AXIS_TAG_ITALIC:
                    fontAxisValues[1].value = p.second;
                    break;
                case DWRITE_FONT_AXIS_TAG_SLANT:
                    fontAxisValues[2].value = p.second;
                    break;
                default:
                    fontAxisValues.emplace_back(tag, p.second);
                    break;
                }
            }
        }
    }

    const auto font = _api.s.write()->font.write();
    _resolveFontMetrics(faceName, fontInfoDesired, fontInfo, font);
    font->fontFeatures = std::move(fontFeatures);
    font->fontAxisValues = std::move(fontAxisValues);
}

void AtlasEngine::_resolveFontMetrics(const wchar_t* requestedFaceName, const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, FontSettings* fontMetrics) const
{
    const auto requestedFamily = fontInfoDesired.GetFamily();
    auto requestedWeight = fontInfoDesired.GetWeight();
    auto fontSize = fontInfoDesired.GetFontSize();
    auto requestedSize = fontInfoDesired.GetEngineSize();

    if (!requestedFaceName)
    {
        requestedFaceName = L"Consolas";
    }
    if (!requestedSize.height)
    {
        fontSize = 12.0f;
        requestedSize = { 0, 12 };
    }
    if (!requestedWeight)
    {
        requestedWeight = DWRITE_FONT_WEIGHT_NORMAL;
    }

    // UpdateFont() (and its NearbyFontLoading feature path specifically) sets `_api.s->font->fontCollection`
    // to a custom font collection that includes .ttf files that are bundled with our app package. See GH#9375.
    // Doing it this way is a bit hacky, but it does have the benefit that we can cache a font collection
    // instance across font changes, like when zooming the font size rapidly using the scroll wheel.
    auto fontCollection = _api.s->font->fontCollection;
    if (!fontCollection)
    {
        THROW_IF_FAILED(_p.dwriteFactory->GetSystemFontCollection(fontCollection.addressof(), FALSE));
    }

    u32 index = 0;
    BOOL exists = false;
    THROW_IF_FAILED(fontCollection->FindFamilyName(requestedFaceName, &index, &exists));
    THROW_HR_IF(DWRITE_E_NOFONT, !exists);

    wil::com_ptr<IDWriteFontFamily> fontFamily;
    THROW_IF_FAILED(fontCollection->GetFontFamily(index, fontFamily.addressof()));

    wil::com_ptr<IDWriteFont> font;
    THROW_IF_FAILED(fontFamily->GetFirstMatchingFont(static_cast<DWRITE_FONT_WEIGHT>(requestedWeight), DWRITE_FONT_STRETCH_NORMAL, DWRITE_FONT_STYLE_NORMAL, font.addressof()));

    wil::com_ptr<IDWriteFontFace> fontFace;
    THROW_IF_FAILED(font->CreateFontFace(fontFace.addressof()));

    DWRITE_FONT_METRICS metrics{};
    fontFace->GetMetrics(&metrics);

    // Point sizes are commonly treated at a 72 DPI scale
    // (including by OpenType), whereas DirectWrite uses 96 DPI.
    // Since we want the height in px we multiply by the display's DPI.
    const auto dpi = static_cast<f32>(_api.s->font->dpi);
    const auto fontSizeInPx = fontSize / 72.0f * dpi;

    const auto designUnitsPerPx = fontSizeInPx / static_cast<f32>(metrics.designUnitsPerEm);
    const auto ascent = static_cast<f32>(metrics.ascent) * designUnitsPerPx;
    const auto descent = static_cast<f32>(metrics.descent) * designUnitsPerPx;
    const auto lineGap = static_cast<f32>(metrics.lineGap) * designUnitsPerPx;
    const auto underlinePosition = static_cast<f32>(-metrics.underlinePosition) * designUnitsPerPx;
    const auto underlineThickness = static_cast<f32>(metrics.underlineThickness) * designUnitsPerPx;
    const auto strikethroughPosition = static_cast<f32>(-metrics.strikethroughPosition) * designUnitsPerPx;
    const auto strikethroughThickness = static_cast<f32>(metrics.strikethroughThickness) * designUnitsPerPx;
    const auto advanceHeight = ascent + descent + lineGap;

    // We use the same character to determine the advance width as CSS for its "ch" unit ("0").
    // According to the CSS spec, if it's impossible to determine the advance width,
    // it must be assumed to be 0.5em wide. em in CSS refers to the computed font-size.
    auto advanceWidth = 0.5f * fontSizeInPx;
    {
        static constexpr u32 codePoint = '0';

        u16 glyphIndex;
        THROW_IF_FAILED(fontFace->GetGlyphIndicesW(&codePoint, 1, &glyphIndex));

        if (glyphIndex)
        {
            DWRITE_GLYPH_METRICS glyphMetrics{};
            THROW_IF_FAILED(fontFace->GetDesignGlyphMetrics(&glyphIndex, 1, &glyphMetrics, FALSE));
            advanceWidth = static_cast<f32>(glyphMetrics.advanceWidth) * designUnitsPerPx;
        }
    }

    auto adjustedWidth = std::roundf(fontInfoDesired.GetCellWidth().Resolve(advanceWidth, dpi, fontSizeInPx, advanceWidth));
    auto adjustedHeight = std::roundf(fontInfoDesired.GetCellHeight().Resolve(advanceHeight, dpi, fontSizeInPx, advanceWidth));

    // Protection against bad user values in GetCellWidth/Y.
    // AtlasEngine fails hard with 0 cell sizes.
    adjustedWidth = std::max(1.0f, adjustedWidth);
    adjustedHeight = std::max(1.0f, adjustedHeight);

    const auto baseline = std::roundf(ascent + (lineGap + adjustedHeight - advanceHeight) / 2.0f);
    const auto underlinePos = std::roundf(baseline + underlinePosition);
    const auto underlineWidth = std::max(1.0f, std::roundf(underlineThickness));
    const auto strikethroughPos = std::roundf(baseline + strikethroughPosition);
    const auto strikethroughWidth = std::max(1.0f, std::roundf(strikethroughThickness));
    const auto doubleUnderlineWidth = std::max(1.0f, std::roundf(underlineThickness / 2.0f));
    const auto thinLineWidth = std::max(1.0f, std::roundf(std::max(adjustedWidth / 16.0f, adjustedHeight / 32.0f)));

    // For double underlines we loosely follow what Word does:
    // 1. The lines are half the width of an underline (= doubleUnderlineWidth)
    // 2. Ideally the bottom line is aligned with the bottom of the underline
    // 3. The top underline is vertically in the middle between baseline and ideal bottom underline
    // 4. If the top line gets too close to the baseline the underlines are shifted downwards
    // 5. The minimum gap between the two lines appears to be similar to Tex (1.2pt)
    // (Additional notes below.)

    // 2.
    auto doubleUnderlinePosBottom = underlinePos + underlineWidth - doubleUnderlineWidth;
    // 3. Since we don't align the center of our two lines, but rather the top borders
    //    we need to subtract half a line width from our center point.
    auto doubleUnderlinePosTop = std::roundf((baseline + doubleUnderlinePosBottom - doubleUnderlineWidth) / 2.0f);
    // 4.
    doubleUnderlinePosTop = std::max(doubleUnderlinePosTop, baseline + doubleUnderlineWidth);
    // 5. The gap is only the distance _between_ the lines, but we need the distance from the
    //    top border of the top and bottom lines, which includes an additional line width.
    const auto doubleUnderlineGap = std::max(1.0f, std::roundf(1.2f / 72.0f * dpi));
    doubleUnderlinePosBottom = std::max(doubleUnderlinePosBottom, doubleUnderlinePosTop + doubleUnderlineGap + doubleUnderlineWidth);
    // Our cells can't overlap each other so we additionally clamp the bottom line to be inside the cell boundaries.
    doubleUnderlinePosBottom = std::min(doubleUnderlinePosBottom, adjustedHeight - doubleUnderlineWidth);

    const auto cellWidth = gsl::narrow<u16>(lrintf(adjustedWidth));
    const auto cellHeight = gsl::narrow<u16>(lrintf(adjustedHeight));

    {
        til::size coordSize;
        coordSize.width = cellWidth;
        coordSize.height = cellHeight;

        if (requestedSize.width == 0)
        {
            // The coordSizeUnscaled parameter to SetFromEngine is used for API functions like GetConsoleFontSize.
            // Since clients expect that settings the font height to Y yields back a font height of Y,
            // we're scaling the X relative/proportional to the actual cellWidth/cellHeight ratio.
            requestedSize.width = gsl::narrow_cast<til::CoordType>(lrintf(fontSize / cellHeight * cellWidth));
        }

        fontInfo.SetFromEngine(requestedFaceName, requestedFamily, requestedWeight, false, coordSize, requestedSize);
    }

    if (fontMetrics)
    {
        std::wstring fontName{ requestedFaceName };
        const auto fontWeightU16 = gsl::narrow_cast<u16>(requestedWeight);
        const auto advanceWidthU16 = gsl::narrow_cast<u16>(lrintf(advanceWidth));
        const auto baselineU16 = gsl::narrow_cast<u16>(lrintf(baseline));
        const auto descenderU16 = gsl::narrow_cast<u16>(cellHeight - baselineU16);
        const auto thinLineWidthU16 = gsl::narrow_cast<u16>(lrintf(thinLineWidth));

        const auto gridBottomPositionU16 = gsl::narrow_cast<u16>(cellHeight - thinLineWidth);
        const auto gridRightPositionU16 = gsl::narrow_cast<u16>(cellWidth - thinLineWidth);

        const auto underlinePosU16 = gsl::narrow_cast<u16>(lrintf(underlinePos));
        const auto underlineWidthU16 = gsl::narrow_cast<u16>(lrintf(underlineWidth));
        const auto strikethroughPosU16 = gsl::narrow_cast<u16>(lrintf(strikethroughPos));
        const auto strikethroughWidthU16 = gsl::narrow_cast<u16>(lrintf(strikethroughWidth));
        const auto doubleUnderlinePosTopU16 = gsl::narrow_cast<u16>(lrintf(doubleUnderlinePosTop));
        const auto doubleUnderlinePosBottomU16 = gsl::narrow_cast<u16>(lrintf(doubleUnderlinePosBottom));
        const auto doubleUnderlineWidthU16 = gsl::narrow_cast<u16>(lrintf(doubleUnderlineWidth));

        // NOTE: From this point onward no early returns or throwing code should exist,
        // as we might cause _api to be in an inconsistent state otherwise.

        fontMetrics->fontCollection = std::move(fontCollection);
        fontMetrics->fontFamily = std::move(fontFamily);
        fontMetrics->fontName = std::move(fontName);
        fontMetrics->fontSize = fontSizeInPx;
        fontMetrics->cellSize = { cellWidth, cellHeight };
        fontMetrics->fontWeight = fontWeightU16;
        fontMetrics->advanceWidth = advanceWidthU16;
        fontMetrics->baseline = baselineU16;
        fontMetrics->descender = descenderU16;
        fontMetrics->thinLineWidth = thinLineWidthU16;

        fontMetrics->gridTop = { 0, thinLineWidthU16 };
        fontMetrics->gridBottom = { gridBottomPositionU16, thinLineWidthU16 };
        fontMetrics->gridLeft = { 0, thinLineWidthU16 };
        fontMetrics->gridRight = { gridRightPositionU16, thinLineWidthU16 };

        fontMetrics->underline = { underlinePosU16, underlineWidthU16 };
        fontMetrics->strikethrough = { strikethroughPosU16, strikethroughWidthU16 };
        fontMetrics->doubleUnderline[0] = { doubleUnderlinePosTopU16, doubleUnderlineWidthU16 };
        fontMetrics->doubleUnderline[1] = { doubleUnderlinePosBottomU16, doubleUnderlineWidthU16 };
        fontMetrics->overline = { 0, underlineWidthU16 };

        fontMetrics->builtinGlyphs = fontInfoDesired.GetEnableBuiltinGlyphs();
    }
}
