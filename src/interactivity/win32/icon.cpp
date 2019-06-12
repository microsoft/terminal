// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "precomp.h"

#include "icon.hpp"

#include "window.hpp"

#include "..\inc\ServiceLocator.hpp"

using namespace Microsoft::Console::Interactivity::Win32;

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
    HRESULT hr = S_OK;

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
    HRESULT hr = _SetIconFromReference(_hIcon, hIcon);

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
    HRESULT hr = S_OK;

    // Return value is count of icons extracted, which is redundant with filling the pointers.
    // http://msdn.microsoft.com/en-us/library/windows/desktop/ms648069(v=vs.85).aspx
    ExtractIconExW(pwszIconLocation, nIconIndex, &_hIcon, &_hSmIcon, 1);

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

    HRESULT hr = GetIcons(&hIcon, &hSmIcon);

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
    HRESULT hr = S_OK;

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
    HRESULT hr = S_OK;

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

    HRESULT hr = _Initialize();

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

    HRESULT hr = S_OK;

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
