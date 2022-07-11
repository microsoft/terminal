// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorSchemeViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "ColorSchemes.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    inline static constexpr uint8_t ColorTableDivider{ 8 };
    inline static constexpr uint8_t ColorTableSize{ 16 };

    inline static constexpr std::wstring_view ForegroundColorTag{ L"Foreground" };
    inline static constexpr std::wstring_view BackgroundColorTag{ L"Background" };
    inline static constexpr std::wstring_view CursorColorTag{ L"CursorColor" };
    inline static constexpr std::wstring_view SelectionBackgroundColorTag{ L"SelectionBackground" };

    struct ColorSchemeViewModel : ColorSchemeViewModelT<ColorSchemeViewModel>, ViewModelHelper<ColorSchemeViewModel>
    {
    public:
        ColorSchemeViewModel(const Model::ColorScheme scheme, const Editor::ColorSchemesPageViewModel parentPageVM);

        winrt::hstring Name();
        void Name(winrt::hstring newName);

        bool RequestRename(winrt::hstring newName);

        Editor::ColorTableEntry ColorEntryAt(uint32_t index);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_PROPERTY(bool, IsInBoxScheme);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, NonBrightColorTable, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, BrightColorTable, nullptr);

        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, ForegroundColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, BackgroundColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CursorColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, SelectionBackgroundColor, _PropertyChangedHandlers, nullptr);

    private:
        winrt::hstring _Name;
        Model::ColorScheme _scheme;
        Editor::ColorSchemesPageViewModel _parentPageVM{ nullptr };
    };

    struct ColorTableEntry : ColorTableEntryT<ColorTableEntry>
    {
    public:
        ColorTableEntry(uint8_t index, Windows::UI::Color color);
        ColorTableEntry(std::wstring_view tag, Windows::UI::Color color);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(Windows::UI::Color, Color, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(IInspectable, Tag, _PropertyChangedHandlers);

    private:
        Windows::UI::Color _color;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemeViewModel);
}
