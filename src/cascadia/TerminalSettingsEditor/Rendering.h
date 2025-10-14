// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Rendering.g.h"
#include "NavigateToRenderingArgs.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NavigateToRenderingArgs : NavigateToRenderingArgsT<NavigateToRenderingArgs>
    {
        NavigateToRenderingArgs(const Editor::RenderingViewModel& vm, const hstring& elementToFocus = {}) :
            _ViewModel(vm),
            _ElementToFocus(elementToFocus) {}

        Editor::RenderingViewModel ViewModel() const noexcept { return _ViewModel; }
        hstring ElementToFocus() const noexcept { return _ElementToFocus; }

    private:
        Editor::RenderingViewModel _ViewModel{ nullptr };
        hstring _ElementToFocus{};
    };

    struct Rendering : public HasScrollViewer<Rendering>, RenderingT<Rendering>
    {
        Rendering();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::RenderingViewModel, ViewModel, PropertyChanged.raise, nullptr);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Rendering);
}
