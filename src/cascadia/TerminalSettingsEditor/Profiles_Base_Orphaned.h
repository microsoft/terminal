// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Profiles_Base_Orphaned.g.h"
#include "ViewModelHelpers.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Profiles_Base_Orphaned : public HasScrollViewer<Profiles_Base_Orphaned>, Profiles_Base_OrphanedT<Profiles_Base_Orphaned>
    {
    public:
        Profiles_Base_Orphaned();

        void OnNavigatedTo(const Windows::UI::Xaml::Navigation::NavigationEventArgs& e);
        void DeleteConfirmation_Click(const Windows::Foundation::IInspectable& sender, const Windows::UI::Xaml::RoutedEventArgs& e);

        WINRT_PROPERTY(Editor::ProfileViewModel, Profile, nullptr);

    private:
        winrt::Windows::UI::Xaml::Controls::SwapChainPanel::LayoutUpdated_revoker _layoutUpdatedRevoker;
    };
};

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Profiles_Base_Orphaned);
}
