/*++
Copyright (c) Microsoft Corporation
Licensed under the MIT license.

Module Name:
- Command.h

Abstract:
- A command represents a single entry in the Command Palette. This is an object
  that has a user facing "name" to display to the user, and an associated action
  which can be dispatched.

- For more information, see GH#2046, #5400, #5674, and #6635

Author(s):
- Mike Griese - June 2020

--*/
#pragma once

#include "Trigger.g.h"
#include "TerminalWarnings.h"
#include "ActionAndArgs.h"
#include "SettingsTypes.h"
#include "JsonUtils.h"

// fwdecl unittest classes
namespace SettingsModelLocalTests
{
    class DeserializationTests;
    class TriggerTests;
};

namespace winrt::Microsoft::Terminal::Settings::Model::implementation
{
    struct Trigger : TriggerT<Trigger>
    {
        Trigger() = default;
        com_ptr<Trigger> Copy() const;

        static winrt::com_ptr<Trigger> FromJson(const Json::Value& json);

        Model::ActionAndArgs EvaluateMatch(const Windows::Foundation::Collections::IVectorView<winrt::hstring>& matches /*,
                                           Windows::Foundation::Collections::IVector<SettingsLoadWarnings> warnings*/
        );

        static std::vector<SettingsLoadWarnings> LayerJson(Windows::Foundation::Collections::IVector<Model::Trigger>& triggers,
                                                           const Json::Value& json);
        Json::Value ToJson() const;

        winrt::Windows::UI::Xaml::Data::INotifyPropertyChanged::PropertyChanged_revoker propertyChangedRevoker;

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_PROPERTY(TriggerType, Type, TriggerType::MatchRegex);
        WINRT_PROPERTY(winrt::hstring, Match, L"");
        // WINRT_PROPERTY(Model::ActionAndArgs, ActionAndArgs);

    private:
        Json::Value _originalActionJson;

        friend class SettingsModelLocalTests::DeserializationTests;
        friend class SettingsModelLocalTests::TriggerTests;
    };
}

namespace winrt::Microsoft::Terminal::Settings::Model::factory_implementation
{
    BASIC_FACTORY(Trigger);
}

namespace Microsoft::Terminal::Settings::Model::JsonUtils
{
    using namespace winrt::Microsoft::Terminal::Settings::Model;

    // If you get weird linker errors about this not being defined, make sure
    // you #include this file in the file that's actually doing the
    // de/serializing (e.g. Profile.cpp)
    template<>
    struct ConversionTrait<Trigger>
    {
        Trigger FromJson(const Json::Value& json)
        {
            return *implementation::Trigger::FromJson(json);
        }

        bool CanConvert(const Json::Value& json) const
        {
            return json.isObject();
        }

        Json::Value ToJson(const Trigger& val)
        {
            return winrt::get_self<implementation::Trigger>(val)->ToJson();
        }

        std::string TypeDescription() const
        {
            return "Trigger";
        }
    };
}
