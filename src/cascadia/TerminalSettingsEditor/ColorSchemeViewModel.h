// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorSchemeViewModel.g.h"
#include "Utils.h"
#include "ViewModelHelpers.h"
#include "ColorSchemes.h"

static constexpr uint8_t ColorTableDivider{ 8 };
static constexpr uint8_t ColorTableSize{ 16 };

static constexpr std::wstring_view ForegroundColorTag{ L"Foreground" };
static constexpr std::wstring_view BackgroundColorTag{ L"Background" };
static constexpr std::wstring_view CursorColorTag{ L"CursorColor" };
static constexpr std::wstring_view SelectionBackgroundColorTag{ L"SelectionBackground" };

static const std::array<std::wstring, 9> InBoxSchemes = {
    L"Campbell",
    L"Campbell Powershell",
    L"Vintage",
    L"One Half Dark",
    L"One Half Light",
    L"Solarized Dark",
    L"Solarized Light",
    L"Tango Dark",
    L"Tango Light"
};

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemeViewModel : ColorSchemeViewModelT<ColorSchemeViewModel>, ViewModelHelper<ColorSchemeViewModel>
    {
    public:
        ColorSchemeViewModel(const Model::ColorScheme scheme);

        winrt::hstring Name();
        void Name(winrt::hstring newName);

        Editor::ColorTableEntry ColorEntryAt(uint32_t index);

        Model::ColorScheme SettingsModelObject();

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);

        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, CurrentNonBrightColorTable, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, CurrentBrightColorTable, nullptr);

        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentForegroundColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentBackgroundColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentCursorColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentSelectionBackgroundColor, _PropertyChangedHandlers, nullptr);

    private:
        winrt::hstring _Name;
        Model::ColorScheme _scheme;
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
