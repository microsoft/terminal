// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemesPageViewModel.h"
#include "ColorSchemesPageViewModel.g.cpp"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
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

    ColorSchemesPageViewModel::ColorSchemesPageViewModel(const Model::CascadiaSettings& settings) :
        _settings{ settings }
    {
        _MakeColorSchemeVMsHelper();
        CurrentScheme(_AllColorSchemes.GetAt(0));
    }

    void ColorSchemesPageViewModel::RequestSetCurrentScheme(Editor::ColorSchemeViewModel scheme)
    {
        CurrentScheme(scheme);

        // Update the state of the page
        _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"CanDeleteCurrentScheme" });
    }

    bool ColorSchemesPageViewModel::RequestRenameCurrentScheme(winrt::hstring newName)
    {
        // check if different name is already in use
        const auto oldName{ CurrentScheme().Name() };
        if (newName != oldName && _settings.GlobalSettings().ColorSchemes().HasKey(newName))
        {
            return false;
        }
        else
        {
            // update the settings model
            CurrentScheme().Name(newName);
            _settings.UpdateColorSchemeReferences(oldName, newName);
            return true;
        }
    }

    void ColorSchemesPageViewModel::RequestDeleteCurrentScheme()
    {
        const auto name{ CurrentScheme().Name() };

        // Delete scheme from settings model
        _settings.GlobalSettings().RemoveColorScheme(name);

        // This ensures that the JSON is updated with "Campbell", because the color scheme was deleted
        _settings.UpdateColorSchemeReferences(name, L"Campbell");
    }

    bool ColorSchemesPageViewModel::CanDeleteCurrentScheme() const
    {
        if (const auto& scheme{ CurrentScheme() })
        {
            // Only allow this color scheme to be deleted if it's not provided in-box
            const std::wstring myName{ scheme.Name() };
            return std::find(std::begin(InBoxSchemes), std::end(InBoxSchemes), myName) == std::end(InBoxSchemes);
        }
        return false;
    }

    void ColorSchemesPageViewModel::_MakeColorSchemeVMsHelper()
    {
        Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel> AllColorSchemes{ single_threaded_observable_vector<Editor::ColorSchemeViewModel>() };
        const auto& colorSchemeMap{ _settings.GlobalSettings().ColorSchemes() };
        for (const auto& pair : colorSchemeMap)
        {
            const auto viewModel = Editor::ColorSchemeViewModel(pair.Value());
            AllColorSchemes.Append(viewModel);
        }
        _AllColorSchemes = AllColorSchemes;
    }
}
