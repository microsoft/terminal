// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include "MySettings.h"

#include <Shlwapi.h> // For PathCombine function
#include <wincodec.h> // Windows Imaging Component
#include <Shlobj.h>
#include <Shlobj_core.h>
#include <wincodec.h>

using namespace std::chrono_literals;
using namespace winrt::Microsoft::Terminal;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

struct __declspec(uuid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")) __declspec(novtable) IMemoryBufferByteAccess : ::IUnknown
{
    virtual HRESULT __stdcall GetBuffer(uint8_t** value, uint32_t* capacity) = 0;
};

namespace winrt::SampleApp::implementation
{
    MyPage::MyPage()
    {
        InitializeComponent();
    }

    void MyPage::Create(uint64_t hwnd)
    {
        _hwnd = reinterpret_cast<HWND>(hwnd);
        auto settings = winrt::make_self<implementation::MySettings>();

        auto connectionSettings{ TerminalConnection::ConptyConnection::CreateSettings(L"cmd.exe /k echo This TermControl is hosted in-proc...",
                                                                                      winrt::hstring{},
                                                                                      L"",
                                                                                      nullptr,
                                                                                      32,
                                                                                      80,
                                                                                      winrt::guid(),
                                                                                      winrt::guid()) };

        // "Microsoft.Terminal.TerminalConnection.ConptyConnection"
        winrt::hstring myClass{ winrt::name_of<TerminalConnection::ConptyConnection>() };
        TerminalConnection::ConnectionInformation connectInfo{ myClass, connectionSettings };

        TerminalConnection::ITerminalConnection conn{ TerminalConnection::ConnectionInformation::CreateConnection(connectInfo) };
        Control::TermControl control{ *settings, *settings, conn };

        InProcContent().Children().Append(control);

        PathInput().Text(L"d:\\dev\\private\\OpenConsole\\res\\terminal.ico");
    }

    // Method Description:
    // - Gets the title of the currently focused terminal control. If there
    //   isn't a control selected for any reason, returns "Windows Terminal"
    // Arguments:
    // - <none>
    // Return Value:
    // - the title of the focused control if there is one, else "Windows Terminal"
    hstring MyPage::Title()
    {
        return { L"Sample Application" };
    }

    void MyPage::_attemptOne(const winrt::hstring& text)
    {
        IWICImagingFactory* pFactory = nullptr;
        IWICBitmapDecoder* pDecoder = nullptr;
        IWICBitmapFrameDecode* pFrame = nullptr;
        IWICFormatConverter* pConverter = nullptr;

        HICON hIcon = nullptr;

        // Create WIC Imaging Factory
        CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));

        // Load the image from the URI
        HRESULT hr = pFactory->CreateDecoderFromFilename(text.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &pDecoder);
        if (SUCCEEDED(hr))
        {
            hr = pDecoder->GetFrame(0, &pFrame);
            if (SUCCEEDED(hr))
            {
                // Convert the image format to a compatible format for icons (e.g., 32bppBGRA)
                hr = pFactory->CreateFormatConverter(&pConverter);
                if (SUCCEEDED(hr))
                {
                    hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0f, WICBitmapPaletteTypeCustom);
                    if (SUCCEEDED(hr))
                    {
                        // Get the image dimensions
                        UINT width, height;
                        pFrame->GetSize(&width, &height);

                        // Create a DIB section to hold the image data
                        BITMAPINFO bmi = {};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = width;
                        bmi.bmiHeader.biHeight = -static_cast<LONG>(height); // Negative height indicates top-down bitmap
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 32; // 32bpp for ARGB format
                        bmi.bmiHeader.biCompression = BI_RGB;

                        void* pBits = nullptr;
                        HBITMAP hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);

                        if (hBitmap && pBits)
                        {
                            // Copy the converted image data into the DIB section
                            hr = pConverter->CopyPixels(nullptr, width * 4, width * height * 4, static_cast<BYTE*>(pBits));

                            // Create an icon from the DIB section
                            ICONINFO iconInfo = {};
                            iconInfo.fIcon = TRUE;
                            // iconInfo.hbmMask = nullptr; // No mask is required for icons
                            iconInfo.hbmMask = CreateBitmap(width, height, 1, 1, 0); // ^ that was a fuckin lie
                            iconInfo.hbmColor = hBitmap;

                            hIcon = CreateIconIndirect(&iconInfo);

                            // get last error if it failed
                            if (!hIcon)
                            {
                                auto gle = GetLastError();
                                hr = HRESULT_FROM_WIN32(gle);
                            }

                            ITaskbarList3* pTaskbarList = nullptr;
                            HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pTaskbarList));
                            if (SUCCEEDED(hr))
                            {
                                // Set the overlay icon
                                hr = pTaskbarList->SetOverlayIcon(_hwnd, hIcon, L"Overlay Icon Description");

                                DestroyIcon(hIcon); // Release the icon

                                pTaskbarList->Release();
                            }

                            DeleteObject(hBitmap); // The HICON owns the bitmap now
                        }
                    }
                }
            }
        }

        // Release COM interfaces
        if (pConverter)
            pConverter->Release();
        if (pFrame)
            pFrame->Release();
        if (pDecoder)
            pDecoder->Release();
        if (pFactory)
            pFactory->Release();
    }

    HICON ConvertSoftwareBitmapToHICON(winrt::Windows::Graphics::Imaging::SoftwareBitmap softwareBitmap)
    {
        // Get the dimensions of the SoftwareBitmap
        int width = softwareBitmap.PixelWidth();
        int height = softwareBitmap.PixelHeight();

        // Get the pixel data from the SoftwareBitmap
        winrt::Windows::Graphics::Imaging::BitmapBuffer bitmapBuffer = softwareBitmap.LockBuffer(
            winrt::Windows::Graphics::Imaging::BitmapBufferAccessMode::Read);
        winrt::Windows::Foundation::IMemoryBufferReference reference = bitmapBuffer.CreateReference();

        // winrt::Windows::Foundation::IMemoryBufferByteAccess byteAccess;
        // IMemoryBufferByteAccess byteAccess;
        auto byteAccess = reference.as<IMemoryBufferByteAccess>();
        byte* pixelData = nullptr;
        uint32_t capacity = 0;
        byteAccess->GetBuffer(&pixelData, &capacity);

        // Create an HBITMAP using CreateDIBSection
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = width;
        bmi.bmiHeader.biHeight = -height; // Negative height indicates top-down bitmap
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32; // Assuming 32bpp RGBA format
        bmi.bmiHeader.biCompression = BI_RGB;

        void* pBits = nullptr;
        HBITMAP hBitmap = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);

        if (hBitmap && pBits)
        {
            // Copy pixel data to the HBITMAP
            memcpy(pBits, pixelData, width * height * 4); // Assuming 32bpp RGBA format
        }

        ICONINFO iconInfo = {};
        iconInfo.fIcon = TRUE;
        // iconInfo.hbmMask = nullptr; // No mask is required for icons
        iconInfo.hbmMask = CreateBitmap(width, height, 1, 1, 0); // ^ that was a fuckin lie
        // iconInfo.hbmMask = CreateBitmap(64, 64, 1, 1, 0); // ^ that was a fuckin lie
        iconInfo.hbmColor = hBitmap;

        HICON hIcon = CreateIconIndirect(&iconInfo);

        // get last error if it failed
        if (!hIcon)
        {
            auto gle = GetLastError();
            auto hr = HRESULT_FROM_WIN32(gle);
            LOG_IF_FAILED(hr);
        }

        DeleteObject(hBitmap);

        return hIcon;
    }

    void MyPage::_setTaskbarBadge(HICON hIcon)
    {
        ITaskbarList3* pTaskbarList = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pTaskbarList));
        if (SUCCEEDED(hr))
        {
            // Set the overlay icon
            hr = pTaskbarList->SetOverlayIcon(_hwnd, hIcon, L"Overlay Icon Description");

            DestroyIcon(hIcon); // Release the icon

            pTaskbarList->Release();
        }
    }

    void MyPage::_setTaskbarIcon(HICON hIcon)
    {
        SendMessageW(_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
        SendMessageW(_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    }

    // Method Description:
    // - Attempt to get the icon index from the icon path provided
    // Arguments:
    // - iconPath: the full icon path, including the index if present
    // - iconPathWithoutIndex: the place to store the icon path, sans the index if present
    // Return Value:
    // - nullopt if the iconPath is not an exe/dll/lnk file in the first place
    // - 0 if the iconPath is an exe/dll/lnk file but does not contain an index (i.e. we default
    //   to the first icon in the file)
    // - the icon index if the iconPath is an exe/dll/lnk file and contains an index
    std::optional<int> _getIconIndex(const winrt::hstring& iconPath, std::wstring_view& iconPathWithoutIndex)
    {
        const auto pathView = std::wstring_view{ iconPath };
        // Does iconPath have a comma in it? If so, split the string on the
        // comma and look for the index and extension.
        const auto commaIndex = pathView.find(L',');

        // split the path on the comma
        iconPathWithoutIndex = pathView.substr(0, commaIndex);

        // It's an exe, dll, or lnk, so we need to extract the icon from the file.
        if (!til::ends_with(iconPathWithoutIndex, L".exe") &&
            !til::ends_with(iconPathWithoutIndex, L".dll") &&
            !til::ends_with(iconPathWithoutIndex, L".lnk"))
        {
            return std::nullopt;
        }

        if (commaIndex != std::wstring::npos)
        {
            // Convert the string iconIndex to a signed int to support negative numbers which represent an Icon's ID.
            const auto index{ til::to_int(pathView.substr(commaIndex + 1)) };
            if (index == til::to_int_error)
            {
                return std::nullopt;
            }
            return static_cast<int>(index);
        }

        // We had a binary path, but no index. Default to 0.
        return 0;
    }

    winrt::fire_and_forget MyPage::_attemptTwo(winrt::hstring path)
    {
        // First things first:
        // Is the path a path to an exe, a dll, or a resource in one of those files?
        // If so, then we can use the icon from that file, without so much rigamarole.

        std::wstring_view iconPathWithoutIndex;
        const auto indexOpt = _getIconIndex(path, iconPathWithoutIndex);
        if (indexOpt.has_value())
        {
            // Here, we know we have a path to an exe, dll, or resource in one of those files.
            auto iconSize = 32;
            HICON hIcon = nullptr;
            winrt::hstring iconPath{ iconPathWithoutIndex };

            LOG_IF_FAILED(SHDefExtractIcon(iconPath.c_str(),
                                           indexOpt.value(),
                                           0,
                                           &hIcon,
                                           nullptr,
                                           iconSize));
            if (hIcon)
            {
                _setTaskbarBadge(hIcon);
            }
            co_return;
        }

        // If not, then we'll have to do the rigamarole.
        try
        {
            // Create a URI from the path
            const Windows::Foundation::Uri uri{ path };

            winrt::Windows::Storage::IStorageFile file{ nullptr };

            // Is the URI a ms-appx URI? then load it from the app package
            if (uri.SchemeName() == L"ms-appx")
            {
                file = co_await winrt::Windows::Storage::StorageFile::GetFileFromApplicationUriAsync(uri);
            }
            // // Is it a web URI? then download it else if (uri.SchemeName() ==
            // L"http" || uri.SchemeName() == L"https")
            // {
            //     auto downloader = winrt::Windows::Networking::BackgroundTransfer::BackgroundDownloader();
            //     auto download = downloader.CreateDownload(uri, nullptr);
            //     auto downloadOperation = download.StartAsync();
            //     co_await downloadOperation;
            //     file = download.ResultFile();
            // }
            //
            // Actually, don't do anything for web URIs. BackgroundDownloader is not
            // supported outside of packaged apps, but I think that also extends to
            // centennial apps. Useless.
            //

            else
            {
                // Open the file, and load it into a SoftwareBitmap
                file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path);
            }

            // Get the software bitmap out of the file
            auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read);
            auto decoder = co_await winrt::Windows::Graphics::Imaging::BitmapDecoder::CreateAsync(stream);
            auto softwareBitmap = co_await decoder.GetSoftwareBitmapAsync();

            // Convert the SoftwareBitmap to an HBITMAP, using Windows Imaging Component
            // auto hBitmap = ConvertSoftwareBitmapToHBITMAP(softwareBitmap);
            // auto hIcon = _convertBitmapToHICON(hBitmap);
            auto hIcon = ConvertSoftwareBitmapToHICON(softwareBitmap);

            _setTaskbarIcon(hIcon);
            _setTaskbarBadge(hIcon);
        }
        CATCH_LOG();
    }

    winrt::fire_and_forget MyPage::OnLoadIconClick(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        const auto text = PathInput().Text();

        co_await winrt::resume_background();

        _attemptTwo(text);

        co_return;
    }
}
