// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "WddmConRenderer.hpp"

#include "main.h"

#pragma hdrstop

//
// Default non-bright white.
//

#define DEFAULT_COLOR_ATTRIBUTE (0xC)

#define DEFAULT_FONT_WIDTH (8)
#define DEFAULT_FONT_HEIGHT (12)

using namespace Microsoft::Console::Render;

WddmConEngine::WddmConEngine() noexcept :
    RenderEngineBase(),
    _hWddmConCtx(INVALID_HANDLE_VALUE),
    _displayHeight(0),
    _displayWidth(0),
    _displayState(nullptr),
    _currentLegacyColorAttribute(DEFAULT_COLOR_ATTRIBUTE)
{
}

void WddmConEngine::FreeResources(ULONG displayHeight)
{
    if (_displayState)
    {
        for (ULONG i = 0; i < displayHeight; i++)
        {
            if (_displayState[i])
            {
                if (_displayState[i]->Old)
                {
                    free(_displayState[i]->Old);
                }
                if (_displayState[i]->New)
                {
                    free(_displayState[i]->New);
                }

                free(_displayState[i]);
            }
        }

        free(_displayState);
    }

    if (_hWddmConCtx != INVALID_HANDLE_VALUE)
    {
        WDDMConDestroy(_hWddmConCtx);
        _hWddmConCtx = INVALID_HANDLE_VALUE;
    }
}

WddmConEngine::~WddmConEngine()
{
#pragma warning(suppress : 26447)
    FreeResources(_displayHeight);
}

[[nodiscard]] HRESULT WddmConEngine::Initialize()
{
    HRESULT hr;
    RECT DisplaySize;
    CD_IO_DISPLAY_SIZE DisplaySizeIoctl;

    if (_hWddmConCtx == INVALID_HANDLE_VALUE)
    {
        hr = WDDMConCreate(&_hWddmConCtx);

        if (SUCCEEDED(hr))
        {
            hr = WDDMConGetDisplaySize(_hWddmConCtx, &DisplaySizeIoctl);

            if (SUCCEEDED(hr))
            {
                DisplaySize.top = 0;
                DisplaySize.left = 0;
                DisplaySize.bottom = gsl::narrow_cast<LONG>(DisplaySizeIoctl.Height);
                DisplaySize.right = gsl::narrow_cast<LONG>(DisplaySizeIoctl.Width);

                _displayState = static_cast<PCD_IO_ROW_INFORMATION*>(calloc(DisplaySize.bottom, sizeof(PCD_IO_ROW_INFORMATION)));

                if (_displayState != nullptr)
                {
                    for (LONG i = 0; i < DisplaySize.bottom; i++)
                    {
                        _displayState[i] = static_cast<PCD_IO_ROW_INFORMATION>(calloc(1, sizeof(CD_IO_ROW_INFORMATION)));
                        if (_displayState[i] == nullptr)
                        {
                            hr = E_OUTOFMEMORY;

                            break;
                        }

                        _displayState[i]->Index = gsl::narrow_cast<SHORT>(i);
                        _displayState[i]->Old = static_cast<PCD_IO_CHARACTER>(calloc(DisplaySize.right, sizeof(CD_IO_CHARACTER)));
                        _displayState[i]->New = static_cast<PCD_IO_CHARACTER>(calloc(DisplaySize.right, sizeof(CD_IO_CHARACTER)));

                        if (_displayState[i]->Old == nullptr || _displayState[i]->New == nullptr)
                        {
                            hr = E_OUTOFMEMORY;

                            break;
                        }
                    }

                    if (SUCCEEDED(hr))
                    {
                        _displayHeight = DisplaySize.bottom;
                        _displayWidth = DisplaySize.right;
                    }
                    else
                    {
                        FreeResources(DisplaySize.bottom);
                    }
                }
                else
                {
                    WDDMConDestroy(_hWddmConCtx);

                    hr = E_OUTOFMEMORY;
                }
            }
            else
            {
                WDDMConDestroy(_hWddmConCtx);
            }
        }
    }
    else
    {
        hr = E_HANDLE;
    }

    return hr;
}

