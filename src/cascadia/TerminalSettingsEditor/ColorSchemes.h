// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "ColorTableEntry.g.h"
#include "ColorSchemes.g.h"
#include "ColorSchemesPageNavigationState.g.h"
#include "ModifyColorSchemeNameEventArgs.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct ModifyColorSchemeNameEventArgs :
        public ModifyColorSchemeNameEventArgsT<ModifyColorSchemeNameEventArgs>
    {
    public:
        ModifyColorSchemeNameEventArgs(hstring oldName, hstring newName) :
            _OldName{ oldName },
            _NewName{ newName } {}

        hstring OldName() const noexcept { return _OldName; }
        hstring NewName() const noexcept { return _NewName; }

    private:
        hstring _OldName;
        hstring _NewName;
    };

    struct ColorSchemesPageNavigationState : ColorSchemesPageNavigationStateT<ColorSchemesPageNavigationState>
    {
    public:
        ColorSchemesPageNavigationState(const Model::GlobalAppSettings& settings) :
            _Globals{ settings } {}

        void RenameColorScheme(hstring oldName, hstring newName);
        void DeleteColorScheme(hstring name);

        TYPED_EVENT(ModifyColorSchemeName, Editor::ColorSchemesPageNavigationState, Editor::ModifyColorSchemeNameEventArgs);
        GETSET_PROPERTY(Model::GlobalAppSettings, Globals, nullptr);
        GETSET_PROPERTY(winrt::hstring, LastSelectedScheme, L"");
    };

    struct ColorSchemes : ColorSchemesT<ColorSchemes>
    {
        ColorSchemes();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        void ColorSchemeSelectionChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::SelectionChangedEventArgs const& args);
        void ColorPickerChanged(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Controls::ColorChangedEventArgs const& args);
        void AddNew_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        void Rename_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void RenameAccept_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void RenameCancel_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);
        void NameBox_PreviewKeyDown(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::Input::KeyRoutedEventArgs const& e);

        bool CanDeleteCurrentScheme() const;
        void DeleteConfirmation_Click(winrt::Windows::Foundation::IInspectable const& sender, winrt::Windows::UI::Xaml::RoutedEventArgs const& e);

        GETSET_PROPERTY(Editor::ColorSchemesPageNavigationState, State, nullptr);
        GETSET_PROPERTY(Windows::Foundation::Collections::IObservableVector<winrt::Microsoft::Terminal::Settings::Editor::ColorTableEntry>, CurrentColorTable, nullptr);
        GETSET_PROPERTY(Windows::Foundation::Collections::IObservableVector<Model::ColorScheme>, ColorSchemeList, nullptr);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::Microsoft::Terminal::Settings::Model::ColorScheme, CurrentColorScheme, _PropertyChangedHandlers, nullptr);
        OBSERVABLE_GETSET_PROPERTY(bool, IsRenaming, _PropertyChangedHandlers, nullptr);

    private:
        void _UpdateColorTable(const winrt::Microsoft::Terminal::Settings::Model::ColorScheme& colorScheme);
        void _UpdateColorSchemeList();
        void _RenameCurrentScheme(hstring newName);
    };

    struct ColorTableEntry : ColorTableEntryT<ColorTableEntry>
    {
    public:
        ColorTableEntry(uint8_t index, Windows::UI::Color color);

        WINRT_CALLBACK(PropertyChanged, Windows::UI::Xaml::Data::PropertyChangedEventHandler);
        OBSERVABLE_GETSET_PROPERTY(winrt::hstring, Name, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(IInspectable, Index, _PropertyChangedHandlers);
        OBSERVABLE_GETSET_PROPERTY(Windows::UI::Color, Color, _PropertyChangedHandlers);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(ColorSchemes);
}
