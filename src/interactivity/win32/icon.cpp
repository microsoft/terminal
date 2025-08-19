// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "icon.hpp"

#include "window.hpp"

#include "../inc/ServiceLocator.hpp"
#include "windowdpiapi.hpp"

using namespace Microsoft::Console::Interactivity::Win32;

// This region contains excerpts from ExtractIconExW and all callees, tuned for the Console use case.
// Including this here helps us avoid a load-time dependency on shell32 or Windows.Storage.dll.
static constexpr uint32_t ILD_HIGHQUALITYSCALE{ 0x10000 };
static constexpr uint32_t LR_EXACTSIZEONLY{ 0x10000 };

// System icons come in several standard sizes.
// This function returns the snap size best suited for an arbitrary size. Note that
// larger than 256 returns the input value, which would result in the 256px icon being used.
static int SnapIconSize(int cx)
{
    static constexpr int rgSizes[] = { 16, 24, 32, 48, 64, 256 };
    for (auto sz : rgSizes)
    {
        if (cx <= sz)
        {
            return sz;
        }
    }
    return cx;
}

static HRESULT GetIconSize(HICON hIcon, PSIZE pSize)
{
    pSize->cx = pSize->cy = 32;

    // If it's a cursor, we'll fail so that we end up using our fallback case instead.
    ICONINFO iconInfo;
    if (GetIconInfo(hIcon, &iconInfo))
    {
        auto cleanup = wil::scope_exit([&] {
            DeleteObject(iconInfo.hbmMask);
            if (iconInfo.hbmColor)
            {
                DeleteObject(iconInfo.hbmColor);
            }
        });

        if (iconInfo.fIcon)
        {
            BITMAP bmp;
            if (GetObject(iconInfo.hbmColor, sizeof(bmp), &bmp))
            {
                pSize->cx = bmp.bmWidth;
                pSize->cy = bmp.bmHeight;
                return S_OK;
            }
        }
    }

    return E_FAIL;
}

#define SET_HR_AND_BREAK_IF_FAILED(_hr) \
    {                                   \
        const auto __hrRet = (_hr);     \
        hr = __hrRet;                   \
        if (FAILED(hr))                 \
        {                               \
            break;                      \
        }                               \
    }

