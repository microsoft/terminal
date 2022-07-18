// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "MyPage.h"
#include "MySettings.h"
#include <LibraryResources.h>
#include "MyPage.g.cpp"
#include "..\..\..\src\cascadia\UnitTests_Control\MockControlSettings.h"
#include "..\..\..\src\types\inc\utils.hpp"
#include <Shlobj.h>
#include <Shlobj_core.h>
#include <wincodec.h>
#include <Windows.Graphics.Imaging.Interop.h>
using namespace std::chrono_literals;
using namespace winrt::Microsoft::Terminal;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
    using IInspectable = Windows::Foundation::IInspectable;
}

using namespace winrt::Windows::Graphics::Imaging;
using namespace winrt::Windows::Storage::Streams;

namespace winrt::SampleApp::implementation
{
    MyPage::MyPage()
    {
        InitializeComponent();
    }

    void MyPage::Create()
    {
    }

    winrt::Windows::Graphics::Imaging::SoftwareBitmap MyConvertToSoftwareBitmap(HICON hicon,
                                                                                winrt::Windows::Graphics::Imaging::BitmapPixelFormat pixelFormat,
                                                                                winrt::Windows::Graphics::Imaging::BitmapAlphaMode alphaMode,
                                                                                IWICImagingFactory* imagingFactory)
    {
        // Load the icon into an IWICBitmap
        wil::com_ptr<IWICBitmap> iconBitmap;
        THROW_IF_FAILED(imagingFactory->CreateBitmapFromHICON(hicon, iconBitmap.put()));

        // Put the IWICBitmap into a SoftwareBitmap. This may fail if WICBitmap's format is not supported by
        // SoftwareBitmap. CreateBitmapFromHICON always creates RGBA8 so we're ok.
        auto softwareBitmap = winrt::capture<winrt::Windows::Graphics::Imaging::SoftwareBitmap>(
            winrt::create_instance<ISoftwareBitmapNativeFactory>(CLSID_SoftwareBitmapNativeFactory),
            &ISoftwareBitmapNativeFactory::CreateFromWICBitmap,
            iconBitmap.get(),
            false);

        // Convert the pixel format and alpha mode if necessary
        if (softwareBitmap.BitmapPixelFormat() != pixelFormat || softwareBitmap.BitmapAlphaMode() != alphaMode)
        {
            softwareBitmap = winrt::Windows::Graphics::Imaging::SoftwareBitmap::Convert(softwareBitmap, pixelFormat, alphaMode);
        }

        return softwareBitmap;
    }

    winrt::Windows::Graphics::Imaging::SoftwareBitmap MyGetBitmapFromIconFileAsync(const winrt::hstring& iconPath,
                                                                                   int32_t iconIndex,
                                                                                   uint32_t iconSize)
    {
        wil::unique_hicon hicon;
        LOG_IF_FAILED(SHDefExtractIcon(iconPath.c_str(), iconIndex, 0, &hicon, nullptr, iconSize));

        if (!hicon)
        {
            return nullptr;
        }

        wil::com_ptr<IWICImagingFactory> wicImagingFactory;
        THROW_IF_FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&wicImagingFactory)));

        return MyConvertToSoftwareBitmap(hicon.get(), BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied, wicImagingFactory.get());
    }

    winrt::fire_and_forget MyPage::CreateClicked(const IInspectable& sender,
                                                 const WUX::Input::TappedRoutedEventArgs& eventArgs)
    {
        // Try:
        // * c:\Windows\System32\SHELL32.dll, 210
        // * c:\Windows\System32\notepad.exe, 0
        // * C:\Program Files\PowerShell\6-preview\pwsh.exe, 0 (this doesn't exist for me)
        // * C:\Program Files\PowerShell\7\pwsh.exe, 0
        auto text{ GuidInput().Text() };
        auto index{ static_cast<int>(IconIndex().Value()) };

        co_await winrt::resume_background();
        auto swBitmap{ MyGetBitmapFromIconFileAsync(text, index, 32) };
        if (swBitmap == nullptr)
        {
            co_return;
        }
        co_await winrt::resume_foreground(Dispatcher());
        winrt::Windows::UI::Xaml::Media::Imaging::SoftwareBitmapSource bitmapSource{};
        co_await bitmapSource.SetBitmapAsync(swBitmap);
        co_await winrt::resume_foreground(Dispatcher());

        winrt::Microsoft::UI::Xaml::Controls::ImageIconSource imageIconSource{};
        imageIconSource.ImageSource(bitmapSource);
        winrt::Microsoft::UI::Xaml::Controls::ImageIcon icon{};
        icon.Source(bitmapSource);
        icon.Width(32);
        icon.Height(32);
        InProcContent().Children().Append(icon);
    }

    void MyPage::CloseClicked(const IInspectable& /*sender*/,
                              const WUX::Input::TappedRoutedEventArgs& /*eventArgs*/)
    {
    }

    void MyPage::KillClicked(const IInspectable& /*sender*/,
                             const WUX::Input::TappedRoutedEventArgs& /*eventArgs*/)
    {
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

}
