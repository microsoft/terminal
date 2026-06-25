// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "IconPicker.h"
#include "IconPicker.g.cpp"

#include <LibraryResources.h>
#include "SegoeFluentIconList.h"
#include "../../types/inc/utils.hpp"
#include "../WinRTUtils/inc/Utils.h"

using namespace winrt;
using namespace winrt::Windows::UI;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Navigation;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::UI::Xaml::Media;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Microsoft::UI::Xaml::Controls;

namespace winrt
{
    namespace MUX = Microsoft::UI::Xaml;
    namespace WUX = Windows::UI::Xaml;
}

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    static constexpr std::wstring_view HideIconValue{ L"none" };

    Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> IconPicker::_BuiltInIcons{ nullptr };
    Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> IconPicker::_IconTypes{ nullptr };
    DependencyProperty IconPicker::_CurrentIconPathProperty{ nullptr };

    Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> IconPicker::BuiltInIcons() noexcept
    {
        if (!_BuiltInIcons)
        {
            // lazy load the built-in icons
            std::vector<Editor::EnumEntry> builtInIcons;
            for (auto& [val, name] : s_SegoeFluentIcons)
            {
                builtInIcons.emplace_back(make<EnumEntry>(hstring{ name }, box_value(val)));
            }
            _BuiltInIcons = single_threaded_observable_vector<Editor::EnumEntry>(std::move(builtInIcons));
        }
        return _BuiltInIcons;
    }

    Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> IconPicker::IconTypes() noexcept
    {
        if (!_IconTypes)
        {
            // lazy load the icon types
            std::vector<Editor::EnumEntry> iconTypes;
            iconTypes.reserve(4);
            iconTypes.emplace_back(make<EnumEntry>(RS_(L"IconPicker_IconTypeNone"), box_value(IconType::None)));
            iconTypes.emplace_back(make<EnumEntry>(RS_(L"IconPicker_IconTypeFontIcon"), box_value(IconType::FontIcon)));
            iconTypes.emplace_back(make<EnumEntry>(RS_(L"IconPicker_IconTypeEmoji"), box_value(IconType::Emoji)));
            iconTypes.emplace_back(make<EnumEntry>(RS_(L"IconPicker_IconTypeImage"), box_value(IconType::Image)));
            _IconTypes = winrt::single_threaded_observable_vector<Editor::EnumEntry>(std::move(iconTypes));
        }
        return _IconTypes;
    }

    IconPicker::IconPicker()
    {
        _InitializeProperties();
        InitializeComponent();

        _DeduceCurrentIconType();
        PropertyChanged([this](auto&&, const PropertyChangedEventArgs& args) {
            const auto propertyName{ args.PropertyName() };
            // "CurrentIconPath" changes are handled by _OnCurrentIconPathChanged()
            if (propertyName == L"CurrentIconType")
            {
                PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"UsingNoIcon" });
                PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"UsingBuiltInIcon" });
                PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"UsingEmojiIcon" });
                PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"UsingImageIcon" });
            }
            else if (propertyName == L"CurrentBuiltInIcon")
            {
                CurrentIconPath(unbox_value<hstring>(_CurrentBuiltInIcon.EnumValue()));
            }
            else if (propertyName == L"CurrentEmojiIcon")
            {
                CurrentIconPath(CurrentEmojiIcon());
            }
        });
    }

    void IconPicker::_InitializeProperties()
    {
        // Initialize any dependency properties here.
        // This performs a lazy load on these properties, instead of
        // initializing them when the DLL loads.
        if (!_CurrentIconPathProperty)
        {
            _CurrentIconPathProperty =
                DependencyProperty::Register(
                    L"CurrentIconPath",
                    xaml_typename<hstring>(),
                    xaml_typename<Editor::IconPicker>(),
                    PropertyMetadata{ nullptr, PropertyChangedCallback{ &IconPicker::_OnCurrentIconPathChanged } });
        }
    }

    void IconPicker::_OnCurrentIconPathChanged(const DependencyObject& d, const DependencyPropertyChangedEventArgs& /*e*/)
    {
        d.as<IconPicker>()->_DeduceCurrentIconType();
    }

    safe_void_coroutine IconPicker::Icon_Click(const IInspectable&, const RoutedEventArgs&)
    {
        auto lifetime = get_strong();

        const auto parentHwnd{ reinterpret_cast<HWND>(WindowRoot().GetHostingWindow()) };
        auto file = co_await OpenImagePicker(parentHwnd);
        if (!file.empty())
        {
            CurrentIconPath(file);
        }
    }

    void IconPicker::BuiltInIconPicker_GotFocus(const IInspectable& sender, const RoutedEventArgs& /*e*/)
    {
        _updateIconFilter({});
        sender.as<AutoSuggestBox>().IsSuggestionListOpen(true);
    }

    void IconPicker::BuiltInIconPicker_QuerySubmitted(const AutoSuggestBox& /*sender*/, const AutoSuggestBoxQuerySubmittedEventArgs& e)
    {
        const auto iconEntry = unbox_value_or<Editor::EnumEntry>(e.ChosenSuggestion(), nullptr);
        if (!iconEntry)
        {
            return;
        }
        CurrentBuiltInIcon(iconEntry);
    }

    void IconPicker::BuiltInIconPicker_TextChanged(const AutoSuggestBox& sender, const AutoSuggestBoxTextChangedEventArgs& e)
    {
        if (e.Reason() != AutoSuggestionBoxTextChangeReason::UserInput)
        {
            return;
        }
        std::wstring_view filter{ sender.Text() };
        filter = til::trim(filter, L' ');
        _updateIconFilter(filter);
    }

    void IconPicker::_updateIconFilter(std::wstring_view filter)
    {
        if (_iconFilter != filter)
        {
            _filteredBuiltInIcons = nullptr;
            _iconFilter = filter;
            _updateFilteredIconList();
            PropertyChanged.raise(*this, PropertyChangedEventArgs{ L"FilteredBuiltInIconList" });
        }
    }

    Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> IconPicker::FilteredBuiltInIconList()
    {
        if (!_filteredBuiltInIcons)
        {
            _updateFilteredIconList();
        }
        return _filteredBuiltInIcons;
    }

    void IconPicker::_updateFilteredIconList()
    {
        _filteredBuiltInIcons = BuiltInIcons();
        if (_iconFilter.empty())
        {
            return;
        }

        // Find matching icons and populate the filtered list
        std::vector<Editor::EnumEntry> filtered;
        filtered.reserve(_filteredBuiltInIcons.Size());
        for (const auto& icon : _filteredBuiltInIcons)
        {
            if (til::contains_linguistic_insensitive(icon.EnumName(), _iconFilter))
            {
                filtered.emplace_back(icon);
            }
        }
        _filteredBuiltInIcons = winrt::single_threaded_observable_vector(std::move(filtered));
    }

    void IconPicker::CurrentIconType(const Windows::Foundation::IInspectable& value)
    {
        if (_currentIconType != value)
        {
            // Switching from...
            if (_currentIconType && unbox_value<IconType>(_currentIconType.as<Editor::EnumEntry>().EnumValue()) == IconType::Image)
            {
                // Stash the current value of Icon. If the user
                // switches out of then back to IconType::Image, we want
                // the path that we display in the text box to remain unchanged.
                _lastIconPath = CurrentIconPath();
            }

            // Set the member here instead of after setting Icon() below!
            // We have an Icon property changed handler defined for when we discard changes.
            // Inadvertently, that means that we call this setter again.
            // Setting the member here means that we early exit at the beginning of the function
            //  because _currentIconType == value.
            _currentIconType = value;

            // Switched to...
            switch (unbox_value<IconType>(value.as<Editor::EnumEntry>().EnumValue()))
            {
            case IconType::None:
            {
                CurrentIconPath(winrt::hstring{ HideIconValue });
                break;
            }
            case IconType::Image:
            {
                if (!_lastIconPath.empty())
                {
                    // Conversely, if we switch to Image,
                    // retrieve that saved value and apply it
                    CurrentIconPath(_lastIconPath);
                }
                break;
            }
            case IconType::FontIcon:
            {
                if (_CurrentBuiltInIcon)
                {
                    CurrentIconPath(unbox_value<hstring>(_CurrentBuiltInIcon.EnumValue()));
                }
                break;
            }
            case IconType::Emoji:
            {
                // Don't set Icon here!
                // Clear out the text box so we direct the user to use the emoji picker.
                CurrentEmojiIcon({});
            }
            }
            // We're not using the VM's Icon() setter above,
            // so notify HasIcon changed manually
            PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"CurrentIconType" });
            PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"HasIcon" });
        }
    }

    bool IconPicker::UsingNoIcon() const
    {
        return _currentIconType == IconTypes().GetAt(0);
    }

    bool IconPicker::UsingBuiltInIcon() const
    {
        return _currentIconType == IconTypes().GetAt(1);
    }

    bool IconPicker::UsingEmojiIcon() const
    {
        return _currentIconType == IconTypes().GetAt(2);
    }

    bool IconPicker::UsingImageIcon() const
    {
        return _currentIconType == IconTypes().GetAt(3);
    }

    void IconPicker::_DeduceCurrentIconType()
    {
        const auto icon = CurrentIconPath();
        if (icon.empty() || icon == HideIconValue)
        {
            _currentIconType = IconTypes().GetAt(0);
        }
        else if (icon.size() == 1 && (L'\uE700' <= til::at(icon, 0) && til::at(icon, 0) <= L'\uF8B3'))
        {
            _currentIconType = IconTypes().GetAt(1);
            _DeduceCurrentBuiltInIcon();
        }
        else if (::Microsoft::Console::Utils::IsLikelyToBeEmojiOrSymbolIcon(icon))
        {
            // We already did a range check for MDL2 Assets in the previous one,
            // so if we're out of that range but still short, assume we're an emoji
            _currentIconType = IconTypes().GetAt(2);
        }
        else
        {
            _currentIconType = IconTypes().GetAt(3);
        }
        PropertyChanged.raise(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"CurrentIconType" });
    }

    void IconPicker::_DeduceCurrentBuiltInIcon()
    {
        const auto icon = CurrentIconPath();
        for (uint32_t i = 0; i < BuiltInIcons().Size(); i++)
        {
            const auto& builtIn = BuiltInIcons().GetAt(i);
            if (icon == unbox_value<hstring>(builtIn.EnumValue()))
            {
                CurrentBuiltInIcon(builtIn);
                return;
            }
        }
        CurrentBuiltInIcon(BuiltInIcons().GetAt(0));
    }

    WUX::Controls::IconSource IconPicker::BuiltInIconConverter(const IInspectable& iconVal)
    {
        return Microsoft::Terminal::UI::IconPathConverter::IconSourceWUX(unbox_value<hstring>(iconVal));
    }
}
