// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "EnumEntry.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct EnumEntry : EnumEntryT<EnumEntry>
    {
    public:
        EnumEntry(const winrt::hstring enumName, const winrt::Windows::Foundation::IInspectable& enumValue) :
            _EnumName{ enumName },
            _EnumValue{ enumValue } {}

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, EnumName, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(winrt::Windows::Foundation::IInspectable, EnumValue, _PropertyChangedHandlers);
    };
}
