// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "AtlasEngine.h"

// #### NOTE ####
// If you see any code in here that contains "_r." you might be seeing a race condition.
// The AtlasEngine::Present() method is called on a background thread without any locks,
// while any of the API methods (like AtlasEngine::Invalidate) might be called concurrently.
// The usage of _r fields is unsafe as those are accessed and written to by the Present() method.

#pragma warning(disable : 4100) // '...': unreferenced formal parameter
// Disable a bunch of warnings which get in the way of writing performant code.
#pragma warning(disable : 26429) // Symbol 'data' is never tested for nullness, it can be marked as not_null (f.23).
#pragma warning(disable : 26446) // Prefer to use gsl::at() instead of unchecked subscript operator (bounds.4).
#pragma warning(disable : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
#pragma warning(disable : 26482) // Only index into arrays using constant expressions (bounds.2).

using namespace Microsoft::Console::Render;

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
constexpr HRESULT vec2_narrow(U x, U y, AtlasEngine::vec2<T>& out) noexcept
{
    return api_narrow(x, out.x) | api_narrow(y, out.y);
}

#pragma region IRenderEngine

[[nodiscard]] HRESULT AtlasEngine::Invalidate(const SMALL_RECT* const psrRegion) noexcept
{
    assert(psrRegion->Top < psrRegion->Bottom && psrRegion->Top >= 0 && psrRegion->Bottom <= _api.cellCount.y);

    // BeginPaint() protects against invalid out of bounds numbers.
    _api.invalidatedRows.x = std::min(_api.invalidatedRows.x, gsl::narrow_cast<u16>(psrRegion->Top));
    _api.invalidatedRows.y = std::max(_api.invalidatedRows.y, gsl::narrow_cast<u16>(psrRegion->Bottom));
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateCursor(const SMALL_RECT* const psrRegion) noexcept
{
    assert(psrRegion->Left <= psrRegion->Right && psrRegion->Left >= 0 && psrRegion->Right <= _api.cellCount.x);
    assert(psrRegion->Top <= psrRegion->Bottom && psrRegion->Top >= 0 && psrRegion->Bottom <= _api.cellCount.y);

    const auto left = gsl::narrow_cast<u16>(psrRegion->Left);
    const auto top = gsl::narrow_cast<u16>(psrRegion->Top);
    const auto right = gsl::narrow_cast<u16>(psrRegion->Right);
    const auto bottom = gsl::narrow_cast<u16>(psrRegion->Bottom);

    // BeginPaint() protects against invalid out of bounds numbers.
    _api.invalidatedCursorArea.left = std::min(_api.invalidatedCursorArea.left, left);
    _api.invalidatedCursorArea.top = std::min(_api.invalidatedCursorArea.top, top);
    _api.invalidatedCursorArea.right = std::max(_api.invalidatedCursorArea.right, right);
    _api.invalidatedCursorArea.bottom = std::max(_api.invalidatedCursorArea.bottom, bottom);
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateSystem(const RECT* const prcDirtyClient) noexcept
{
    const auto top = prcDirtyClient->top / _api.cellSize.y;
    const auto bottom = prcDirtyClient->bottom / _api.cellSize.y;

    // BeginPaint() protects against invalid out of bounds numbers.
    SMALL_RECT rect;
    rect.Top = gsl::narrow_cast<SHORT>(top);
    rect.Bottom = gsl::narrow_cast<SHORT>(bottom);
    return Invalidate(&rect);
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateSelection(const std::vector<SMALL_RECT>& rectangles) noexcept
{
    for (const auto& rect : rectangles)
    {
        // BeginPaint() protects against invalid out of bounds numbers.
        // TODO: rect can contain invalid out of bounds coordinates when the selection is being
        // dragged outside of the viewport (and the window begins scrolling automatically).
        _api.invalidatedRows.x = gsl::narrow_cast<u16>(std::min<int>(_api.invalidatedRows.x, std::max<int>(0, rect.Top)));
        _api.invalidatedRows.y = gsl::narrow_cast<u16>(std::max<int>(_api.invalidatedRows.y, std::max<int>(0, rect.Bottom)));
    }
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateScroll(const COORD* const pcoordDelta) noexcept
{
    const auto delta = pcoordDelta->Y;
    if (delta == 0)
    {
        return S_OK;
    }

    _api.scrollOffset = gsl::narrow_cast<i16>(clamp<int>(_api.scrollOffset + delta, i16min, i16max));

    // InvalidateScroll() is a "synchronous" API. Any Invalidate()s after
    // a InvalidateScroll() refer to the new viewport after the scroll.
    // --> We need to shift the current invalidation rectangles as well.

    _api.invalidatedCursorArea.top = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedCursorArea.top + delta, u16min, u16max));
    _api.invalidatedCursorArea.bottom = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedCursorArea.bottom + delta, u16min, u16max));

    if (delta < 0)
    {
        _api.invalidatedRows.x = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedRows.x + delta, u16min, u16max));
        _api.invalidatedRows.y = _api.cellCount.y;
    }
    else
    {
        _api.invalidatedRows.x = 0;
        _api.invalidatedRows.y = gsl::narrow_cast<u16>(clamp<int>(_api.invalidatedRows.y + delta, u16min, u16max));
    }

    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateAll() noexcept
{
    _api.invalidatedRows = invalidatedRowsAll;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateCircling(_Out_ bool* const pForcePaint) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pForcePaint);
    *pForcePaint = false;
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::InvalidateTitle(const std::wstring_view proposedTitle) noexcept
{
    WI_SetFlag(_api.invalidations, ApiInvalidations::Title);
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::UpdateFont(const FontInfoDesired& fontInfoDesired, _Out_ FontInfo& fontInfo) noexcept
{
    return UpdateFont(fontInfoDesired, fontInfo, {}, {});
}

[[nodiscard]] HRESULT AtlasEngine::UpdateSoftFont(const gsl::span<const uint16_t> bitPattern, const SIZE cellSize, const size_t centeringHint) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::UpdateDpi(const int dpi) noexcept
{
    u16 newDPI;
    RETURN_IF_FAILED(api_narrow(dpi, newDPI));

    if (_api.dpi != newDPI)
    {
        _api.dpi = newDPI;
        WI_SetFlag(_api.invalidations, ApiInvalidations::Font);
    }

    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::UpdateViewport(const SMALL_RECT srNewViewport) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::GetProposedFont(const FontInfoDesired& fontInfoDesired, _Out_ FontInfo& fontInfo, const int dpi) noexcept
try
{
    const wchar_t* requestedFaceName = fontInfoDesired.GetFaceName().data();
    const auto requestedFamily = fontInfoDesired.GetFamily();
    auto requestedWeight = fontInfoDesired.GetWeight();
    auto requestedSize = fontInfoDesired.GetEngineSize();

    if (!requestedFaceName)
    {
        requestedFaceName = L"Consolas";
    }
    if (!requestedWeight)
    {
        requestedWeight = DWRITE_FONT_WEIGHT_NORMAL;
    }
    if (!requestedSize.Y)
    {
        requestedSize = { 0, 16 };
    }

    std::wstring faceNameBuffer;
    std::wstring_view resultingFaceName = requestedFaceName;
    COORD resultingCellSize{};

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
        lf.lfHeight = -MulDiv(requestedSize.Y, dpi, 72);
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

        SIZE sz;
        RETURN_HR_IF(E_FAIL, !GetTextExtentPoint32W(hdc.get(), L"M", 1, &sz));
        resultingCellSize.X = gsl::narrow<SHORT>(sz.cx);
        resultingCellSize.Y = gsl::narrow<SHORT>(sz.cy);
    }
    else
#endif
    {
        wil::com_ptr<IDWriteTextFormat> textFormat;
        THROW_IF_FAILED(_createTextFormat(requestedFaceName, static_cast<DWRITE_FONT_WEIGHT>(requestedWeight), DWRITE_FONT_STYLE_NORMAL, requestedSize.Y, textFormat.addressof()));

        wil::com_ptr<IDWriteTextLayout> textLayout;
        THROW_IF_FAILED(_sr.dwriteFactory->CreateTextLayout(L"M", 1, textFormat.get(), FLT_MAX, FLT_MAX, textLayout.addressof()));

        DWRITE_TEXT_METRICS metrics;
        THROW_IF_FAILED(textLayout->GetMetrics(&metrics));

        const auto scaling = static_cast<float>(dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
        const auto cx = std::ceil(metrics.width * scaling);
        const auto cy = std::ceil(metrics.height * scaling);
        resultingCellSize.X = gsl::narrow<SHORT>(cx);
        resultingCellSize.Y = gsl::narrow<SHORT>(cy);

        faceNameBuffer.resize(textFormat->GetFontFamilyNameLength());
        THROW_IF_FAILED(textFormat->GetFontFamilyName(faceNameBuffer.data(), static_cast<UINT32>(faceNameBuffer.size() + 1)));
        resultingFaceName = faceNameBuffer;
    }

    fontInfo.SetFromEngine(resultingFaceName, requestedFamily, requestedWeight, false, resultingCellSize, requestedSize);
    return S_OK;
}
CATCH_RETURN()

[[nodiscard]] HRESULT AtlasEngine::GetDirtyArea(gsl::span<const til::rectangle>& area) noexcept
{
    area = gsl::span{ &_api.dirtyRect, 1 };
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::GetFontSize(_Out_ COORD* const pFontSize) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pFontSize);
    pFontSize->X = gsl::narrow_cast<SHORT>(_api.cellSize.x);
    pFontSize->Y = gsl::narrow_cast<SHORT>(_api.cellSize.y);
    return S_OK;
}

[[nodiscard]] HRESULT AtlasEngine::IsGlyphWideByFont(const std::wstring_view glyph, _Out_ bool* const pResult) noexcept
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pResult);

    wil::com_ptr<IDWriteTextLayout> textLayout;
    RETURN_IF_FAILED(_sr.dwriteFactory->CreateTextLayout(glyph.data(), gsl::narrow_cast<uint32_t>(glyph.size()), _getTextFormat(false, false), FLT_MAX, FLT_MAX, textLayout.addressof()));

    DWRITE_TEXT_METRICS metrics;
    RETURN_IF_FAILED(textLayout->GetMetrics(&metrics));

    *pResult = static_cast<unsigned int>(std::ceil(metrics.width)) > _api.cellSize.x;
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

