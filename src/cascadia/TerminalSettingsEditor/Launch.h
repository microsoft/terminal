// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#pragma once

#include "Launch.g.h"
#include "Utils.h"

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    struct Launch : public /*HasScrollViewer<Launch>,*/ LaunchT<Launch>
    {
    public:
        Launch();

        void OnNavigatedTo(const winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs& e);

        til::property_changed_event PropertyChanged;
        WINRT_OBSERVABLE_PROPERTY(Editor::LaunchViewModel, ViewModel, PropertyChanged.raise, nullptr);
        void ViewChanging(const winrt::Windows::Foundation::IInspectable& sender,
                          const winrt::Windows::UI::Xaml::Controls::ScrollViewerViewChangingEventArgs& /*e*/);
    };
}

namespace winrt::Microsoft::Terminal::Settings::Editor::factory_implementation
{
    BASIC_FACTORY(Launch);
}
