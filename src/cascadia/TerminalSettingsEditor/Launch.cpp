// Copyright (c) Microsoft Corporation.
// Licensed under the MIT license.

#include "pch.h"
#include "Launch.h"
#include "Launch.g.cpp"
#include "MainPage.h"

using namespace winrt;
using namespace winrt::Windows::UI::Xaml;
using namespace winrt::Windows::UI::Xaml::Controls;
using namespace winrt::Windows::Foundation;
using namespace winrt::Microsoft::Terminal::Settings::Model;

namespace winrt::Microsoft::Terminal::Settings::Editor::implementation
{
    Launch::Launch()
    {
        InitializeComponent();
    }

    void Launch::OnNavigatedTo(winrt::Windows::UI::Xaml::Navigation::NavigationEventArgs e)
    {
        _Settings = e.Parameter().as<Model::CascadiaSettings>();
    }

    IInspectable Launch::CurrentDefaultProfile()
    {
        return winrt::box_value(_Settings.FindProfile(_Settings.GlobalSettings().DefaultProfile()));
    }

    void Launch::CurrentDefaultProfile(const IInspectable& value)
    {
        const auto profile{ winrt::unbox_value<Model::Profile>(value) };
        _Settings.GlobalSettings().DefaultProfile(profile.Guid());
    }
}
