// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemesPageViewModel.h"
#include "ColorSchemesPageViewModel.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    inline static constexpr std::array<std::wstring_view, 9> InBoxSchemes = {
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
        _settings{ settings },
        _viewModelToSchemeMap{ winrt::single_threaded_map<Editor::ColorSchemeViewModel, Model::ColorScheme>() }
    {
        _MakeColorSchemeVMsHelper();
        CurrentScheme(_AllColorSchemes.GetAt(0));
    }

    void ColorSchemesPageViewModel::UpdateSettings(const Model::CascadiaSettings& settings)
    {
        _settings = settings;

        // We want to re-initialize our AllColorSchemes list, but we want to make sure
        // we still have the same CurrentScheme as before (if that scheme still exists)

        // Store the name of the current scheme
        const auto currentSchemeName = CurrentScheme().Name();

        // Re-initialize the color scheme list
        _MakeColorSchemeVMsHelper();

        // Re-select the previously selected scheme if it exists
        const auto it = _AllColorSchemes.First();
        while (it.HasCurrent())
        {
            auto scheme = *it;
            if (scheme.Name() == currentSchemeName)
            {
                CurrentScheme(scheme);
                break;
            }
            it.MoveNext();
        }
        if (!it.HasCurrent())
        {
            // we didn't find the previously selected scheme, just select the first one
            CurrentScheme(_AllColorSchemes.GetAt(0));
        }
    }

    void ColorSchemesPageViewModel::_MakeColorSchemeVMsHelper()
    {
        std::vector<Editor::ColorSchemeViewModel> allColorSchemes;

        const auto colorSchemeMap = _settings.GlobalSettings().ColorSchemes();
        for (const auto& pair : colorSchemeMap)
        {
            const auto scheme = pair.Value();
            Editor::ColorSchemeViewModel viewModel{ scheme, *this };
            viewModel.IsInBoxScheme(std::find(std::begin(InBoxSchemes), std::end(InBoxSchemes), scheme.Name()) != std::end(InBoxSchemes));
            allColorSchemes.emplace_back(viewModel);

            // We will need access to the settings model object later, but we don't
            // want to expose it on the color scheme VM, so we store the reference to it
            // in our internal map
            _viewModelToSchemeMap.Insert(viewModel, scheme);
        }

        _AllColorSchemes = single_threaded_observable_vector<Editor::ColorSchemeViewModel>(std::move(allColorSchemes));
    }

    void ColorSchemesPageViewModel::RequestSetCurrentScheme(Editor::ColorSchemeViewModel scheme)
    {
        CurrentScheme(scheme);

        // Update the state of the page
        _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"CanDeleteCurrentScheme" });
    }

    bool ColorSchemesPageViewModel::RequestRenameCurrentScheme(hstring newName)
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
            _settings.GlobalSettings().AddColorScheme(_viewModelToSchemeMap.Lookup(CurrentScheme()));
            _settings.UpdateColorSchemeReferences(oldName, newName);

            // We need to let MainPage know so the BreadcrumbBarItem can be updated
            _PropertyChangedHandlers(*this, Windows::UI::Xaml::Data::PropertyChangedEventArgs{ L"CurrentSchemeName" });
            return true;
        }
    }

    void ColorSchemesPageViewModel::RequestDeleteCurrentScheme()
    {
        const auto name{ CurrentScheme().Name() };
        const auto size{ _AllColorSchemes.Size() };

        for (uint32_t i = 0; i < size; ++i)
        {
            if (_AllColorSchemes.GetAt(i).Name() == name)
            {
                _viewModelToSchemeMap.Remove(_AllColorSchemes.GetAt(i));
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
        const Editor::ColorSchemeViewModel schemeVM{ scheme, *this };
        _AllColorSchemes.Append(schemeVM);
        _viewModelToSchemeMap.Insert(schemeVM, scheme);
        return schemeVM;
    }

    void ColorSchemesPageViewModel::RequestSetCurrentPage(ColorSchemesSubPage subPage)
    {
        CurrentPage(subPage);
    }

    bool ColorSchemesPageViewModel::CanDeleteCurrentScheme() const
    {
        if (const auto& scheme{ CurrentScheme() })
        {
            // Only allow this color scheme to be deleted if it's not provided in-box
            return !scheme.IsInBoxScheme();
        }
        return false;
    }
}
