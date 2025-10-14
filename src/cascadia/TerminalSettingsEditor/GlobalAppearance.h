// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "GlobalAppearance.g.h"
#include "NavigateToGlobalAppearanceArgs.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NavigateToGlobalAppearanceArgs : NavigateToGlobalAppearanceArgsT<NavigateToGlobalAppearanceArgs>
    {
        NavigateToGlobalAppearanceArgs(const Editor::GlobalAppearanceViewModel& vm, const hstring& elementToFocus = {}) :
            _ViewModel(vm),
            _ElementToFocus(elementToFocus) {}

        Editor::GlobalAppearanceViewModel ViewModel() const noexcept { return _ViewModel; }
        hstring ElementToFocus() const noexcept { return _ElementToFocus; }

    private:
        Editor::GlobalAppearanceViewModel _ViewModel{ nullptr };
        hstring _ElementToFocus{};
    };

    struct GlobalAppearance : public HasScrollViewer<GlobalAppearance>, GlobalAppearanceT<GlobalAppearance>
    {
    public:
        GlobalAppearance();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::GlobalAppearanceViewModel, ViewModel, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(GlobalAppearance);
}
