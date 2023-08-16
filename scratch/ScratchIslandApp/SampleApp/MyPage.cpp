// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include "MySettings.h"

#include <Shlwapi.h> // For PathCombine function
#include <wincodec.h> // Windows Imaging Component

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

    HBITMAP ConvertSoftwareBitmapToHBITMAP(winrt::Windows::Graphics::Imaging::SoftwareBitmap softwareBitmap)
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

        return hBitmap;
    }
    HICON _convertBitmapToHICON(HBITMAP hBitmap)
    {
        ICONINFO iconInfo = {};
        iconInfo.fIcon = TRUE;
        // iconInfo.hbmMask = nullptr; // No mask is required for icons
        // iconInfo.hbmMask = CreateBitmap(width, height, 1, 1, 0); // ^ that was a fuckin lie
        iconInfo.hbmMask = CreateBitmap(64, 64, 1, 1, 0); // ^ that was a fuckin lie
        iconInfo.hbmColor = hBitmap;

        HICON hIcon = CreateIconIndirect(&iconInfo);

        // get last error if it failed
        if (!hIcon)
        {
            auto gle = GetLastError();
            auto hr = HRESULT_FROM_WIN32(gle);
            LOG_IF_FAILED(hr);
        }

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

    winrt::fire_and_forget MyPage::_attemptTwo(const winrt::hstring& path)
    {
        // Open the file, and load it into a SoftwareBitmap
        auto file = co_await winrt::Windows::Storage::StorageFile::GetFileFromPathAsync(path);
        auto stream = co_await file.OpenAsync(winrt::Windows::Storage::FileAccessMode::Read);
        auto decoder = co_await winrt::Windows::Graphics::Imaging::BitmapDecoder::CreateAsync(stream);
        auto softwareBitmap = co_await decoder.GetSoftwareBitmapAsync();

        // Convert the SoftwareBitmap to an HBITMAP, using Windows Imaging Component
        auto hBitmap = ConvertSoftwareBitmapToHBITMAP(softwareBitmap);
        auto hIcon = _convertBitmapToHICON(hBitmap);
        _setTaskbarBadge(hIcon);
    }

    winrt::fire_and_forget MyPage::OnLoadIconClick(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        const auto text = PathInput().Text();

        co_await winrt::resume_background();

        _attemptTwo(text);

        co_return;
    }
}