bool WddmConEngine::IsInitialized() noexcept
{
    return _hWddmConCtx != INVALID_HANDLE_VALUE;
}

[[nodiscard]] HRESULT WddmConEngine::Enable() noexcept
try
{
    RETURN_LAST_ERROR_IF(_hWddmConCtx == INVALID_HANDLE_VALUE);
    return WDDMConEnableDisplayAccess(_hWddmConCtx, TRUE);
}
CATCH_RETURN()

[[nodiscard]] HRESULT WddmConEngine::Disable() noexcept
try
{
    RETURN_LAST_ERROR_IF(_hWddmConCtx == INVALID_HANDLE_VALUE);
    return WDDMConEnableDisplayAccess(_hWddmConCtx, FALSE);
}
CATCH_RETURN()

[[nodiscard]] HRESULT WddmConEngine::Invalidate(const til::rect* const /*psrRegion*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateCursor(const til::rect* const /*psrRegion*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateSystem(const til::rect* const /*prcDirtyClient*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateSelection(const std::vector<til::rect>& /*rectangles*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateScroll(const til::point* const /*pcoordDelta*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateAll() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept
{
    *pForcePaint = false;
    return S_FALSE;
}

[[nodiscard]] HRESULT WddmConEngine::StartPaint() noexcept
try
{
    RETURN_LAST_ERROR_IF(_hWddmConCtx == INVALID_HANDLE_VALUE);
    return WDDMConBeginUpdateDisplayBatch(_hWddmConCtx);
}
CATCH_RETURN()

[[nodiscard]] HRESULT WddmConEngine::EndPaint() noexcept
try
{
    RETURN_LAST_ERROR_IF(_hWddmConCtx == INVALID_HANDLE_VALUE);
    return WDDMConEndUpdateDisplayBatch(_hWddmConCtx);
}
CATCH_RETURN()

// Routine Description:
// - Used to perform longer running presentation steps outside the lock so the other threads can continue.
// - Not currently used by WddmConEngine.
// Arguments:
// - <none>
// Return Value:
// - S_FALSE since we do nothing.

[[nodiscard]] HRESULT WddmConEngine::Present() noexcept
{
    return S_FALSE;
}

[[nodiscard]] HRESULT WddmConEngine::ScrollFrame() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::PaintBackground() noexcept
{
    RETURN_LAST_ERROR_IF(_hWddmConCtx == INVALID_HANDLE_VALUE);

    for (LONG rowIndex = 0; rowIndex < _displayHeight; rowIndex++)
    {
        for (LONG colIndex = 0; colIndex < _displayWidth; colIndex++)
        {
            const auto OldChar = &_displayState[rowIndex]->Old[colIndex];
            const auto NewChar = &_displayState[rowIndex]->New[colIndex];

            OldChar->Character = NewChar->Character;
            OldChar->Attribute = NewChar->Attribute;

            NewChar->Character = L' ';
            NewChar->Attribute = 0x0;
        }
    }

    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::PaintBufferLine(const std::span<const Cluster> clusters,
                                                     const til::point coord,
                                                     const bool /*trimLeft*/,
                                                     const bool /*lineWrapped*/) noexcept
{
    try
    {
        RETURN_LAST_ERROR_IF(_hWddmConCtx == INVALID_HANDLE_VALUE);

        for (size_t i = 0; i < clusters.size() && i < gsl::narrow_cast<size_t>(_displayWidth); i++)
        {
            const auto OldChar = &_displayState[coord.y]->Old[coord.x + i];
            const auto NewChar = &_displayState[coord.y]->New[coord.x + i];

            OldChar->Character = NewChar->Character;
            OldChar->Attribute = NewChar->Attribute;

            NewChar->Character = til::at(clusters, i).GetTextAsSingle();
            NewChar->Attribute = _currentLegacyColorAttribute;
        }

        return WDDMConUpdateDisplay(_hWddmConCtx, _displayState[coord.y], FALSE);
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT WddmConEngine::PaintBufferGridLines(GridLineSet const /*lines*/,
                                                          COLORREF const /*color*/,
                                                          size_t const /*cchLine*/,
                                                          const til::point /*coordTarget*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::PaintSelection(const til::rect& /*rect*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::PaintCursor(const CursorOptions& /*options*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::UpdateDrawingBrushes(const TextAttribute& textAttributes,
                                                          const RenderSettings& /*renderSettings*/,
                                                          const gsl::not_null<IRenderData*> /*pData*/,
                                                          const bool /*usingSoftFont*/,
                                                          const bool /*isSettingDefaultBrushes*/) noexcept
{
    _currentLegacyColorAttribute = textAttributes.GetLegacyAttributes();

    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept
{
    return GetProposedFont(fiFontInfoDesired, fiFontInfo, USER_DEFAULT_SCREEN_DPI);
}

[[nodiscard]] HRESULT WddmConEngine::UpdateDpi(const int /*iDpi*/) noexcept
{
    return S_OK;
}

// Method Description:
// - This method will update our internal reference for how big the viewport is.
//      Does nothing for WDDMCon.
// Arguments:
// - srNewViewport - The bounds of the new viewport.
// Return Value:
// - HRESULT S_OK
[[nodiscard]] HRESULT WddmConEngine::UpdateViewport(const til::inclusive_rect& /*srNewViewport*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::GetProposedFont(const FontInfoDesired& /*fiFontInfoDesired*/,
                                                     FontInfo& fiFontInfo,
                                                     const int /*iDpi*/) noexcept
{
    til::size coordSize;
#pragma warning(suppress : 26447)
    LOG_IF_FAILED(GetFontSize(&coordSize));

    fiFontInfo.SetFromEngine(fiFontInfo.GetFaceName(),
                             fiFontInfo.GetFamily(),
                             fiFontInfo.GetWeight(),
                             fiFontInfo.IsTrueTypeFont(),
                             coordSize,
                             coordSize);

    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::GetDirtyArea(std::span<const til::rect>& area) noexcept
{
    _dirtyArea.bottom = std::max<LONG>(0, _displayHeight);
    _dirtyArea.right = std::max<LONG>(0, _displayWidth);

    area = { &_dirtyArea,
             1 };

    return S_OK;
}

til::rect WddmConEngine::GetDisplaySize() noexcept
{
    til::rect r;
    r.top = 0;
    r.left = 0;
    r.bottom = _displayHeight;
    r.right = _displayWidth;

    return r;
}

[[nodiscard]] HRESULT WddmConEngine::GetFontSize(_Out_ til::size* const pFontSize) noexcept
{
    // In order to retrieve the font size being used by DirectX, it is necessary
    // to modify the API set that defines the contract for WddmCon. However, the
    // intention is to subsume WddmCon into ConhostV2 directly once the issue of
    // building in the OneCore 'depot' including DirectX headers and libs is
    // resolved. The font size has no bearing on the behavior of the console
    // since it is used to determine the invalid rectangle whenever the console
    // buffer changes. However, given that no invalidation logic exists for this
    // renderer, the value returned by this function is irrelevant.
    //
    // TODO: MSFT 11851921 - Subsume WddmCon into ConhostV2 and remove the API
    //       set extension.
    *pFontSize = { DEFAULT_FONT_WIDTH, DEFAULT_FONT_HEIGHT };
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::IsGlyphWideByFont(const std::wstring_view /*glyph*/, _Out_ bool* const pResult) noexcept
{
    *pResult = false;
    return S_OK;
}

// Method Description:
// - Updates the window's title string.
//      Does nothing for WddmCon.
// Arguments:
// - newTitle: the new string to use for the title of the window
// Return Value:
// - S_OK
[[nodiscard]] HRESULT WddmConEngine::_DoUpdateTitle(_In_ const std::wstring_view /*newTitle*/) noexcept
{
    return S_OK;
}
