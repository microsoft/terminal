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
    winrt::fire_and_forget MyPage::OnLoadIconClick(Windows::Foundation::IInspectable const& sender, Windows::UI::Xaml::RoutedEventArgs const& e)
    {
        const auto text = PathInput().Text();

        co_await winrt::resume_background();

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

        co_return;
    }
}