[[nodiscard]] bool AtlasEngine::GetRetroTerminalEffect() const noexcept
{
    return false;
}

[[nodiscard]] float AtlasEngine::GetScaling() const noexcept
{
    return static_cast<float>(_api.dpi) / static_cast<float>(USER_DEFAULT_SCREEN_DPI);
}

[[nodiscard]] HANDLE AtlasEngine::GetSwapChainHandle()
{
    if (WI_IsFlagSet(_api.invalidations, ApiInvalidations::Device))
    {
        _createResources();
        WI_ClearFlag(_api.invalidations, ApiInvalidations::Device);
    }

    return _api.swapChainHandle.get();
}

[[nodiscard]] Microsoft::Console::Types::Viewport AtlasEngine::GetViewportInCharacters(const Types::Viewport& viewInPixels) const noexcept
{
    assert(_api.cellSize.x != 0);
    assert(_api.cellSize.y != 0);
    return Types::Viewport::FromDimensions(viewInPixels.Origin(), COORD{ gsl::narrow_cast<short>(viewInPixels.Width() / _api.cellSize.x), gsl::narrow_cast<short>(viewInPixels.Height() / _api.cellSize.y) });
}

[[nodiscard]] Microsoft::Console::Types::Viewport AtlasEngine::GetViewportInPixels(const Types::Viewport& viewInCharacters) const noexcept
{
    assert(_api.cellSize.x != 0);
    assert(_api.cellSize.y != 0);
    return Types::Viewport::FromDimensions(viewInCharacters.Origin(), COORD{ gsl::narrow_cast<short>(viewInCharacters.Width() * _api.cellSize.x), gsl::narrow_cast<short>(viewInCharacters.Height() * _api.cellSize.y) });
}

