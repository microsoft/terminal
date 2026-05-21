// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "CornerRadiusFilterConverters.h"
#include "CornerRadiusConverter.g.cpp"
#include "TopCornerRadiusFilterConverter.g.cpp"
#include "BottomCornerRadiusFilterConverter.g.cpp"

using namespace winrt::Windows::UI::Xaml;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    winrt::Windows::Foundation::IInspectable CornerRadiusConverter::Convert(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        if (!value)
        {
            return value;
        }
        const auto cr = unbox_value_or<CornerRadius>(value, CornerRadius{ 0, 0, 0, 0 });
        return box_value(CornerRadius{ 0, 0, cr.BottomRight, cr.BottomLeft });
    }

    winrt::Windows::Foundation::IInspectable CornerRadiusConverter::ConvertBack(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        return value;
    }

    winrt::Windows::Foundation::IInspectable TopCornerRadiusFilterConverter::Convert(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        if (!value)
        {
            return value;
        }
        const auto cr = unbox_value_or<CornerRadius>(value, CornerRadius{ 0, 0, 0, 0 });
        return box_value(CornerRadius{ cr.TopLeft, cr.TopRight, 0, 0 });
    }

    winrt::Windows::Foundation::IInspectable TopCornerRadiusFilterConverter::ConvertBack(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        return value;
    }

    winrt::Windows::Foundation::IInspectable BottomCornerRadiusFilterConverter::Convert(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        if (!value)
        {
            return value;
        }
        const auto cr = unbox_value_or<CornerRadius>(value, CornerRadius{ 0, 0, 0, 0 });
        return box_value(CornerRadius{ 0, 0, cr.BottomRight, cr.BottomLeft });
    }

    winrt::Windows::Foundation::IInspectable BottomCornerRadiusFilterConverter::ConvertBack(const winrt::Windows::Foundation::IInspectable& value, const Interop::TypeName& /*targetType*/, const winrt::Windows::Foundation::IInspectable& /*parameter*/, const hstring& /*language*/)
    {
        return value;
    }
}
