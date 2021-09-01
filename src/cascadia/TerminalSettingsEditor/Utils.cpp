// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "pch.h"
#include "Utils.h"

#include <LibraryResources.h>

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::UI::Xaml;

UTILS_DEFINE_LIBRARY_RESOURCE_SCOPE(L"Microsoft.Terminal.Settings.Editor/Resources");

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
        THROW_IF_FAILED(dialog->SetFileTypes(ARRAYSIZE(supportedImageFileTypes), supportedImageFileTypes));
        THROW_IF_FAILED(dialog->SetFileTypeIndex(1)); // the array is 1-indexed
        THROW_IF_FAILED(dialog->SetDefaultExtension(L"jpg;jpeg;png;bmp;gif;tiff;ico"));
    });
}

namespace winrt::Microsoft::Terminal::Settings
{
    hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable)
    {
        Controls::ComboBox comboBox = comboBoxAsInspectable.as<Controls::ComboBox>();
        Controls::ComboBoxItem selectedOption = comboBox.SelectedItem().as<Controls::ComboBoxItem>();

        return unbox_value<hstring>(selectedOption.Tag());
    }

    hstring LocalizedNameForEnumName(const std::wstring_view sectionAndEnumType, const std::wstring_view enumValue, const std::wstring_view propertyType)
    {
        // Uppercase the first letter to conform to our current Resource keys
        auto fmtKey = fmt::format(L"{}{}{}/{}", sectionAndEnumType, char(std::towupper(enumValue[0])), enumValue.substr(1), propertyType);
        return GetLibraryResourceString(fmtKey);
    }
}
