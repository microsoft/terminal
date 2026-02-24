// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "Utils.h"

// Function Description:
// - Helper that opens a file picker pre-seeded with image file types.
winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenImagePicker(HWND parentHwnd)
{
    static constexpr COMDLG_FILTERSPEC supportedImageFileTypes[] = {
        { L"All Supported Bitmap Types (*.jpg, *.jpeg, *.png, *.bmp, *.gif, *.tiff, *.ico)", L"*.jpg;*.jpeg;*.png;*.bmp;*.gif;*.tiff;*.ico" },
        { L"All Files (*.*)", L"*.*" }
    };

    static constexpr winrt::guid clientGuidImagePicker{ 0x55675F54, 0x74A1, 0x4552, { 0xA3, 0x9D, 0x94, 0xAE, 0x85, 0xD8, 0xF2, 0x7A } };
    return OpenFilePicker(parentHwnd, [](auto&& dialog) {
        THROW_IF_FAILED(dialog->SetClientGuid(clientGuidImagePicker));
        try
        {
            auto pictureFolderShellItem{ winrt::capture<IShellItem>(&SHGetKnownFolderItem, FOLDERID_PicturesLibrary, KF_FLAG_DEFAULT, nullptr) };
            dialog->SetDefaultFolder(pictureFolderShellItem.get());
        }
        CATCH_LOG(); // non-fatal

#pragma warning(suppress : 26485) // so we can pass in the supportedImageFileTypes without the analyzer complaining
        THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedImageFileTypes), supportedImageFileTypes));
        THROW_IF_FAILED(dialog->SetFileTypeIndex(1)); // the array is 1-indexed
        THROW_IF_FAILED(dialog->SetDefaultExtension(L"jpg;jpeg;png;bmp;gif;tiff;ico"));
    });
}
