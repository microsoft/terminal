// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "icon.hpp"

#include "window.hpp"

#include "../inc/ServiceLocator.hpp"

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
    static constexpr int rgSizes[] = { 16, 32, 48, 256 };
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

static UINT ConExtractIconInBothSizesW(PCWSTR szFileName, int nIconIndex, HICON* phiconLarge, HICON* phiconSmall)
{
    HICON ahicon[2] = { nullptr, nullptr };
    auto result = ConExtractIcons(
        szFileName,
        nIconIndex,
        MAKELONG(GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CXSMICON)),
        MAKELONG(GetSystemMetrics(SM_CYICON), GetSystemMetrics(SM_CYSMICON)),
        ahicon,
        2,
        0);

    *phiconLarge = ahicon[0];
    *phiconSmall = ahicon[1];

    return result;
}
// Excerpted Region Ends

Icon::Icon() :
    _fInitialized(false),
    _hDefaultIcon(nullptr),
    _hDefaultSmIcon(nullptr),
    _hIcon(nullptr),
    _hSmIcon(nullptr)
{
}

Icon::~Icon()
{
    // Do not destroy icon handles. They're shared icons as they were loaded from LoadIcon/LoadImage.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms648063(v=vs.85).aspx

    // Do destroy icons from ExtractIconEx. They're not shared.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms648069(v=vs.85).aspx
    _DestroyNonDefaultIcons();
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
[[nodiscard]] HRESULT Icon::GetIcons(_Out_opt_ HICON* const phIcon, _Out_opt_ HICON* const phSmIcon)
{
    auto hr = S_OK;

    if (nullptr != phIcon)
    {
        hr = _GetAvailableIconFromReference(_hIcon, _hDefaultIcon, phIcon);
    }

    if (SUCCEEDED(hr))
    {
        if (nullptr != phSmIcon)
        {
            hr = _GetAvailableIconFromReference(_hSmIcon, _hDefaultSmIcon, phSmIcon);
        }
    }

    return hr;
}

// Routine Description:
// - Sets custom icons onto the class or resets the icons to defaults. Use a null handle to reset an icon to its default value.
// Arguments:
// - hIcon - The large icon handle or null to reset to default
// - hSmIcon - The small icon handle or null to reset to default
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Icon::SetIcons(const HICON hIcon, const HICON hSmIcon)
{
    auto hr = _SetIconFromReference(_hIcon, hIcon);

    if (SUCCEEDED(hr))
    {
        hr = _SetIconFromReference(_hSmIcon, hSmIcon);
    }

    if (SUCCEEDED(hr))
    {
        HICON hNewIcon;
        HICON hNewSmIcon;

        hr = GetIcons(&hNewIcon, &hNewSmIcon);

        if (SUCCEEDED(hr))
        {
            // Special case. If we had a non-default big icon and a default small icon, set the small icon to null when updating the window.
            // This will cause the large one to be stretched and used as the small one.
            if (hNewIcon != _hDefaultIcon && hNewSmIcon == _hDefaultSmIcon)
            {
                hNewSmIcon = nullptr;
            }

            PostMessageW(ServiceLocator::LocateConsoleWindow()->GetWindowHandle(), WM_SETICON, ICON_BIG, (LPARAM)hNewIcon);
            PostMessageW(ServiceLocator::LocateConsoleWindow()->GetWindowHandle(), WM_SETICON, ICON_SMALL, (LPARAM)hNewSmIcon);
        }
    }

    return hr;
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
    auto hr = S_OK;

    // Return value is count of icons extracted, which is redundant with filling the pointers.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms648069(v=vs.85).aspx
    ConExtractIconInBothSizesW(pwszIconLocation, nIconIndex, &_hIcon, &_hSmIcon);

    // If the large icon failed, then ensure that we use the defaults.
    if (_hIcon == nullptr)
    {
        _DestroyNonDefaultIcons(); // ensure handles are destroyed/null
        hr = E_FAIL;
    }

    return hr;
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
[[nodiscard]] HRESULT Icon::ApplyWindowMessageWorkaround(const HWND hwnd)
{
    HICON hIcon;
    HICON hSmIcon;

    auto hr = GetIcons(&hIcon, &hSmIcon);

    if (SUCCEEDED(hr))
    {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hSmIcon);
    }

    return hr;
}

// Routine Description:
// - Initializes the icon class by loading the default icons.
// Arguments:
// - <none>
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Icon::_Initialize()
{
    auto hr = S_OK;

    if (!_fInitialized)
    {
#pragma warning(push)
#pragma warning(disable : 4302) // typecast warning from MAKEINTRESOURCE
        _hDefaultIcon = LoadIconW(nullptr, MAKEINTRESOURCE(IDI_APPLICATION));
#pragma warning(pop)

        if (_hDefaultIcon == nullptr)
        {
            hr = HRESULT_FROM_WIN32(GetLastError());
        }

        if (SUCCEEDED(hr))
        {
#pragma warning(push)
#pragma warning(disable : 4302) // typecast warning from MAKEINTRESOURCE
            _hDefaultSmIcon = (HICON)LoadImageW(nullptr,
                                                MAKEINTRESOURCE(IDI_APPLICATION),
                                                IMAGE_ICON,
                                                GetSystemMetrics(SM_CXSMICON),
                                                GetSystemMetrics(SM_CYSMICON),
                                                LR_SHARED);
#pragma warning(pop)

            if (_hDefaultSmIcon == nullptr)
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
            }
        }

        if (SUCCEEDED(hr))
        {
            _fInitialized = true;
        }
    }

    return hr;
}

