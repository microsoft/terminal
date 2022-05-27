// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemesPageViewModel.h"
#include "ColorSchemesPageViewModel.g.cpp"

#include <LibraryResources.h>
#include "..\WinRTUtils\inc\Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ColorSchemesPageViewModel::ColorSchemesPageViewModel(const Model::CascadiaSettings& settings) :
        _settings{ settings }
    {
        _MakeColorSchemeVMsHelper();
        CurrentScheme(_AllColorSchemes.GetAt(0));
    }

    void ColorSchemesPageViewModel::UpdateSettings(const Model::CascadiaSettings& settings)
    {
        _settings = settings;
        _MakeColorSchemeVMsHelper();
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
            _settings.GlobalSettings().RemoveColorScheme(oldName);
            _settings.GlobalSettings().AddColorScheme(CurrentScheme().SettingsModelObject());
            _settings.UpdateColorSchemeReferences(oldName, newName);
            return true;
        }
    }

    void ColorSchemesPageViewModel::RequestDeleteCurrentScheme()
    {
        const auto name{ CurrentScheme().Name() };

        for (uint32_t i = 0; i < _AllColorSchemes.Size(); ++i)
        {
            if (_AllColorSchemes.GetAt(i).Name() == name)
            {
                _AllColorSchemes.RemoveAt(i);
                break;
            }
        }

        // Delete scheme from settings model
        _settings.GlobalSettings().RemoveColorScheme(name);

        // This ensures that the JSON is updated with "Campbell", because the color scheme was deleted
        _settings.UpdateColorSchemeReferences(name, L"Campbell");
    }

    Editor::ColorSchemeViewModel ColorSchemesPageViewModel::RequestAddNew()
    {
        const hstring schemeName{ fmt::format(L"Color Scheme {}", _settings.GlobalSettings().ColorSchemes().Size() + 1) };
        Model::ColorScheme scheme{ schemeName };

        // Add the new color scheme
        _settings.GlobalSettings().AddColorScheme(scheme);

        // Construct the new color scheme VM
        const Editor::ColorSchemeViewModel schemeVM{ scheme };
        _AllColorSchemes.Append(schemeVM);
        return schemeVM;
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
