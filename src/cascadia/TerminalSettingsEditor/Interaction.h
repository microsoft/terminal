// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Interaction.g.h"
#include "NavigateToInteractionArgs.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct NavigateToInteractionArgs : NavigateToInteractionArgsT<NavigateToInteractionArgs>
    {
        NavigateToInteractionArgs(const Editor::InteractionViewModel& vm, const hstring& elementToFocus = {}) :
            _ViewModel(vm),
            _ElementToFocus(elementToFocus) {}

        Editor::InteractionViewModel ViewModel() const noexcept { return _ViewModel; }
        hstring ElementToFocus() const noexcept { return _ElementToFocus; }

    private:
        Editor::InteractionViewModel _ViewModel{ nullptr };
        hstring _ElementToFocus{};
    };

    struct Interaction : public HasScrollViewer<Interaction>, InteractionT<Interaction>
    {
        Interaction();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::InteractionViewModel, ViewModel, PropertyChanged.raise, nullptr);
        GETSET_BINDABLE_ENUM_SETTING(WarnAboutMultiLinePaste, winrt::Microsoft::Terminal::Control::WarnAboutMultiLinePaste, ViewModel().WarnAboutMultiLinePaste);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Interaction);
}