static HRESULT CreateSmallerIcon(HICON hicon, UINT cx, UINT cy, HICON* phico)
{
    *phico = nullptr;
    HRESULT hr = S_OK;

    do
    {
        SIZE size;
        SET_HR_AND_BREAK_IF_FAILED(GetIconSize(hicon, &size));

        if ((size.cx == (int)cx) && (size.cy == (int)cy))
        {
            *phico = hicon;
            hr = S_FALSE;
            break;
        }

        if ((size.cx < (int)cx) || (size.cy < (int)cy))
        {
            // If we're passed in a smaller icon than desired, we have a choice; we can either fail altogether, or we could scale it up.
            // Failing would make it the client's responsibility to figure out what to do, which sounds like more work
            // So instead, we just create an icon the best we can.
            // We'll use the fallback below for this.
            break;
        }

        wil::unique_hmodule comctl32{ LoadLibraryExW(L"comctl32.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32) };
        if (auto ILCoCreateInstance = GetProcAddressByFunctionDeclaration(comctl32.get(), ImageList_CoCreateInstance))
        {
            wil::com_ptr_nothrow<IImageList2> pimlOriginal;
            wil::com_ptr_nothrow<IImageList2> pimlIcon;
            int fRet = -1;

            SET_HR_AND_BREAK_IF_FAILED(ILCoCreateInstance(CLSID_ImageList, NULL, IID_PPV_ARGS(&pimlOriginal)));
            SET_HR_AND_BREAK_IF_FAILED(pimlOriginal->Initialize(size.cx, size.cy, ILC_COLOR32 | ILC_MASK | ILC_HIGHQUALITYSCALE, 1, 1));
            SET_HR_AND_BREAK_IF_FAILED(pimlOriginal->ReplaceIcon(-1, hicon, &fRet));
            SET_HR_AND_BREAK_IF_FAILED(ILCoCreateInstance(CLSID_ImageList, NULL, IID_PPV_ARGS(&pimlIcon)));
            SET_HR_AND_BREAK_IF_FAILED(pimlIcon->Initialize(cx, cy, ILC_COLOR32 | ILC_MASK | ILC_HIGHQUALITYSCALE, 1, 1));
            SET_HR_AND_BREAK_IF_FAILED(pimlIcon->SetImageCount(1));
            SET_HR_AND_BREAK_IF_FAILED(pimlIcon->ReplaceFromImageList(0, pimlOriginal.get(), 0, NULL, 0));
            SET_HR_AND_BREAK_IF_FAILED(pimlIcon->GetIcon(0, ILD_HIGHQUALITYSCALE, phico));
        }
    } while (0);

    if (!*phico)
    {
        // For whatever reason, we still don't have an icon. Maybe we have a cursor. At any rate,
        // we'll use CopyImage as a last-ditch effort.
        *phico = (HICON)CopyImage(hicon, IMAGE_ICON, cx, cy, LR_COPYFROMRESOURCE);
        hr = *phico ? S_OK : E_FAIL;
    }

    return hr;
}

static UINT ConExtractIcons(PCWSTR szFileName, int nIconIndex, int cxIcon, int cyIcon, HICON* phicon, UINT nIcons, UINT lrFlags)
{
    UINT result = (UINT)-1;
    std::wstring expandedPath, finalPath;

    std::fill_n(phicon, nIcons, nullptr);

    if (FAILED(wil::ExpandEnvironmentStringsW(szFileName, expandedPath)))
    {
        return result;
    }

    if (FAILED(wil::SearchPathW(nullptr, expandedPath.c_str(), nullptr, finalPath)))
    {
        return result;
    }

    int snapcx = SnapIconSize(LOWORD(cxIcon));
    int snapcy = SnapIconSize(LOWORD(cyIcon));

    // PrivateExtractIconsW can extract two sizes of icons, and does this by having the client
    // pass in both in one argument. If that's the case, we have to compute the snap sizes for
    // both requested sizes.

    if (nIcons == 2)
    {
        snapcx = MAKELONG(snapcx, SnapIconSize(HIWORD(cxIcon)));
        snapcy = MAKELONG(snapcy, SnapIconSize(HIWORD(cyIcon)));
    }

    // When we're in high dpi mode, we need to get larger icons and scale them down, rather than
    // the default user32 behaviour of taking the smaller icon and scaling it up.
    if ((cxIcon != 0) && (cyIcon != 0) && (nIcons <= 2) && (!((snapcx == cxIcon) && (snapcy == cyIcon))))
    {
        // The icon asked for doesn't match one of the standard snap sizes but the file may have
        // that size in it anyway - eg 20x20, 64x64, etc.  Try to get the requested size and if
        // it's not present get the snap size and scale it down to the requested size.

        // PrivateExtractIconsW will fail if you ask for 2 icons and only 1 size if there is only 1 icon in the
        // file, even if the one in there matches the one you want.  So, if the caller only specified one
        // size, only ask for 1 icon.
        UINT nIconsActual = (HIWORD(cxIcon)) ? nIcons : 1;
        result = PrivateExtractIconsW(finalPath.c_str(), nIconIndex, cxIcon, cyIcon, phicon, nullptr, nIconsActual, lrFlags | LR_EXACTSIZEONLY);
        if (nIconsActual != result)
        {
            HICON ahTempIcon[2] = { 0 };

            // If there is no exact match the API can return 0 but phicon[0] set to a valid hicon.  In that
            // case destroy the icon and reset the entry.
            UINT i;
            for (i = 0; i < nIcons; i++)
            {
                if (phicon[i])
                {
                    DestroyIcon(phicon[i]);
                    phicon[i] = nullptr;
                }
            }

            // The size we want is not present, go ahead and scale the image.
            result = PrivateExtractIconsW(finalPath.c_str(), nIconIndex, snapcx, snapcy, ahTempIcon, nullptr, nIcons, lrFlags | LR_EXACTSIZEONLY);

            if ((int)result > 0)
            {
                // If CreateSmallerIcon returns S_FALSE, it means the passed in copy is already the correct size
                // We don't want to destroy it, so null it out as appropriate
                HRESULT hr = CreateSmallerIcon(ahTempIcon[0], LOWORD(cxIcon), LOWORD(cyIcon), phicon);
                if (FAILED(hr))
                {
                    result = (UINT)-1;
                }
                else if (hr == S_FALSE)
                {
                    ahTempIcon[0] = nullptr;
                }

                if (SUCCEEDED(hr) && ((int)result > 1))
                {
                    __analysis_assume(result < nIcons); // if PrivateExtractIconsW could be annotated with a range + error value(-1) this wouldn't be needed
                    hr = CreateSmallerIcon(ahTempIcon[1], HIWORD(cxIcon), HIWORD(cyIcon), phicon + 1);
                    if (FAILED(hr))
                    {
                        DestroyIcon(phicon[0]);
                        phicon[0] = nullptr;

                        result = (UINT)-1;
                    }
                    else if (hr == S_FALSE)
                    {
                        ahTempIcon[1] = nullptr;
                    }
                }
            }
            if (ahTempIcon[0])
            {
                DestroyIcon(ahTempIcon[0]);
            }
            if (ahTempIcon[1])
            {
                DestroyIcon(ahTempIcon[1]);
            }
        }
    }

    if (phicon[0] == nullptr)
    {
        // Okay, now get USER to do the extraction if all else failed
        result = PrivateExtractIconsW(finalPath.c_str(), nIconIndex, cxIcon, cyIcon, phicon, nullptr, nIcons, lrFlags);
    }
    return result;
}

static UINT ConExtractIconInBothSizesW(int dpi, PCWSTR szFileName, int nIconIndex, HICON* phiconLarge, HICON* phiconSmall)
{
    HICON ahicon[2] = { nullptr, nullptr };
    auto result = ConExtractIcons(
        szFileName,
        nIconIndex,
        MAKELONG(GetSystemMetricsForDpi(dpi, SM_CXICON), GetSystemMetricsForDpi(dpi, SM_CXSMICON)),
        MAKELONG(GetSystemMetricsForDpi(dpi, SM_CYICON), GetSystemMetricsForDpi(dpi, SM_CYSMICON)),
        ahicon,
        2,
        0);

    *phiconLarge = ahicon[0];
    *phiconSmall = ahicon[1];

    return result;
}
// Excerpted Region Ends

Icon::Icon()
{
#pragma warning(push)
#pragma warning(disable : 4302) // typecast warning from MAKEINTRESOURCE
    _hDefaultIcon = LoadIconW(nullptr, MAKEINTRESOURCE(IDI_APPLICATION));
    _hDefaultSmIcon = (HICON)LoadImageW(nullptr,
                                        MAKEINTRESOURCE(IDI_APPLICATION),
                                        IMAGE_ICON,
                                        GetSystemMetrics(SM_CXSMICON),
                                        GetSystemMetrics(SM_CYSMICON),
                                        LR_SHARED);
#pragma warning(pop)
}

// Routine Description:
// - Returns the singleton instance of the Icon
// Arguments:
// - <none>
// Return Value:
// - Reference to the singleton.
Icon& Icon::Instance()
{
    static Icon i;
    return i;
}

// Routine Description:
// - Gets the requested icons. Will return default icons if no icons have been set.
// Arguments:
// - phIcon - The large icon representation.
// - phSmIcon - The small icon representation.
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Icon::GetIcons(int dpi, _Out_opt_ HICON* const phIcon, _Out_opt_ HICON* const phSmIcon)
{
    auto found{ _iconHandlesPerDpi.find(dpi) };
    if (found == _iconHandlesPerDpi.end())
    {
        std::ignore = LoadIconsForDpi(dpi);
        found = _iconHandlesPerDpi.find(dpi);
    }

    if (phIcon)
    {
        if (found != _iconHandlesPerDpi.end())
        {
            *phIcon = found->second.first.get();
        }
        if (!*phIcon)
        {
            *phIcon = _hDefaultIcon;
        }
        if (!*phIcon)
        {
            return E_FAIL;
        }
    }

    if (phSmIcon)
    {
        if (found != _iconHandlesPerDpi.end())
        {
            *phSmIcon = found->second.second.get();
        }
        if (!*phSmIcon)
        {
            *phSmIcon = _hDefaultSmIcon;
        }
        if (!*phSmIcon)
        {
            return E_FAIL;
        }
    }

    return S_OK;
}

// Routine Description:
// - Loads icons from a given path on the file system.
// - Will only load one icon from the file.
// Arguments:
// - pwszIconLocation - File system path to extract icons from
// - nIconIndex - Index offset of the icon within the file
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Icon::LoadIconsFromPath(_In_ PCWSTR pwszIconLocation, const int nIconIndex)
{
    _iconPathAndIndex = { std::wstring{ pwszIconLocation }, nIconIndex };
    _iconHandlesPerDpi.clear();
    return LoadIconsForDpi(96);
}

[[nodiscard]] HRESULT Icon::LoadIconsForDpi(int dpi)
{
    wil::unique_hicon icon, smIcon;
    ConExtractIconInBothSizesW(dpi, _iconPathAndIndex.first.c_str(), _iconPathAndIndex.second, &icon, &smIcon);
    if (!icon)
    {
        return E_FAIL;
    }

    _iconHandlesPerDpi.try_emplace(dpi, std::pair<wil::unique_hicon, wil::unique_hicon>{ std::move(icon), std::move(smIcon) });
    return S_OK;
}

// Routine Description:
// - Workaround for an oddity in WM_GETICON.
// - If you never call WM_SETICON and the system would have to look into the window class to get the icon,
//   then any call to WM_GETICON will return NULL for the specified icon instead of returning the window class value.
//   By calling WM_SETICON once, we ensure that third-party apps calling WM_GETICON will receive the icon we specify.
// Arguments:
// - hwnd - Handle to apply message workaround to.
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Icon::ApplyIconsToWindow(const HWND hwnd)
{
    HICON hIcon, hSmIcon;

    const auto dpi = ServiceLocator::LocateHighDpiApi<WindowDpiApi>()->GetDpiForWindow(hwnd);
    RETURN_IF_FAILED(GetIcons(dpi, &hIcon, &hSmIcon));

    SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmIcon);

    return S_OK;
}
