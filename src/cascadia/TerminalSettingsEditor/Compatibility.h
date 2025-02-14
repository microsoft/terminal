// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Compatibility.g.h"
#include "CompatibilityViewModel.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct CompatibilityViewModel : CompatibilityViewModelT<CompatibilityViewModel>, ViewModelHelper<CompatibilityViewModel>
    {
    public:
        CompatibilityViewModel(Model::GlobalAppSettings globalSettings);

        bool DebugFeaturesAvailable() const noexcept;

        // DON'T YOU DARE ADD A `WINRT_CALLBACK(PropertyChanged` TO A CLASS DERIVED FROM ViewModelHelper. Do this instead:
        using ViewModelHelper<CompatibilityViewModel>::PropertyChanged;

        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, AllowHeadless);
        PERMANENT_OBSERVABLE_PROJECTED_SETTING(_GlobalSettings, DebugFeaturesEnabled);
        GETSET_BINDABLE_ENUM_SETTING(TextMeasurement, winrt::Microsoft::Terminal::Control::TextMeasurement, _GlobalSettings.TextMeasurement);

    private:
        Model::GlobalAppSettings _GlobalSettings;
    };

    struct Compatibility : public HasScrollViewer<Compatibility>, CompatibilityT<Compatibility>
    {
        Compatibility();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::CompatibilityViewModel, ViewModel, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Compatibility);
    BASIC_FACTORY(CompatibilityViewModel);
}
