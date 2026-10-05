// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

// Transitional reuse of the existing JSON mappings. Native public declarations
// do not include this adapter or the generated product types it depends on.
#include "pch.h"
#include <winrt/Microsoft.Terminal.Settings.Model.h>
#include "TerminalSettingsSerializationHelpers.h"
#include "Native/SettingsTypes.h"
#include "Native/MediaResourceList.h"

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    template<typename NativeType, typename ProjectedType>
    struct NativeEnumConversion
    {
        NativeType FromJson(const Json::Value& json)
        {
            return static_cast<NativeType>(ConversionTrait<ProjectedType>{}.FromJson(json));
        }

        bool CanConvert(const Json::Value& json)
        {
            return ConversionTrait<ProjectedType>{}.CanConvert(json);
        }

        Json::Value ToJson(const NativeType value)
        {
            return ConversionTrait<ProjectedType>{}.ToJson(static_cast<ProjectedType>(value));
        }

        std::string TypeDescription() const
        {
            return ConversionTrait<ProjectedType>{}.TypeDescription();
        }
    };

#define NATIVE_ENUM_JSON(nativeType, projectedType) \
    template<> struct ConversionTrait<Native::nativeType> : NativeEnumConversion<Native::nativeType, projectedType> {}

    NATIVE_ENUM_JSON(ScrollbarState, winrt::Microsoft::Terminal::Control::ScrollbarState);
    NATIVE_ENUM_JSON(TextAntialiasingMode, winrt::Microsoft::Terminal::Control::TextAntialiasingMode);
    NATIVE_ENUM_JSON(PathTranslationStyle, winrt::Microsoft::Terminal::Control::PathTranslationStyle);
    NATIVE_ENUM_JSON(CloseOnExitMode, winrt::Microsoft::Terminal::Settings::Model::CloseOnExitMode);
    NATIVE_ENUM_JSON(BellStyle, winrt::Microsoft::Terminal::Settings::Model::BellStyle);
    NATIVE_ENUM_JSON(CursorStyle, winrt::Microsoft::Terminal::Core::CursorStyle);
    NATIVE_ENUM_JSON(AdjustTextMode, winrt::Microsoft::Terminal::Core::AdjustTextMode);
    NATIVE_ENUM_JSON(Stretch, winrt::Windows::UI::Xaml::Media::Stretch);
    NATIVE_ENUM_JSON(ConvergedAlignment, winrt::Microsoft::Terminal::Settings::Model::ConvergedAlignment);
    NATIVE_ENUM_JSON(IntenseStyle, winrt::Microsoft::Terminal::Settings::Model::IntenseStyle);

#undef NATIVE_ENUM_JSON

    template<>
    struct ConversionTrait<Native::Color>
    {
        using ProjectedColor = winrt::Microsoft::Terminal::Core::Color;

        Native::Color FromJson(const Json::Value& json)
        {
            const auto color = ConversionTrait<ProjectedColor>{}.FromJson(json);
            return { color.R, color.G, color.B, color.A };
        }

        bool CanConvert(const Json::Value& json)
        {
            return ConversionTrait<ProjectedColor>{}.CanConvert(json);
        }

        Json::Value ToJson(const Native::Color& value)
        {
            return ConversionTrait<ProjectedColor>{}.ToJson({ value.R, value.G, value.B, value.A });
        }

        std::string TypeDescription() const
        {
            return ConversionTrait<ProjectedColor>{}.TypeDescription();
        }
    };

    template<>
    struct ConversionTrait<winrt::com_ptr<Native::MediaResource>>
    {
        winrt::com_ptr<Native::MediaResource> FromJson(const Json::Value& json)
        {
            if (json.isNull())
            {
                return Native::MediaResource::FromString({});
            }
            return Native::MediaResource::FromString(winrt::hstring{ til::u8u16(Detail::GetStringView(json)) });
        }

        bool CanConvert(const Json::Value& json)
        {
            return json.isString() || json.isNull();
        }

        Json::Value ToJson(const winrt::com_ptr<Native::MediaResource>& value)
        {
            if (!value || value->Path().empty())
            {
                return Json::Value::nullSingleton();
            }
            return til::u16u8(value->Path());
        }

        std::string TypeDescription() const
        {
            return "file path";
        }
    };

    template<>
    struct ConversionTrait<winrt::com_ptr<Native::MediaResourceList>>
    {
        using Values = std::vector<winrt::com_ptr<Native::MediaResource>>;

        winrt::com_ptr<Native::MediaResourceList> FromJson(const Json::Value& json)
        {
            return Native::MakeMediaResourceList(ConversionTrait<Values>{}.FromJson(json));
        }

        bool CanConvert(const Json::Value& json)
        {
            return ConversionTrait<Values>{}.CanConvert(json);
        }

        Json::Value ToJson(const winrt::com_ptr<Native::MediaResourceList>& values)
        {
            Json::Value json{ Json::arrayValue };
            if (values)
            {
                ConversionTrait<winrt::com_ptr<Native::MediaResource>> trait;
                for (auto iterator = values->First(); iterator->HasCurrent(); iterator->MoveNext())
                {
                    json.append(trait.ToJson(iterator->Current()));
                }
            }
            return json;
        }

        std::string TypeDescription() const
        {
            return ConversionTrait<Values>{}.TypeDescription();
        }
    };
}
