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

WddmConEngine::WddmConEngine() :
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
    FreeResources(_displayHeight);
}

[[nodiscard]] HRESULT WddmConEngine::Initialize() noexcept
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
                DisplaySize.bottom = (LONG)DisplaySizeIoctl.Height;
                DisplaySize.right = (LONG)DisplaySizeIoctl.Width;

                _displayState = (PCD_IO_ROW_INFORMATION*)calloc(DisplaySize.bottom, sizeof(PCD_IO_ROW_INFORMATION));

                if (_displayState != nullptr)
                {
                    for (LONG i = 0; i < DisplaySize.bottom; i++)
                    {
                        _displayState[i] = (PCD_IO_ROW_INFORMATION)calloc(1, sizeof(CD_IO_ROW_INFORMATION));
                        if (_displayState[i] == nullptr)
                        {
                            hr = E_OUTOFMEMORY;

                            break;
                        }

                        _displayState[i]->Index = (SHORT)i;
                        _displayState[i]->Old = (PCD_IO_CHARACTER)calloc(DisplaySize.right, sizeof(CD_IO_CHARACTER));
                        _displayState[i]->New = (PCD_IO_CHARACTER)calloc(DisplaySize.right, sizeof(CD_IO_CHARACTER));

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

bool WddmConEngine::IsInitialized()
{
    return _hWddmConCtx != INVALID_HANDLE_VALUE;
}

[[nodiscard]] HRESULT WddmConEngine::Enable() noexcept
{
    RETURN_IF_HANDLE_INVALID(_hWddmConCtx);
    return WDDMConEnableDisplayAccess((PHANDLE)_hWddmConCtx, TRUE);
}

[[nodiscard]] HRESULT WddmConEngine::Disable() noexcept
{
    RETURN_IF_HANDLE_INVALID(_hWddmConCtx);
    return WDDMConEnableDisplayAccess((PHANDLE)_hWddmConCtx, FALSE);
}

[[nodiscard]] HRESULT WddmConEngine::Invalidate(const SMALL_RECT* const /*psrRegion*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateCursor(const COORD* const /*pcoordCursor*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateSystem(const RECT* const /*prcDirtyClient*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateSelection(const std::vector<SMALL_RECT>& /*rectangles*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateScroll(const COORD* const /*pcoordDelta*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateAll() noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::InvalidateCircling(_Out_ bool* const pForcePaint) noexcept
{
    *pForcePaint = false;
    return S_FALSE;
}

[[nodiscard]] HRESULT WddmConEngine::PrepareForTeardown(_Out_ bool* const pForcePaint) noexcept
{
    *pForcePaint = false;
    return S_FALSE;
}

[[nodiscard]] HRESULT WddmConEngine::StartPaint() noexcept
{
    RETURN_IF_HANDLE_INVALID(_hWddmConCtx);
    return WDDMConBeginUpdateDisplayBatch(_hWddmConCtx);
}

[[nodiscard]] HRESULT WddmConEngine::EndPaint() noexcept
{
    RETURN_IF_HANDLE_INVALID(_hWddmConCtx);
    return WDDMConEndUpdateDisplayBatch(_hWddmConCtx);
}

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
    RETURN_IF_HANDLE_INVALID(_hWddmConCtx);

    PCD_IO_CHARACTER OldChar;
    PCD_IO_CHARACTER NewChar;

    for (LONG rowIndex = 0; rowIndex < _displayHeight; rowIndex++)
    {
        for (LONG colIndex = 0; colIndex < _displayWidth; colIndex++)
        {
            OldChar = &_displayState[rowIndex]->Old[colIndex];
            NewChar = &_displayState[rowIndex]->New[colIndex];

            OldChar->Character = NewChar->Character;
            OldChar->Atribute = NewChar->Atribute;

            NewChar->Character = L' ';
            NewChar->Atribute = 0x0;
        }
    }

    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::PaintBufferLine(std::basic_string_view<Cluster> const clusters,
                                                     const COORD coord,
                                                     const bool /*trimLeft*/) noexcept
{
    try
    {
        RETURN_IF_HANDLE_INVALID(_hWddmConCtx);

        PCD_IO_CHARACTER OldChar;
        PCD_IO_CHARACTER NewChar;

        for (size_t i = 0; i < clusters.size() && i < (size_t)_displayWidth; i++)
        {
            OldChar = &_displayState[coord.Y]->Old[coord.X + i];
            NewChar = &_displayState[coord.Y]->New[coord.X + i];

            OldChar->Character = NewChar->Character;
            OldChar->Atribute = NewChar->Atribute;

            NewChar->Character = clusters.at(i).GetTextAsSingle();
            NewChar->Atribute = _currentLegacyColorAttribute;
        }

        return WDDMConUpdateDisplay(_hWddmConCtx, _displayState[coord.Y], FALSE);
    }
    CATCH_RETURN();
}

[[nodiscard]] HRESULT WddmConEngine::PaintBufferGridLines(GridLines const /*lines*/,
                                                          COLORREF const /*color*/,
                                                          size_t const /*cchLine*/,
                                                          COORD const /*coordTarget*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::PaintSelection(const SMALL_RECT /*rect*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::PaintCursor(const IRenderEngine::CursorOptions& /*options*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::UpdateDrawingBrushes(COLORREF const /*colorForeground*/,
                                                          COLORREF const /*colorBackground*/,
                                                          const WORD legacyColorAttribute,
                                                          const ExtendedAttributes /*extendedAttrs*/,
                                                          bool const /*isSettingDefaultBrushes*/) noexcept
{
    _currentLegacyColorAttribute = legacyColorAttribute;

    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::UpdateFont(const FontInfoDesired& fiFontInfoDesired, FontInfo& fiFontInfo) noexcept
{
    return GetProposedFont(fiFontInfoDesired, fiFontInfo, USER_DEFAULT_SCREEN_DPI);
}

[[nodiscard]] HRESULT WddmConEngine::UpdateDpi(int const /*iDpi*/) noexcept
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
[[nodiscard]] HRESULT WddmConEngine::UpdateViewport(const SMALL_RECT /*srNewViewport*/) noexcept
{
    return S_OK;
}

[[nodiscard]] HRESULT WddmConEngine::GetProposedFont(const FontInfoDesired& /*fiFontInfoDesired*/,
                                                     FontInfo& fiFontInfo,
                                                     int const /*iDpi*/) noexcept
{
    COORD coordSize = { 0 };
    LOG_IF_FAILED(GetFontSize(&coordSize));

    fiFontInfo.SetFromEngine(fiFontInfo.GetFaceName(),
                             fiFontInfo.GetFamily(),
                             fiFontInfo.GetWeight(),
                             fiFontInfo.IsTrueTypeFont(),
                             coordSize,
                             coordSize);

    return S_OK;
}

SMALL_RECT WddmConEngine::GetDirtyRectInChars()
{
    SMALL_RECT r;
    r.Bottom = _displayHeight > 0 ? (SHORT)(_displayHeight - 1) : 0;
    r.Top = 0;
    r.Left = 0;
    r.Right = _displayWidth > 0 ? (SHORT)(_displayWidth - 1) : 0;

    return r;
}

RECT WddmConEngine::GetDisplaySize()
{
    RECT r;
    r.top = 0;
    r.left = 0;
    r.bottom = _displayHeight;
    r.right = _displayWidth;

    return r;
}

[[nodiscard]] HRESULT WddmConEngine::GetFontSize(_Out_ COORD* const pFontSize) noexcept
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
    COORD c;
    c.X = DEFAULT_FONT_WIDTH;
    c.Y = DEFAULT_FONT_HEIGHT;

    *pFontSize = c;
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
[[nodiscard]] HRESULT WddmConEngine::_DoUpdateTitle(_In_ const std::wstring& /*newTitle*/) noexcept
{
    return S_OK;
}
