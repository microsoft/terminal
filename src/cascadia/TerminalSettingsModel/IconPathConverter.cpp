#include "pch.h"
#include "IconPathConverter.h"
#include "IconPathConverter.g.cpp"

#include "Utils.h"

using namespace winrt::Windows;
using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
// These are templates that help us figure out which BitmapIconSource/FontIconSource to use for a given IconSource.
// We have to do this because some of our code still wants to use WUX/MUX IconSources.
#pragma region BitmapIconSource
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
#pragma endregion

#pragma region FontIconSource
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
#pragma endregion

    // Method Description:
    // - Creates an IconSource for the given path. The icon returned is a colored
    //   icon. If we couldn't create the icon for any reason, we return an empty
    //   IconElement.
    // Template Types:
    // - <TIconSource>: The type of IconSource (MUX, WUX) to generate.
    // Arguments:
    // - path: the full, expanded path to the icon.
    // Return Value:
    // - An IconElement with its IconSource set, if possible.
    template<typename TIconSource>
    TIconSource _getColoredBitmapIcon(const winrt::hstring& path)
    {
        if (!path.empty())
        {
            try
            {
                winrt::Windows::Foundation::Uri iconUri{ path };
                BitmapIconSource<TIconSource>::type iconSource;
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

    // Method Description:
    // - Creates an IconSource for the given path.
    //    * If the icon is a path to an image, we'll use that.
    //    * If it isn't, then we'll try and use the text as a FontIcon. If the
    //      character is in the range of symbols reserved for the Segoe MDL2
    //      Asserts, well treat it as such. Otherwise, we'll default to a Sego
    //      UI icon, so things like emoji will work.
    //    * If we couldn't create the icon for any reason, we return an empty
    //      IconElement.
    // Template Types:
    // - <TIconSource>: The type of IconSource (MUX, WUX) to generate.
    // Arguments:
    // - path: the unprocessed path to the icon.
    // Return Value:
    // - An IconElement with its IconSource set, if possible.
    template<typename TIconSource>
    TIconSource _getIconSource(const winrt::hstring& iconPath)
    {
        TIconSource iconSource{ nullptr };

        if (iconPath.size() != 0)
        {
            const auto expandedIconPath{ _expandIconPath(iconPath) };
            iconSource = _getColoredBitmapIcon<TIconSource>(expandedIconPath);

            // If we fail to set the icon source using the "icon" as a path,
            // let's try it as a symbol/emoji.
            //
            // Anything longer than 2 wchar_t's _isn't_ an emoji or symbol, so
            // don't do this if it's just an invalid path.
            if (!iconSource && iconPath.size() <= 2)
            {
                try
                {
                    FontIconSource<TIconSource>::type icon;
                    const wchar_t ch = iconPath[0];

                    // The range of MDL2 Icons isn't explicitly defined, but
                    // we're using this based off the table on:
                    // https://docs.microsoft.com/en-us/windows/uwp/design/style/segoe-ui-symbol-font
                    const bool isMDL2Icon = ch >= L'\uE700' && ch <= L'\uF8FF';
                    if (isMDL2Icon)
                    {
                        icon.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily{ L"Segoe MDL2 Assets" });
                    }
                    else
                    {
                        // Note: you _do_ need to manually set the font here.
                        icon.FontFamily(winrt::Windows::UI::Xaml::Media::FontFamily{ L"Segoe UI" });
                    }
                    icon.FontSize(12);
                    icon.Glyph(iconPath);
                    iconSource = icon;
                }
                CATCH_LOG();
            }
        }
        if (!iconSource)
        {
            // Set the default IconSource to a BitmapIconSource with a null source
            // (instead of just nullptr) because there's a really weird crash when swapping
            // data bound IconSourceElements in a ListViewTemplate (i.e. CommandPalette).
            // Swapping between nullptr IconSources and non-null IconSources causes a crash
            // to occur, but swapping between IconSources with a null source and non-null IconSources
            // work perfectly fine :shrug:.
            BitmapIconSource<TIconSource>::type icon;
            icon.UriSource(nullptr);
            iconSource = icon;
        }

        return iconSource;
    }

    static winrt::hstring _expandIconPath(hstring iconPath)
    {
        if (iconPath.empty())
        {
            return iconPath;
        }
        winrt::hstring envExpandedPath{ wil::ExpandEnvironmentStringsW<std::wstring>(iconPath.c_str()) };
        return envExpandedPath;
    }

    // Method Description:
    // - Attempt to convert something into another type. For the
    //   IconPathConverter, we support a variety of icons:
    //    * If the icon is a path to an image, we'll use that.
    //    * If it isn't, then we'll try and use the text as a FontIcon. If the
    //      character is in the range of symbols reserved for the Segoe MDL2
    //      Asserts, well treat it as such. Otherwise, we'll default to a Sego
    //      UI icon, so things like emoji will work.
    // - MUST BE CALLED ON THE UI THREAD.
    // Arguments:
    // - value: the input object to attempt to convert into an IconSource.
    // Return Value:
    // - Visible if the object was a string and wasn't the empty string.
    Foundation::IInspectable IconPathConverter::Convert(Foundation::IInspectable const& value,
                                                        Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                        Foundation::IInspectable const& /* parameter */,
                                                        hstring const& /* language */)
    {
        const auto& iconPath = winrt::unbox_value_or<winrt::hstring>(value, L"");
        return _getIconSource<Controls::IconSource>(iconPath);
    }

    // unused for one-way bindings
    Foundation::IInspectable IconPathConverter::ConvertBack(Foundation::IInspectable const& /* value */,
                                                            Windows::UI::Xaml::Interop::TypeName const& /* targetType */,
                                                            Foundation::IInspectable const& /* parameter */,
                                                            hstring const& /* language */)
    {
        throw hresult_not_implemented();
    }

    Windows::UI::Xaml::Controls::IconSource IconPathConverter::IconSourceWUX(hstring path)
    {
        return _getIconSource<Windows::UI::Xaml::Controls::IconSource>(path);
    }

    Microsoft::UI::Xaml::Controls::IconSource IconPathConverter::IconSourceMUX(hstring path)
    {
        return _getIconSource<Microsoft::UI::Xaml::Controls::IconSource>(path);
    }
}