void AtlasEngine::SetAntialiasingMode(const D2D1_TEXT_ANTIALIAS_MODE antialiasingMode) noexcept
{
    _api.antialiasingMode = gsl::narrow_cast<u16>(antialiasingMode);
    WI_SetFlag(_api.invalidations, ApiInvalidations::Font);
}

void AtlasEngine::SetCallback(std::function<void()> pfn) noexcept
{
    _api.swapChainChangedCallback = std::move(pfn);
}

void AtlasEngine::SetDefaultTextBackgroundOpacity(const float opacity) noexcept
{
    _api.backgroundOpaqueMixin = opacity == 1.0f ? 0xff000000 : 0x00000000;
    WI_SetFlag(_api.invalidations, ApiInvalidations::Device);
}

void AtlasEngine::SetForceFullRepaintRendering(bool enable) noexcept
{
}

[[nodiscard]] HRESULT AtlasEngine::SetHwnd(const HWND hwnd) noexcept
{
    _api.hwnd = hwnd;
    WI_SetFlag(_api.invalidations, ApiInvalidations::Device);
    return S_OK;
}

void AtlasEngine::SetPixelShaderPath(std::wstring_view value) noexcept
{
}

void AtlasEngine::SetRetroTerminalEffect(bool enable) noexcept
{
}

