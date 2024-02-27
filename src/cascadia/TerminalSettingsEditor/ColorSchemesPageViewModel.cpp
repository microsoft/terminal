// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "ColorSchemesPageViewModel.h"
#include "ColorSchemesPageViewModel.g.cpp"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    ColorSchemesPageViewModel::ColorSchemesPageViewModel(const Model::CascadiaSettings& settings) :
        _settings{ settings },
        _viewModelToSchemeMap{ winrt::single_threaded_map<Editor::ColorSchemeViewModel, Model::ColorScheme>() }
    {
        _MakeColorSchemeVMsHelper();
    }

    void ColorSchemesPageViewModel::UpdateSettings(const Model::CascadiaSettings& settings)
    {
        _settings = settings;

        // We want to re-initialize our AllColorSchemes list, but we want to make sure
        // we still have the same CurrentScheme as before (if that scheme still exists)

        // Store the name of the current scheme
        const auto currentSchemeName = HasCurrentScheme() ? CurrentScheme().Name() : hstring{};

        // Re-initialize the color scheme list
        _MakeColorSchemeVMsHelper();

        // Re-select the previously selected scheme if it exists
        if (!currentSchemeName.empty())
        {
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
                // we didn't find the previously selected scheme
                CurrentScheme(nullptr);
            }
        }
        else
        {
            // didn't have a scheme,
            // so skip over looking through the schemes
            CurrentScheme(nullptr);
        }
    }

    void ColorSchemesPageViewModel::CurrentScheme(const Editor::ColorSchemeViewModel& newSelectedScheme)
    {
        if (_CurrentScheme != newSelectedScheme)
        {
            _CurrentScheme = newSelectedScheme;
            _NotifyChanges(L"CurrentScheme", L"CanDeleteCurrentScheme", L"HasCurrentScheme");
        }
    }

    Editor::ColorSchemeViewModel ColorSchemesPageViewModel::CurrentScheme()
    {
        return _CurrentScheme;
    }

    bool ColorSchemesPageViewModel::HasCurrentScheme() const noexcept
    {
        return _CurrentScheme != nullptr;
    }

    void ColorSchemesPageViewModel::_MakeColorSchemeVMsHelper()
    {
        std::vector<Editor::ColorSchemeViewModel> allColorSchemes;

        const auto colorSchemeMap = _settings.GlobalSettings().ColorSchemes();
        for (const auto& pair : colorSchemeMap)
        {
            const auto scheme = pair.Value();
            auto viewModel{ winrt::make<ColorSchemeViewModel>(scheme, *this, _settings) };
            allColorSchemes.emplace_back(viewModel);

            // We will need access to the settings model object later, but we don't
            // want to expose it on the color scheme VM, so we store the reference to it
            // in our internal map
            _viewModelToSchemeMap.Insert(viewModel, scheme);
        }

        _AllColorSchemes = single_threaded_observable_vector<Editor::ColorSchemeViewModel>(std::move(allColorSchemes));
        _NotifyChanges(L"AllColorSchemes");
    }

    Editor::ColorSchemeViewModel ColorSchemesPageViewModel::RequestAddNew()
    {
        const hstring schemeName{ fmt::format(L"Color Scheme {}", _settings.GlobalSettings().ColorSchemes().Size() + 1) };
        Model::ColorScheme scheme{ schemeName };

        // Add the new color scheme
        _settings.GlobalSettings().AddColorScheme(scheme);

        // Construct the new color scheme VM
        const auto schemeVM{ winrt::make<ColorSchemeViewModel>(scheme, *this, _settings) };
        _AllColorSchemes.Append(schemeVM);
        _viewModelToSchemeMap.Insert(schemeVM, scheme);
        return schemeVM;
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
            _NotifyChanges(L"CurrentSchemeName");
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
                if (i < _AllColorSchemes.Size())
                {
                    // select same index
                    CurrentScheme(_AllColorSchemes.GetAt(i));
                }
                else
                {
                    // select last color scheme (avoid out of bounds error)
                    CurrentScheme(_AllColorSchemes.GetAt(i - 1));
                }
                break;
            }
        }

        // Delete scheme from settings model
        _settings.GlobalSettings().RemoveColorScheme(name);

        // This ensures that the JSON is updated with "Campbell", because the color scheme was deleted
        _settings.UpdateColorSchemeReferences(name, L"Campbell");

        // If we're not on this page, switch back to this page
        if (CurrentPage() != ColorSchemesSubPage::Base)
        {
            CurrentPage(ColorSchemesSubPage::Base);
        }
    }

    void ColorSchemesPageViewModel::RequestEditSelectedScheme()
    {
        if (_CurrentScheme)
        {
            CurrentPage(ColorSchemesSubPage::EditColorScheme);
        }
    }

    void ColorSchemesPageViewModel::RequestSetSelectedSchemeAsDefault()
    {
        if (_CurrentScheme)
        {
            _settings.ProfileDefaults().DefaultAppearance().LightColorSchemeName(_CurrentScheme.Name());
            _settings.ProfileDefaults().DefaultAppearance().DarkColorSchemeName(_CurrentScheme.Name());
            for (const auto scheme : _AllColorSchemes)
            {
                auto schemeImpl{ get_self<ColorSchemeViewModel>(scheme) };
                schemeImpl->RefreshIsDefault();
            }
        }
    }

    void ColorSchemesPageViewModel::RequestDuplicateCurrentScheme()
    {
        if (_CurrentScheme)
        {
            if (auto actualCurrentScheme = _viewModelToSchemeMap.TryLookup(_CurrentScheme))
            {
                auto scheme = _settings.GlobalSettings().DuplicateColorScheme(actualCurrentScheme);
                // Construct the new color scheme VM
                const auto schemeVM{ winrt::make<ColorSchemeViewModel>(scheme, *this, _settings) };
                _AllColorSchemes.Append(schemeVM);
                _viewModelToSchemeMap.Insert(schemeVM, scheme);
                CurrentScheme(schemeVM);
                CurrentPage(ColorSchemesSubPage::Base);
                RequestEditSelectedScheme();
            }
        }
    }

    bool ColorSchemesPageViewModel::CanDeleteCurrentScheme() const
    {
        if (_CurrentScheme)
        {
            return _CurrentScheme.IsEditable();
        }
        return false;
    }

    void ColorSchemesPageViewModel::SchemeListItemClicked(const IInspectable& /*sender*/, const winrt::Windows::UI::Xaml::Controls::ItemClickEventArgs& e)
    {
        if (const auto item = e.ClickedItem())
        {
            CurrentScheme(item.try_as<Editor::ColorSchemeViewModel>());
            RequestEditSelectedScheme();
        }
    }
}
