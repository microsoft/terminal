// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Appearances.h"
#include "Appearances.g.cpp"
#include "EnumEntry.h"

#include <LibraryResources.h>

using namespace winrt::Windows::UI::Text;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Data;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::Terminal::Settings::Model;

static const std::array<winrt::guid, 2> InBoxProfileGuids{
    winrt::guid{ 0x61c54bbd, 0xc2c6, 0x5271, { 0x96, 0xe7, 0x00, 0x9a, 0x87, 0xff, 0x44, 0xbf } }, // Windows Powershell
    winrt::guid{ 0x0caa0dad, 0x35be, 0x5f56, { 0xa8, 0xff, 0xaf, 0xce, 0xee, 0xaa, 0x61, 0x01 } } // Command Prompt
};

// Function Description:
// - This function presents a File Open "common dialog" and returns its selected file asynchronously.
// Parameters:
// - customize: A lambda that receives an IFileDialog* to customize.
// Return value:
// (async) path to the selected item.
template<typename TLambda>
static winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenFilePicker(HWND parentHwnd, TLambda&& customize)
{
    auto fileDialog{ winrt::create_instance<IFileDialog>(CLSID_FileOpenDialog) };
    DWORD flags{};
    THROW_IF_FAILED(fileDialog->GetOptions(&flags));
    THROW_IF_FAILED(fileDialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT)); // filesystem objects only; no recent places
    customize(fileDialog.get());

    auto hr{ fileDialog->Show(parentHwnd) };
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

// Function Description:
// - Helper that opens a file picker pre-seeded with image file types.
static winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenImagePicker(HWND parentHwnd)
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

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    AppearanceViewModel::AppearanceViewModel(const Model::AppearanceConfig& appearance) :
        _appearance{ appearance }
    {
    }

    bool AppearanceViewModel::CanDeleteAppearance() const
    {
        return false;
    }

    DependencyProperty Appearances::_AppearanceProperty{ nullptr };

    Appearances::Appearances() :
        _ColorSchemeList{ single_threaded_observable_vector<ColorScheme>() }
    {
        InitializeComponent();

        INITIALIZE_BINDABLE_ENUM_SETTING(CursorShape, CursorStyle, winrt::Microsoft::Terminal::Core::CursorStyle, L"Profile_CursorShape", L"Content");

        if (!_AppearanceProperty)
        {
            _AppearanceProperty =
                DependencyProperty::Register(
                    L"Appearance",
                    xaml_typename<Editor::AppearanceViewModel>(),
                    xaml_typename<Editor::Appearances>(),
                    PropertyMetadata{ nullptr });
        }

        if (Appearance())
        {
            const auto& colorSchemeMap{ Appearance().Schemes() };
            for (const auto& pair : colorSchemeMap)
            {
                _ColorSchemeList.Append(pair.Value());
            }

            _ViewModelChangedRevoker = Appearance().PropertyChanged(winrt::auto_revoke, [=](auto&&, const PropertyChangedEventArgs& args) {
                const auto settingName{ args.PropertyName() };
                if (settingName == L"CursorShape")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentCursorShape" });
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"IsVintageCursor" });
                }
                else if (settingName == L"ColorSchemeName")
                {
                    _PropertyChangedHandlers(*this, PropertyChangedEventArgs{ L"CurrentColorScheme" });
                }
            });
        }
    }

    ColorScheme Appearances::CurrentColorScheme()
    {
        const auto schemeName{ Appearance().ColorSchemeName() };
        if (const auto scheme{ Appearance().Schemes().TryLookup(schemeName) })
        {
            return scheme;
        }
        else
        {
            // This Profile points to a color scheme that was renamed or deleted.
            // Fallback to Campbell.
            return Appearance().Schemes().TryLookup(L"Campbell");
        }
    }

    void Appearances::CurrentColorScheme(const ColorScheme& val)
    {
        Appearance().ColorSchemeName(val.Name());
    }

    bool Appearances::IsVintageCursor() const
    {
        return Appearance().CursorShape() == Core::CursorStyle::Vintage;
    }
}
