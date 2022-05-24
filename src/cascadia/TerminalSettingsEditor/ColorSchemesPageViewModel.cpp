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

    void ColorSchemesPageViewModel::RequestSetCurrentScheme(Editor::ColorSchemeViewModel scheme)
    {
        CurrentScheme(scheme);
    }

    void ColorSchemesPageViewModel::_MakeColorSchemeVMsHelper()
    {
        Windows::Foundation::Collections::IObservableVector<Editor::ColorSchemeViewModel> AllColorSchemes{ single_threaded_observable_vector<Editor::ColorSchemeViewModel>() };
        const auto& colorSchemeMap{ _settings.GlobalSettings().ColorSchemes() };
        for (const auto& pair : colorSchemeMap)
        {
            const auto viewModel = Editor::ColorSchemeViewModel(pair.Value());
            viewModel.DeleteColorScheme({ this, &ColorSchemesPageViewModel::_DeleteColorScheme });
            AllColorSchemes.Append(viewModel);
        }
        _AllColorSchemes = AllColorSchemes;
    }

    void ColorSchemesPageViewModel::_DeleteColorScheme(const IInspectable /*sender*/, const Editor::DeleteColorSchemeEventArgs& args)
    {
        const auto name{ args.SchemeName() };

        // Delete scheme from settings model
        _settings.GlobalSettings().RemoveColorScheme(name);

        // This ensures that the JSON is updated with "Campbell", because the color scheme was deleted
        _settings.UpdateColorSchemeReferences(name, L"Campbell");
    }
}
