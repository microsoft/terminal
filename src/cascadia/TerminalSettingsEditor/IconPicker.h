// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "IconPicker.g.h"
#include "EnumEntry.h"
#include "Utils.h"
#include "cppwinrt_utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct IconPicker : public HasScrollViewer<IconPicker>, IconPickerT<IconPicker>
    {
    public:
        IconPicker();

        static constexpr std::wstring_view HideIconValue{ L"none" };
        static Windows::UI::Xaml::Controls::IconSource BuiltInIconConverter(const Windows::Foundation::IInspectable& iconVal);
        static Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> BuiltInIcons() noexcept;
        static Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> IconTypes() noexcept;

        Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> FilteredBuiltInIconList();
        safe_void_coroutine Icon_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void BuiltInIconPicker_GotFocus(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);
        void BuiltInIconPicker_TextChanged(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const Windows::UI::Xaml::Controls::AutoSuggestBoxTextChangedEventArgs& e);
        void BuiltInIconPicker_QuerySubmitted(const winrt::Windows::UI::Xaml::Controls::AutoSuggestBox& sender, const Windows::UI::Xaml::Controls::AutoSuggestBoxQuerySubmittedEventArgs& e);

        Windows::Foundation::IInspectable CurrentIconType() const noexcept { return _currentIconType; }
        void CurrentIconType(const Windows::Foundation::IInspectable& value);

        Editor::IHostedInWindow WindowRoot() const noexcept { return _weakWindowRoot ? _weakWindowRoot.get() : nullptr; }
        void WindowRoot(const Editor::IHostedInWindow& value) noexcept { _weakWindowRoot = value; }

        bool UsingNoIcon() const;
        bool UsingBuiltInIcon() const;
        bool UsingEmojiIcon() const;
        bool UsingImageIcon() const;

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(hstring, CurrentEmojiIcon, PropertyChanged.raise);
        WINRT_OBSERVABLE_PROPERTY(Editor::EnumEntry, CurrentBuiltInIcon, PropertyChanged.raise, nullptr);

        DEPENDENCY_PROPERTY(hstring, CurrentIconPath);

    private:
        static Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> _BuiltInIcons;
        static Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> _IconTypes;
        Windows::Foundation::Collections::IObservableVector<Editor::EnumEntry> _filteredBuiltInIcons;
        std::wstring _iconFilter;
        Windows::Foundation::IInspectable _currentIconType{};
        winrt::hstring _lastIconPath;
        winrt::weak_ref<Editor::IHostedInWindow> _weakWindowRoot;

        static void _InitializeProperties();
        static void _OnCurrentIconPathChanged(const Windows::UI::Xaml::DependencyObject& d, const Windows::UI::Xaml::DependencyPropertyChangedEventArgs& e);

        void _DeduceCurrentIconType();
        void _DeduceCurrentBuiltInIcon();
        void _updateIconFilter(std::wstring_view filter);
        void _updateFilteredIconList();
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(IconPicker);
}
