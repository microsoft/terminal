// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "../inc/cppwinrt_utils.h"

// This macro must be used alongside GETSET_BINDABLE_ENUM_SETTING.
// Use this in your class's constructor after Initialize_Component().
// It sorts and initializes the observable list of enum entries with the enum name
// being its localized name, and also initializes the enum to EnumEntry
// map that's required to tell XAML what enum value the currently active
// setting has.
#define INITIALIZE_BINDABLE_ENUM_SETTING(name, enumMappingsName, enumType, resourceSectionAndType, resourceProperty)                                    \
    std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> name##List;                                                                    \
    _##name##Map = winrt::single_threaded_map<enumType, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>();                                     \
    auto enumMapping##name = winrt::Microsoft::Terminal::Settings::Model::EnumMappings::##enumMappingsName();                                           \
    for (auto [key, value] : enumMapping##name)                                                                                                         \
    {                                                                                                                                                   \
        auto enumName = LocalizedNameForEnumName(resourceSectionAndType, key, resourceProperty);                                                        \
        auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(enumName, winrt::box_value<enumType>(value)); \
        name##List.emplace_back(entry);                                                                                                                 \
        _##name##Map.Insert(value, entry);                                                                                                              \
    }                                                                                                                                                   \
    std::sort(begin(name##List), end(name##List), EnumEntryComparator<enumType>());                                                                     \
    _##name##List = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(name##List));

#define INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(name, enumMappingsName, enumType, resourceSectionAndType, resourceProperty)                      \
    std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> name##List;                                                                    \
    _##name##Map = winrt::single_threaded_map<enumType, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>();                                     \
    auto enumMapping##name = winrt::Microsoft::Terminal::Settings::Model::EnumMappings::##enumMappingsName();                                           \
    for (auto [key, value] : enumMapping##name)                                                                                                         \
    {                                                                                                                                                   \
        auto enumName = LocalizedNameForEnumName(resourceSectionAndType, key, resourceProperty);                                                        \
        auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(enumName, winrt::box_value<enumType>(value)); \
        name##List.emplace_back(entry);                                                                                                                 \
        _##name##Map.Insert(value, entry);                                                                                                              \
    }                                                                                                                                                   \
    std::sort(begin(name##List), end(name##List), EnumEntryReverseComparator<enumType>());                                                              \
    _##name##List = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(name##List));

// This macro must be used alongside INITIALIZE_BINDABLE_ENUM_SETTING.
// It declares the needed data structures, getters, and setters to make
// the given enum type bindable to XAML. It provides an observable list
// of EnumEntries so that we may display all possible values of the given
// enum type and its localized names. It also provides a getter and setter
// for the setting we wish to bind to.
#define GETSET_BINDABLE_ENUM_SETTING(name, enumType, settingsModelName, settingNameInModel)                                                                  \
public:                                                                                                                                                      \
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>##name##List()                        \
    {                                                                                                                                                        \
        return _##name##List;                                                                                                                                \
    }                                                                                                                                                        \
                                                                                                                                                             \
    winrt::Windows::Foundation::IInspectable Current##name()                                                                                                 \
    {                                                                                                                                                        \
        return winrt::box_value<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(_##name##Map.Lookup(##settingsModelName().##settingNameInModel())); \
    }                                                                                                                                                        \
                                                                                                                                                             \
    void Current##name(const winrt::Windows::Foundation::IInspectable& enumEntry)                                                                            \
    {                                                                                                                                                        \
        if (auto ee = enumEntry.try_as<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>())                                                           \
        {                                                                                                                                                    \
            auto setting = winrt::unbox_value<enumType>(ee.EnumValue());                                                                                     \
            ##settingsModelName().##settingNameInModel(setting);                                                                                             \
        }                                                                                                                                                    \
    }                                                                                                                                                        \
                                                                                                                                                             \
private:                                                                                                                                                     \
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _##name##List;                       \
    winrt::Windows::Foundation::Collections::IMap<enumType, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _##name##Map;

// This macro defines a dependency property for a WinRT class.
// Use this in your class' header file after declaring it in the idl.
// Remember to register your dependency property in the respective cpp file.
#define DEPENDENCY_PROPERTY(type, name)                                  \
public:                                                                  \
    static winrt::Windows::UI::Xaml::DependencyProperty name##Property() \
    {                                                                    \
        return _##name##Property;                                        \
    }                                                                    \
    type name() const                                                    \
    {                                                                    \
        return winrt::unbox_value<type>(GetValue(_##name##Property));    \
    }                                                                    \
    void name(type const& value)                                         \
    {                                                                    \
        SetValue(_##name##Property, winrt::box_value(value));            \
    }                                                                    \
                                                                         \
private:                                                                 \
    static winrt::Windows::UI::Xaml::DependencyProperty _##name##Property;

namespace winrt::Microsoft::Terminal::Settings
{
    winrt::hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable);
    winrt::hstring LocalizedNameForEnumName(const std::wstring_view sectionAndType, const std::wstring_view enumValue, const std::wstring_view propertyType);
}
