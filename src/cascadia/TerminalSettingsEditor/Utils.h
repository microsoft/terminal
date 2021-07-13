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
#define INITIALIZE_BINDABLE_ENUM_SETTING(name, enumMappingsName, enumType, resourceSectionAndType, resourceProperty)                                        \
    do                                                                                                                                                      \
    {                                                                                                                                                       \
        std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> name##List;                                                                    \
        _##name##Map = winrt::single_threaded_map<enumType, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>();                                     \
        auto enumMapping##name = winrt::Microsoft::Terminal::Settings::Model::EnumMappings::enumMappingsName();                                             \
        for (auto [key, value] : enumMapping##name)                                                                                                         \
        {                                                                                                                                                   \
            auto enumName = LocalizedNameForEnumName(resourceSectionAndType, key, resourceProperty);                                                        \
            auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(enumName, winrt::box_value<enumType>(value)); \
            name##List.emplace_back(entry);                                                                                                                 \
            _##name##Map.Insert(value, entry);                                                                                                              \
        }                                                                                                                                                   \
        std::sort(name##List.begin(), name##List.end(), EnumEntryComparator<enumType>());                                                                   \
        _##name##List = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(name##List));           \
    } while (0);

#define INITIALIZE_BINDABLE_ENUM_SETTING_REVERSE_ORDER(name, enumMappingsName, enumType, resourceSectionAndType, resourceProperty)                          \
    do                                                                                                                                                      \
    {                                                                                                                                                       \
        std::vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> name##List;                                                                    \
        _##name##Map = winrt::single_threaded_map<enumType, winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>();                                     \
        auto enumMapping##name = winrt::Microsoft::Terminal::Settings::Model::EnumMappings::enumMappingsName();                                             \
        for (auto [key, value] : enumMapping##name)                                                                                                         \
        {                                                                                                                                                   \
            auto enumName = LocalizedNameForEnumName(resourceSectionAndType, key, resourceProperty);                                                        \
            auto entry = winrt::make<winrt::Microsoft::Terminal::Settings::Editor::implementation::EnumEntry>(enumName, winrt::box_value<enumType>(value)); \
            name##List.emplace_back(entry);                                                                                                                 \
            _##name##Map.Insert(value, entry);                                                                                                              \
        }                                                                                                                                                   \
        std::sort(name##List.begin(), name##List.end(), EnumEntryReverseComparator<enumType>());                                                            \
        _##name##List = winrt::single_threaded_observable_vector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(std::move(name##List));           \
    } while (0);

// This macro must be used alongside INITIALIZE_BINDABLE_ENUM_SETTING.
// It declares the needed data structures, getters, and setters to make
// the given enum type bindable to XAML. It provides an observable list
// of EnumEntries so that we may display all possible values of the given
// enum type and its localized names. It also provides a getter and setter
// for the setting we wish to bind to.
#define GETSET_BINDABLE_ENUM_SETTING(name, enumType, settingsModelName, settingNameInModel)                                                              \
public:                                                                                                                                                  \
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> name##List()                     \
    {                                                                                                                                                    \
        return _##name##List;                                                                                                                            \
    }                                                                                                                                                    \
                                                                                                                                                         \
    winrt::Windows::Foundation::IInspectable Current##name()                                                                                             \
    {                                                                                                                                                    \
        return winrt::box_value<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>(_##name##Map.Lookup(settingsModelName().settingNameInModel())); \
    }                                                                                                                                                    \
                                                                                                                                                         \
    void Current##name(const winrt::Windows::Foundation::IInspectable& enumEntry)                                                                        \
    {                                                                                                                                                    \
        if (auto ee = enumEntry.try_as<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry>())                                                       \
        {                                                                                                                                                \
            auto setting = winrt::unbox_value<enumType>(ee.EnumValue());                                                                                 \
            settingsModelName().settingNameInModel(setting);                                                                                             \
        }                                                                                                                                                \
    }                                                                                                                                                    \
                                                                                                                                                         \
private:                                                                                                                                                 \
    winrt::Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::EnumEntry> _##name##List;                   \
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

// Function Description:
// - This function presents a File Open "common dialog" and returns its selected file asynchronously.
// Parameters:
// - customize: A lambda that receives an IFileDialog* to customize.
// Return value:
// (async) path to the selected item.
template<typename TLambda>
winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenFilePicker(HWND parentHwnd, TLambda&& customize)
{
    auto fileDialog{ winrt::create_instance<IFileDialog>(CLSID_FileOpenDialog) };
    DWORD flags{};
    THROW_IF_FAILED(fileDialog->GetOptions(&flags));
    THROW_IF_FAILED(fileDialog->SetOptions(flags | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR | FOS_DONTADDTORECENT)); // filesystem objects only; no recent places
    customize(fileDialog.get());

    auto hr{ fileDialog->Show(parentHwnd) };
    if (!SUCCEEDED(hr))
    {
        if (hr == HRESULT_FROM_WIN32(ERROR_CANCELLED))
        {
            co_return winrt::hstring{};
        }
        THROW_HR(hr);
    }

    winrt::com_ptr<IShellItem> result;
    THROW_IF_FAILED(fileDialog->GetResult(result.put()));

    wil::unique_cotaskmem_string filePath;
    THROW_IF_FAILED(result->GetDisplayName(SIGDN_FILESYSPATH, &filePath));

    co_return winrt::hstring{ filePath.get() };
}

winrt::Windows::Foundation::IAsyncOperation<winrt::hstring> OpenImagePicker(HWND parentHwnd);

namespace winrt::Microsoft::Terminal::Settings
{
    winrt::hstring GetSelectedItemTag(winrt::Windows::Foundation::IInspectable const& comboBoxAsInspectable);
    winrt::hstring LocalizedNameForEnumName(const std::wstring_view sectionAndType, const std::wstring_view enumValue, const std::wstring_view propertyType);
}

// BODGY!
//
// The following function and struct are a workaround for GH#9320.
//
// DismissAllPopups can be used to dismiss all popups for a particular UI
// element. However, we've got a bunch of pages with scroll viewers that may or
// may not have popups in them. Rather than define the same exact body for all
// their ViewChanging events, the HasScrollViewer struct will just do it for
// you!
inline void DismissAllPopups(winrt::Windows::UI::Xaml::XamlRoot const& xamlRoot)
{
    const auto popups{ winrt::Windows::UI::Xaml::Media::VisualTreeHelper::GetOpenPopupsForXamlRoot(xamlRoot) };
    for (const auto& p : popups)
    {
        p.IsOpen(false);
    }
}

template<typename T>
struct HasScrollViewer
{
    // When the ScrollViewer scrolls, dismiss any popups we might have.
    void ViewChanging(winrt::Windows::Foundation::IInspectable const& sender,
                      const winrt::Windows::UI::Xaml::Controls::ScrollViewerViewChangingEventArgs& /*e*/)
    {
        // Inside this struct, we can't get at the XamlRoot() that our subclass
        // implements. I mean, _we_ can, but when XAML does it's code
        // generation, _XAML_ won't be able to figure it out.
        //
        // Fortunately for us, we don't need to! The sender is a UIElement, so
        // we can just get _their_ XamlRoot().
        if (const auto& uielem{ sender.try_as<winrt::Windows::UI::Xaml::UIElement>() })
        {
            DismissAllPopups(uielem.XamlRoot());
        }
    }
};