// Routine Description:
// - Frees any non-default icon handles we may have loaded from a path on the file system
// Arguments:
// - <none>
// Return Value:
// - <none>
void Icon::_DestroyNonDefaultIcons()
{
    _FreeIconFromReference(_hIcon);
    _FreeIconFromReference(_hSmIcon);
}

// Routine Description:
// - Helper method to choose one of the two given references to fill the pointer with.
// - It will choose the specific icon if it is available and otherwise fall back to the default icon.
// Arguments:
// - hIconRef - reference to the specific icon handle inside this class
// - hDefaultIconRef - reference to the default icon handle inside this class
// - phIcon - pointer to receive the chosen icon handle
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Icon::_GetAvailableIconFromReference(_In_ HICON& hIconRef, _In_ HICON& hDefaultIconRef, _Out_ HICON* const phIcon)
{
    auto hr = S_OK;

    // expecting hIconRef to be pointing to either the regular or small custom handles
    FAIL_FAST_IF(!(&hIconRef == &_hIcon || &hIconRef == &_hSmIcon));

    // expecting hDefaultIconRef to be pointing to either the regular or small default handles
    FAIL_FAST_IF(!(&hDefaultIconRef == &_hDefaultIcon || &hDefaultIconRef == &_hDefaultSmIcon));

    if (hIconRef != nullptr)
    {
        *phIcon = hIconRef;
    }
    else
    {
        hr = _GetDefaultIconFromReference(hDefaultIconRef, phIcon);
    }

    return hr;
}

// Routine Description:
// - Helper method to initialize and retrieve a default icon.
// Arguments:
// - hIconRef - Either the small or large icon handle references within this class
// - phIcon - The pointer to fill with the icon if it is available.
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Icon::_GetDefaultIconFromReference(_In_ HICON& hIconRef, _Out_ HICON* const phIcon)
{
    // expecting hIconRef to be pointing to either the regular or small default handles
    FAIL_FAST_IF(!(&hIconRef == &_hDefaultIcon || &hIconRef == &_hDefaultSmIcon));

    auto hr = _Initialize();

    if (SUCCEEDED(hr))
    {
        *phIcon = hIconRef;
    }

    return hr;
}

// Routine Description:
// - Helper method to set an icon handle into the given reference to an icon within this class.
// - This will appropriately call to free existing custom icons.
// Arguments:
// - hIconRef - Either the small or large icon handle references within this class
// - hNewIcon - The new icon handle to replace the reference with.
// Return Value:
// - S_OK or HRESULT failure code.
[[nodiscard]] HRESULT Icon::_SetIconFromReference(_In_ HICON& hIconRef, const HICON hNewIcon)
{
    // expecting hIconRef to be pointing to either the regular or small custom handles
    FAIL_FAST_IF(!(&hIconRef == &_hIcon || &hIconRef == &_hSmIcon));

    auto hr = S_OK;

    // Only set icon if something changed
    if (hNewIcon != hIconRef)
    {
        // If we had an existing custom icon, free it.
        _FreeIconFromReference(hIconRef);

        // If we were given a non-null icon, store it.
        if (hNewIcon != nullptr)
        {
            hIconRef = hNewIcon;
        }

        // Otherwise, we'll default back to using the default icon. Get method will handle this.
    }

    return hr;
}

// Routine Description:
// - Helper method to free a specific icon reference to a specific icon within this class.
// - WARNING: Do not use with the default icons. They do not need to be released.
// Arguments:
// - hIconRef - Either the small or large specific icon handle references within this class
// Return Value:
// - None
void Icon::_FreeIconFromReference(_In_ HICON& hIconRef)
{
    // expecting hIconRef to be pointing to either the regular or small custom handles
    FAIL_FAST_IF(!(&hIconRef == &_hIcon || &hIconRef == &_hSmIcon));

    if (hIconRef != nullptr)
    {
        DestroyIcon(hIconRef);
        hIconRef = nullptr;
    }
}
