// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorTableEntry.g.h"
#include "ColorSchemes.g.h"
#include "ColorSchemesPageNavigationState.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ColorSchemesPageNavigationState : ColorSchemesPageNavigationStateT<ColorSchemesPageNavigationState>
    {
    public:
        ColorSchemesPageNavigationState(const Model::CascadiaSettings& settings) :
            _Settings{ settings } {}

        WINRT_PROPERTY(Model::CascadiaSettings, Settings, nullptr);
        WINRT_PROPERTY(winrt::hstring, LastSelectedScheme, L"");
    };

    struct ColorSchemes : public HasScrollViewer<ColorSchemes>, ColorSchemesT<ColorSchemes>
    {
        ColorSchemes();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void ColorSchemeSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void ColorPickerChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Microsoft::UI::Xaml::Controls::ColorChangedEventArgs const& args);
        void AddNew_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        void Rename_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void RenameAccept_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void RenameCancel_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void NameBox_PreviewKeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        bool CanDeleteCurrentScheme() const;
        void DeleteConfirmation_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        WINRT_PROPERTY(Editor::ColorSchemesPageNavigationState, State, nullptr);
        WINRT_PROPERTY(Model::ColorScheme, CurrentColorScheme, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, CurrentNonBrightColorTable, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IVector<Editor::ColorTableEntry>, CurrentBrightColorTable, nullptr);
        WINRT_PROPERTY(Windows::Foundation::Collections::IObservableVector<Model::ColorScheme>, ColorSchemeList, nullptr);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(bool, IsRenaming, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentForegroundColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentBackgroundColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentCursorColor, _PropertyChangedHandlers, nullptr);
        WINRT_OBSERVABLE_PROPERTY(Editor::ColorTableEntry, CurrentSelectionBackgroundColor, _PropertyChangedHandlers, nullptr);

    private:
        void _UpdateColorTable(const winrt::Microsoft::Terminal::Settings::Model::ColorScheme& colorScheme);
        void _UpdateColorSchemeList();
        void _RenameCurrentScheme(hstring newName);
    };

    struct ColorTableEntry : ColorTableEntryT<ColorTableEntry>
    {
    public:
        ColorTableEntry(uint8_t index, Windows::UI::Color color);
        ColorTableEntry(std::wstring_view tag, Windows::UI::Color color);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        WINRT_OBSERVABLE_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(IInspectable, Tag, _PropertyChangedHandlers);
        WINRT_OBSERVABLE_PROPERTY(Windows::UI::Color, Color, _PropertyChangedHandlers);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemes);
}
