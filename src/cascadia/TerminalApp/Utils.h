/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Utils.h

Abstract:
- Helpers for the TerminalApp project
Author(s):
- Mike Griese - May 2019

--*/
#pragma once

namespace winrt::Microsoft::UI::Xaml::Controls
{
    struct IconSource;
    struct BitmapIconSource;
}

namespace winrt::Windows::UI::Xaml::Controls
{
    struct IconSource;
    struct BitmapIconSource;
}

namespace Microsoft::TerminalApp::details
{
    // This is a template that helps us figure out which BitmapIconSource to use for a given IconSource.
    // We have to do this because some of our code still wants to use WUX IconSources.
    template<typename TIconSource>
    struct BitmapIconSource
    {
    };

    template<>
    struct BitmapIconSource<winrt::Microsoft::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Microsoft::UI::Xaml::Controls::BitmapIconSource;
    };

    template<>
    struct BitmapIconSource<winrt::Windows::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Windows::UI::Xaml::Controls::BitmapIconSource;
    };

    template<typename TIconSource>
    struct FontIconSource
    {
    };

    template<>
    struct FontIconSource<winrt::Microsoft::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Microsoft::UI::Xaml::Controls::FontIconSource;
    };

    template<>
    struct FontIconSource<winrt::Windows::UI::Xaml::Controls::IconSource>
    {
        using type = winrt::Windows::UI::Xaml::Controls::FontIconSource;
    };
}

// Method Description:
// - Creates an IconElement for the given path. The icon returned is a colored
//   icon. If we couldn't create the icon for any reason, we return an empty
//   IconElement.
// Template Types:
// - <TIconSource>: The type of IconSource (MUX, WUX) to generate.
// Arguments:
// - path: the full, expanded path to the icon.
// Return Value:
// - An IconElement with its IconSource set, if possible.
template<typename TIconSource>
TIconSource GetColoredIcon(const winrt::hstring& path)
{
    if (!path.empty())
    {
        try
        {
            winrt::Windows::Foundation::Uri iconUri{ path };
            ::Microsoft::TerminalApp::details::BitmapIconSource<TIconSource>::type iconSource;
            // Make sure to set this to false, so we keep the RGB data of the
            // image. Otherwise, the icon will be white for all the
            // non-transparent pixels in the image.
            iconSource.ShowAsMonochrome(false);
            iconSource.UriSource(iconUri);
            return iconSource;
        }
        CATCH_LOG();
    }

    return nullptr;
}

// TODO: GH#1564 SUI polish - Dedupe with Command's icon handler
template<typename TIconSource>
TIconSource GetFontIcon(const winrt::Windows::UI::Xaml::Media::FontFamily& fontFamily,
                        const double fontSize,
                        const winrt::hstring glyph)
{
    if (!glyph.empty())
    {
        try
        {
            ::Microsoft::TerminalApp::details::FontIconSource<TIconSource>::type iconSource;
            iconSource.FontFamily(fontFamily);
            iconSource.FontSize(fontSize);
            iconSource.Glyph(glyph);
            return iconSource;
        }
        CATCH_LOG();
    }

    return nullptr;
}
