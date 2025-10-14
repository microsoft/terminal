// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Launch.g.h"
#include "NavigateToLaunchArgs.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NavigateToLaunchArgs : NavigateToLaunchArgsT<NavigateToLaunchArgs>
    {
        NavigateToLaunchArgs(const Editor::LaunchViewModel& vm, const hstring& elementToFocus = {}) :
            _ViewModel(vm),
            _ElementToFocus(elementToFocus) {}

        Editor::LaunchViewModel ViewModel() const noexcept { return _ViewModel; }
        hstring ElementToFocus() const noexcept { return _ElementToFocus; }

    private:
        Editor::LaunchViewModel _ViewModel{ nullptr };
        hstring _ElementToFocus{};
    };

    struct Launch : public HasScrollViewer<Launch>, LaunchT<Launch>
    {
    public:
        Launch();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::LaunchViewModel, ViewModel, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}