void AtlasEngine::SetSelectionBackground(const COLORREF color, const float alpha) noexcept
{
    const u32 selectionColor = (color & 0xffffff) | gsl::narrow_cast<u32>(std::lroundf(alpha * 255.0f)) << 24;

    if (_api.selectionColor != selectionColor)
    {
        _api.selectionColor = selectionColor;
        WI_SetFlag(_api.invalidations, ApiInvalidations::Settings);
    }
}

void AtlasEngine::SetSoftwareRendering(bool enable) noexcept
{
}

void AtlasEngine::SetIntenseIsBold(bool enable) noexcept
{
}

void AtlasEngine::SetWarningCallback(std::function<void(HRESULT)> pfn) noexcept
{
    _api.warningCallback = std::move(pfn);
}

[[nodiscard]] HRESULT AtlasEngine::SetWindowSize(const SIZE pixels) noexcept
{
    u16x2 newSize;
    RETURN_IF_FAILED(vec2_narrow(pixels.cx, pixels.cy, newSize));

    // At the time of writing:
    // When Win+D is pressed a render pass is initiated. As conhost is in the background, GetClientRect will return {0,0}.
    // This isn't a valid value for _api.sizeInPixel and would crash _recreateSizeDependentResources().
    if (_api.sizeInPixel != newSize && newSize != u16x2{})
    {
        _api.sizeInPixel = newSize;
        _api.cellCount = _api.sizeInPixel / _api.cellSize;
        WI_SetFlag(_api.invalidations, ApiInvalidations::Size);
    }

    return S_OK;
}

void AtlasEngine::ToggleShaderEffects() noexcept
{
}

