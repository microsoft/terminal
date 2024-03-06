// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include <ShObjIdl_core.h>

// Function Description:
// - This function presents a File Open "common dialog" and returns its selected file asynchronously.
// Parameters:
// - customize: A lambda that receives an IFileDialog* to customize.
// Return value:
// (async) path to the selected item.
template<typename TLambda>
winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> FilePicker(HWND parentHwnd, bool saveDialog, TLambda&& customize)
{
    auto fileDialog{ saveDialog ? winrt::create_instance<IFileDialog>(CLSID_FileSaveDialog) :
                                  winrt::create_instance<IFileDialog>(CLSID_FileOpenDialog) };
    DWORD flags{};
    THROW_IF_FAILED(fileDialog->GetOptions(&flags));
    THROW_IF_FAILED(fileDialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT)); // filesystem objects only; no recent places

    // BODGY: The MSVC 14.31.31103 toolset seems to misdiagnose this line.
#pragma warning(suppress : 26481) // Don't use pointer arithmetic. Use span instead (bounds.1).
    customize(fileDialog.get());

    const auto hr{ fileDialog->Show(parentHwnd) };
    if (!SUCCEEDED(hr))
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            co_return winrt::hstring{};
        }
        THROW_HR(hr);
    }

    winrt::com_ptr<IShellItem> result;
    THROW_IF_FAILED(fileDialog->GetResult(result.put()));

    wil::unique_cotaskmem_string filePath;
    THROW_IF_FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &filePath));

    co_return winrt::hstring{ filePath.get() };
}

template<typename TLambda>
winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenFilePicker(HWND parentHwnd, TLambda&& customize)
{
    return FilePicker(parentHwnd, false, customize);
}
template<typename TLambda>
winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> SaveFilePicker(HWND parentHwnd, TLambda&& customize)
{
    return FilePicker(parentHwnd, true, customize);
}

winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenImagePicker(HWND parentHwnd);

#ifdef WINRT_Windows_UI_Xaml_H
// Only compile me if Windows.UI.Xaml is already included.
//
// XAML Hacks:
//
// the App is always in the OS theme, so the
// App::Current().Resources() lookup will always get the value for the
// OS theme, not the requested theme.
//
// This helper allows us to instead lookup the value of a resource
// specified by `key` for the given `requestedTheme`, from the
// dictionaries in App.xaml. Make sure the value is actually there!
// Otherwise this'll throw like any other Lookup for a resource that
// isn't there.
winrt::Windows::Foundation::IInspectable ThemeLookup(const auto& res,
                                                     const winrt::Windows::UI::Xaml::ElementTheme& requestedTheme,
                                                     const winrt::Windows::Foundation::IInspectable& key)
{
    // You want the Default version of the resource? Great, the App is
    // always in the OS theme. Just look it up and be done.
    if (requestedTheme == winrt::Windows::UI::Xaml::ElementTheme::Default)
    {
        return res.Lookup(key);
    }
    static const auto lightKey = winrt::box_value(L"Light");
    static const auto darkKey = winrt::box_value(L"Dark");
    // There isn't an ElementTheme::HighContrast.

    const auto requestedThemeKey = requestedTheme == winrt::Windows::UI::Xaml::ElementTheme::Dark ? darkKey : lightKey;
    for (const auto& dictionary : res.MergedDictionaries())
    {
        // Don't look in the MUX resources. They come first. A person
        // with more patience than me may find a way to look through our
        // dictionaries first, then the MUX ones, but that's not needed
        // currently
        if (dictionary.Source())
        {
            continue;
        }
        // Look through the theme dictionaries we defined:
        for (const auto& [dictionaryKey, dict] : dictionary.ThemeDictionaries())
        {
            // Does the key for this dict match the theme we're looking for?
            if (winrt::unbox_value<winrt::hstring>(dictionaryKey) !=
                winrt::unbox_value<winrt::hstring>(requestedThemeKey))
            {
                // No? skip it.
                continue;
            }
            // Look for the requested resource in this dict.
            const auto themeDictionary = dict.as<winrt::Windows::UI::Xaml::ResourceDictionary>();
            if (themeDictionary.HasKey(key))
            {
                return themeDictionary.Lookup(key);
            }
        }
    }

    // We didn't find it in the requested dict, fall back to the default dictionary.
    return res.Lookup(key);
};
#endif
