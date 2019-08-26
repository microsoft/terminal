// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Utils.h"
#include "../../types/inc/Utils.hpp"


// Method Description:
// - Contstructs a wstring from a given Json::Value object. Reads the object as
//   a std::string using asString, then builds an hstring from that std::string,
//   then converts that hstring into a std::wstring.
// Arguments:
// - json: the Json::Value to parse as a string
// Return Value:
// - the wstring equivalent of the value in json
std::wstring GetWstringFromJson(const Json::Value& json)
{
    return winrt::to_hstring(json.asString()).c_str();
}

// Method Description:
// - Creates an IconElement for the given path. The icon returned is a colored
//   icon. If we couldn't create the icon for any reason, we return an empty
//   IconElement.
// Arguments:
// - path: the full, expanded path to the icon.
// Return Value:
// - An IconElement with its IconSource set, if possible.
winrt::Windows::UI::Xaml::Controls::IconElement GetColoredIcon(const winrt::hstring& path)
{
    winrt::Windows::UI::Xaml::Controls::IconSourceElement elem{};
    if (!path.empty())
    {
        try
        {
            winrt::Windows::Foundation::Uri iconUri{ path };
            winrt::Windows::UI::Xaml::Controls::BitmapIconSource iconSource;
            // Make sure to set this to false, so we keep the RGB data of the
            // image. Otherwise, the icon will be white for all the
            // non-transparent pixels in the image.
            iconSource.ShowAsMonochrome(false);
            iconSource.UriSource(iconUri);
            elem.IconSource(iconSource);
        }
        CATCH_LOG();
    }

    return elem;
}