[[nodiscard]] HRESULT AtlasEngine::UpdateFont(const FontInfoDesired& fontInfoDesired, FontInfo& fontInfo, const std::unordered_map<std::wstring_view, uint32_t>& features, const std::unordered_map<std::wstring_view, float>& axes) noexcept
try
{
    // This must be the first action as it actually modifies fontInfo, which we use later on in this function.
    RETURN_IF_FAILED(GetProposedFont(fontInfoDesired, fontInfo, _api.dpi));

    u16x2 newSize;
    {
        const auto size = fontInfo.GetSize();
        RETURN_IF_FAILED(vec2_narrow(size.X, size.Y, newSize));
    }

    std::vector<DWRITE_FONT_FEATURE> fontFeatures;
    if (!features.empty())
    {
        // All of these features are enabled by default by DirectWrite.
        // If you want to (and can) peek into the source of DirectWrite
        // you can look for the "GenericDefaultGsubFeatures" and "GenericDefaultGposFeatures" arrays.
        // Gsub is for GetGlyphs() and Gpos for GetGlyphPlacements().
        //
        // These values are sorted by their numeric value on little endian (you can see them in a debugger).
        // Please try to keep it that way.
        //
        // GH#10774: Apparently specifying all of the features is just redundant.
        static constexpr auto defaults = std::array{
            DWRITE_FONT_FEATURE_TAG_STANDARD_LIGATURES,
            DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_LIGATURES,
            DWRITE_FONT_FEATURE_TAG_KERNING,
            DWRITE_FONT_FEATURE_TAG_CONTEXTUAL_ALTERNATES,
        };
        std::array<bool, defaults.size()> overriddenDefaults{};

        for (const auto& p : features)
        {
            if (p.first.size() == 4)
            {
                const auto s = p.first.data();
                const auto tag = DWRITE_MAKE_FONT_FEATURE_TAG(s[0], s[1], s[2], s[3]);

                fontFeatures.emplace_back(DWRITE_FONT_FEATURE{ tag, p.second });

                for (size_t i = 0; i < defaults.size(); ++i)
                {
                    if (defaults[i] == tag)
                    {
                        overriddenDefaults[i] = true;
                        break;
                    }
                }
            }
        }

        for (size_t i = 0; i < defaults.size(); ++i)
        {
            if (!overriddenDefaults[i])
            {
                fontFeatures.emplace_back(DWRITE_FONT_FEATURE{ defaults[i], 1 });
            }
        }
    }

    std::vector<DWRITE_FONT_AXIS_VALUE> fontAxisValues;
    if (!axes.empty())
    {
        fontAxisValues.reserve(axes.size());

        for (const auto& p : axes)
        {
            if (p.first.size() == 4)
            {
                const auto s = p.first.data();
                fontAxisValues.emplace_back(DWRITE_FONT_AXIS_VALUE{ DWRITE_MAKE_FONT_AXIS_TAG(s[0], s[1], s[2], s[3]), p.second });
            }
        }
    }

    const auto& faceName = fontInfo.GetFaceName();
    auto fontName = wil::make_process_heap_string(faceName.data(), faceName.size());
    RETURN_IF_NULL_ALLOC(fontName);

    // NOTE: From this point onward no early returns or throwing code should exist,
    // as we might cause _api to be in an inconsistent state otherwise.

    _api.fontFeatures = std::move(fontFeatures);
    _api.fontAxisValues = std::move(fontAxisValues);
    _api.fontName = std::move(fontName);
    _api.fontSize = fontInfo.GetUnscaledSize().Y;
    _api.fontWeight = gsl::narrow_cast<u16>(fontInfo.GetWeight());
    WI_SetFlag(_api.invalidations, ApiInvalidations::Font);

    if (newSize != _api.cellSize)
    {
        _api.cellSize = newSize;
        _api.cellCount = _api.sizeInPixel / _api.cellSize;
        WI_SetFlag(_api.invalidations, ApiInvalidations::Size);
    }

    return S_OK;
}
CATCH_RETURN()

void AtlasEngine::UpdateHyperlinkHoveredId(const uint16_t hoveredId) noexcept
{
}

#pragma endregion
