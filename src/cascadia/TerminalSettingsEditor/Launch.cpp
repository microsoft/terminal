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

    GlobalAppSettings Launch::GlobalSettings()
    {
        return MainPage::Settings().GlobalSettings();
    }

    IInspectable Launch::CurrentDefaultProfile()
    {
        return winrt::box_value(MainPage::Settings().FindProfile(GlobalSettings().DefaultProfile()));
    }

    void Launch::CurrentDefaultProfile(const IInspectable& value)
    {
        const auto profile{ winrt::unbox_value<Model::Profile>(value) };
        GlobalSettings().DefaultProfile(profile.Guid());
    }
}
